#include "m_pd.h"

#include "chuck.h"
#include "chuck_globals.h"
#include "chuck_dl.h"

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <list>
#include <string>
#include <unordered_map>
#include <vector>

#include <libgen.h>
#include <unistd.h>
#ifdef __APPLE__
#include <sys/wait.h>
#endif

// clang-format off

// globals defs
#define LOG_LEVEL CK_LOG_SYSTEM // chuck log levels 0-10 (default: 2)
#define MAX_TAP_CHANNELS 16

// max simultaneous chuck~ objects that can receive routed callbacks
#define CK_MAX_INSTANCES 128
// depth of the audio-thread -> scheduler-thread reply queue, per instance
#define CK_REPLY_QUEUE_SIZE 256
// max atoms carried by a single reply
#define CK_REPLY_MAX_ATOMS 64
// max in-flight 'get' requests whose variable name is remembered, per instance
#define CK_PENDING_SIZE 256
// max concurrent event listeners, per instance
#define CK_MAX_LISTENS 32
// how often the reply queue is drained, in ms, while replies are enabled
#define CK_REPLY_POLL_MS 20.0

// forward decl so the instance slot table can refer to it
typedef struct _ck t_ck;

// chuck's globals callbacks carry a t_CKINT id and nothing else, so routing a
// reply back to the object that asked for it means encoding the destination in
// that id. slot indexes this table; the table is only mutated on the scheduler
// thread (object creation / destruction) and only read on the audio thread, so
// atomic pointer slots are enough to make the lookup safe without locking
static std::atomic<t_ck*> CK_INSTANCE_SLOTS[CK_MAX_INSTANCES];

// a reply queued from the audio thread, drained on the scheduler thread
typedef struct _ck_reply {
    t_symbol* selector;
    int argc;
    t_atom argv[CK_REPLY_MAX_ATOMS];
} t_ck_reply;

// pack / unpack a globals callback id: high 32 bits select the instance slot,
// low 32 bits are a per-instance ticket identifying the request
#define CK_ID_PACK(slot, ticket) \
    (((t_CKINT)(slot) << 32) | ((t_CKINT)(ticket) & 0xFFFFFFFFLL))
#define CK_ID_SLOT(id)   ((long)(((t_CKINT)(id)) >> 32))
#define CK_ID_TICKET(id) ((long)(((t_CKINT)(id)) & 0xFFFFFFFFLL))

// object struct
struct _ck {
    t_object obj;
    t_float x_f;

    int oid;                    // object id
    int srate;                  // sample rate
    int verbose;                // pd-side reporting verbosity (loglevel is
                                // process-global; see ck_loglevel)

    // chuck
    int buffer_size;            // buffer size for for both input and output
    float *in_chuck_buffer;     // intermediate chuck input buffer
    float *out_chuck_buffer;    // intermediate chuck output buffer
    t_symbol *patcher_dir;      // directory containing the current patch
    t_symbol *external_dir;     // directory containing this external
    t_symbol *examples_dir;     // directory containing the `examples` dir
    t_symbol *chugins_dir;      // directory containing chugins (compiled chuck plugins)
    t_symbol* editor;           // external text editor for chuck code
    t_symbol* edit_file;        // path of file to edit by external editor
    t_symbol *filename;         // last chuck file run
    long current_shred_id;      // current shred ID
    ChucK *chuck;               // chuck instance

    // switches
    int run_needs_audio;

    // configurable channels (replaces fixed N_IN/OUT_CHANNELS)
    int channels;               // number of I/O channels (default: 2)
    t_sample** in_vectors;      // input vector pointers (set in ck_dsp)
    t_sample** out_vectors;     // output vector pointers (main + tap)

    // tap infrastructure (for reading global UGen samples)
    int tap_channels;           // number of tap outlet channels (0 = disabled)
    t_symbol* tap_ugens[MAX_TAP_CHANNELS]; // names of global UGens to tap (one per outlet)
    int tap_ugen_nchans[MAX_TAP_CHANNELS]; // channel count of the UGen feeding this outlet (0 = mono)
    int tap_ugen_chan[MAX_TAP_CHANNELS];   // which channel of that UGen this outlet carries
    float* tap_buffer;          // buffer for tapped samples
    int tap_buffer_frames;      // frames allocated per channel in tap_buffer

    // dynamic inlets/outlets
    t_inlet** extra_inlets;     // extra signal inlets beyond the default one
    t_outlet** signal_outlets;  // all signal outlets (main + tap)

    // reply-related (routing chuck callbacks back out to the patch).
    // opt-in: nothing is queued or emitted until 'reply 1' is sent
    t_outlet* reply_outlet;     // rightmost outlet: get/listen/shred replies
    t_clock* reply_clock;       // drains the queue on the scheduler thread
    std::atomic<int> reply_enabled;  // read on the audio thread
    long slot;                  // index into CK_INSTANCE_SLOTS (-1 if none)
    t_ck_reply reply_queue[CK_REPLY_QUEUE_SIZE];
    std::atomic<long> reply_head;    // read cursor, owned by the scheduler thread
    std::atomic<long> reply_tail;    // write cursor, owned by the audio thread
    t_symbol* pending_names[CK_PENDING_SIZE]; // variable name per in-flight get
    long pending_ticket;        // next ticket to issue
    t_symbol* listen_names[CK_MAX_LISTENS];   // event name per active listener
};


// static global variables
static int CK_INSTANCE_COUNT = 0;   // monotonic; source of per-object ids
static int CK_INSTANCE_LIVE = 0;    // currently-alive instances; guards globalCleanup

// core
static void *ck_new(t_symbol *s, int argc, t_atom *argv);
static void ck_free(t_ck *x);
extern "C" void chuck_tilde_setup(void);

// reporting helpers.
//
// NOTE these are gated on x->verbose, NOT on the ChucK VM log level. The Max sibling of
// this file now does the same; the two differ only in the default (0 there,
// 1 here, each preserving what that external already printed). See
// source/docs/logging.md in the chuck-max project, "Relationship to pd-chuck". loglevel means one
// thing only -- how chatty the ChucK VM is in its own internal logging, which
// is what ChucK::setLogLevel() controls. Conflating the two means you cannot
// quieten this object without also quietening the engine, or vice versa.
// errors and warnings are never gated: a warning nobody sees is not a warning.
// query answers ('status', 'get', 'param', ...) use post() directly, since a
// question that returns nothing is indistinguishable from a broken object.
#if defined(__GNUC__) || defined(__clang__)
#define CK_PRINTF_FMT(fmt_idx, args_idx) \
    __attribute__((format(printf, fmt_idx, args_idx)))
#else
#define CK_PRINTF_FMT(fmt_idx, args_idx)
#endif

static void ck_info(t_ck* x, const char* fmt, ...) CK_PRINTF_FMT(2, 3);
static void ck_warn(t_ck* x, const char* fmt, ...) CK_PRINTF_FMT(2, 3);
static void ck_debug(t_ck* x, const char* fmt, ...) CK_PRINTF_FMT(2, 3);
static void ck_error(t_ck* x, const char* fmt, ...) CK_PRINTF_FMT(2, 3);
static void ck_verbose(t_ck* x, t_symbol* s, int argc, t_atom* argv);

// helpers
//-----------------------------------------------------------------------------------------------
// reporting helpers

static void ck_info(t_ck* x, const char* fmt, ...)
{
    if (x->verbose >= 1) {
        char msg[MAXPDSTRING];
        va_list va;
        va_start(va, fmt);
        vsnprintf(msg, MAXPDSTRING, fmt, va);
        va_end(va);
        post("[chuck~] %s", msg);
    }
}

static void ck_debug(t_ck* x, const char* fmt, ...)
{
    if (x->verbose >= 2) {
        char msg[MAXPDSTRING];
        va_list va;
        va_start(va, fmt);
        vsnprintf(msg, MAXPDSTRING, fmt, va);
        va_end(va);
        post("[chuck~ debug] %s", msg);
    }
}

// never gated
static void ck_warn(t_ck* x, const char* fmt, ...)
{
    char msg[MAXPDSTRING];
    va_list va;
    va_start(va, fmt);
    vsnprintf(msg, MAXPDSTRING, fmt, va);
    va_end(va);
    logpost(x, PD_NORMAL, "[chuck~ warning] %s", msg);
}

// never gated
static void ck_error(t_ck* x, const char* fmt, ...)
{
    char msg[MAXPDSTRING];
    va_list va;
    va_start(va, fmt);
    vsnprintf(msg, MAXPDSTRING, fmt, va);
    va_end(va);
    ck_error(x, "%s", msg);
}

// 'verbose' / 'verbose 0|1|2': how much this object reports.
// 0 quiet (errors and warnings only), 1 normal, 2 adds debug detail.
// independent of 'loglevel', which controls the ChucK VM's own logging.
static void ck_verbose(t_ck* x, t_symbol* s, int argc, t_atom* argv)
{
    if (argc == 0) {
        post("verbose: %d", x->verbose);
        return;
    }
    int v = (int)atom_getfloat(argv);
    if (v < 0 || v > 2) {
        ck_error(x, "verbose: must be 0, 1 or 2");
        return;
    }
    x->verbose = v;
    post("verbose: %d", x->verbose);
}

static void ck_stdout_print(const char* msg);
static void ck_stderr_print(const char* msg);
static void ck_run_file(t_ck *x);
static void ck_compile_file(t_ck *x, const char *filename);
static void ck_send_chuck_vm_msg(t_ck* x, Chuck_Msg_Type msg_type);
static bool path_exists(const char* name);
static t_symbol* ck_check_file(t_ck* x, t_symbol* name);

// general message handlers
static void ck_safe(t_ck* x, t_float f);          // set run_needs_audio attribute (bool)
static void ck_file(t_ck* x, t_symbol* s);        // set/get active chuck file (for run)
static void ck_bang(t_ck *x);                     // (re)load chuck file
static void ck_anything(t_ck *x, t_symbol *s, int argc, t_atom *argv); // set global params by name, value

// chuck vm message handlers
static void ck_add(t_ck* x, t_symbol* s, long argc, t_atom* argv); // add shred from file
static void ck_run(t_ck* x, t_symbol* s); // alias of add, run chuck file
static void ck_eval(t_ck* x, t_symbol* s, long argc, t_atom* argv);    // evaluation chuck code
static void ck_remove(t_ck* x, t_symbol* s, long argc, t_atom* argv);  // remove shreds (all, last, by #)
static void ck_removeall(t_ck* x);                                     // remove all shreds (keeps VM state)
static void ck_abort(t_ck* x);                                         // abort the currently-running shred
static void ck_replace(t_ck* x, t_symbol* s, long argc, t_atom* argv); // replace shreds
static void ck_clear(t_ck* x, t_symbol* s, long argc, t_atom* argv);   // clear_vm, clear_globals
static void ck_reset(t_ck* x, t_symbol* s, long argc, t_atom* argv);   // clear_vm, reset_id
static void ck_status(t_ck* x); // get info about running shreds
static void ck_time(t_ck* x);
static void ck_adaptive(t_ck* x, t_symbol* s, long argc, t_atom* argv); // get/set adaptive mode
static void ck_param(t_ck* x, t_symbol* s, long argc, t_atom* argv);    // get/set chuck params
static void ck_shreds(t_ck* x, t_symbol* s, long argc, t_atom* argv);   // shred introspection
static void ck_tap(t_ck* x, t_symbol* s, long argc, t_atom* argv);      // set global UGen to tap


// external editor message handlers
static void ck_editor(t_ck *x, t_symbol* s);
static void ck_edit(t_ck *x, t_symbol* s);

// informational message handlers
static void ck_chugins(t_ck* x);                  // probe and list available chugins
static void ck_globals(t_ck* x);                  // list global variables
static void ck_docs(t_ck* x);                     // open chuck docs in a browser
static void ck_vm(t_ck* x);                       // get vm state
static void ck_loglevel(t_ck* x, t_symbol* s, long argc, t_atom* argv); 

// audio processing
static void ck_dsp(t_ck *x, t_signal **sp);
static t_int *ck_perform(t_int *w);

// event/callback message handlers
static void ck_signal(t_ck* x, t_symbol* s);      // signal global event
static void ck_broadcast(t_ck* x, t_symbol* s);   // broadcast global event
static void ck_listen(t_ck* x, t_symbol* s, t_float listen_forever);
static void ck_unlisten(t_ck* x, t_symbol* s);

// global variable get/set via callbacks
static void ck_get(t_ck* x, t_symbol* s, long argc, t_atom* argv);
static void ck_set(t_ck* x, t_symbol* s, long argc, t_atom* argv);

// reply plumbing: chuck's globals callbacks fire on the audio thread, so
// replies are queued there and flushed on the scheduler thread by a clock
static void ck_reply(t_ck* x, t_symbol* s, int argc, t_atom* argv); // enable/disable
static void ck_reply_push(t_ck* x, t_symbol* selector, int argc, t_atom* argv);
static void ck_reply_drain(t_ck* x);
static t_ck* ck_instance_from_id(t_CKINT id);
static t_symbol* ck_pending_name(t_ck* x, t_CKINT id);
static t_CKINT ck_pending_issue(t_ck* x, t_symbol* name);

// callbacks (events); the id encodes instance slot + listener ticket
static void cb_event(t_CKINT id);

// callbacks (variables); the id encodes instance slot + request ticket
static void cb_get_int(t_CKINT id, t_CKINT val);
static void cb_get_float(t_CKINT id, t_CKFLOAT val);
static void cb_get_string(t_CKINT id, const char* val);
static void cb_get_int_array(t_CKINT id, t_CKINT array[], t_CKUINT n);
static void cb_get_int_array_value(t_CKINT id, t_CKINT value);
static void cb_get_float_array(t_CKINT id, t_CKFLOAT array[], t_CKUINT n);
static void cb_get_float_array_value(t_CKINT id, t_CKFLOAT value);
static void cb_get_assoc_int_array_value(t_CKINT id, t_CKINT val);
static void cb_get_assoc_float_array_value(t_CKINT id, t_CKFLOAT val);

// shred lifecycle watcher; BINDLE carries the t_ck* directly
static void CK_DLL_CALL cb_shreds_watcher(Chuck_VM_Shred* shred, t_CKINT code,
                                          t_CKINT param, Chuck_VM* vm, void* bindle);

// dump all global variables
static void cb_get_all_global_vars(const std::vector<Chuck_Globals_TypeValue> & list, void * data);


// global class instance
static t_class *ck_class;

// cached reply selectors. every message out of the reply outlet uses one of
// these: the selector is always ours and any user-supplied name travels as an
// argument, so no ChucK global can ever collide with the reply vocabulary.
// cached rather than gensym'd at call time because these are emitted from the
// audio thread
static t_symbol* ps_val = NULL;      // val <name> <value...>   -- 'get' reply
static t_symbol* ps_event = NULL;    // event <name>            -- 'listen'
static t_symbol* ps_shred = NULL;    // shred add|remove <id>   -- vm watcher
static t_symbol* ps_global = NULL;   // global <name> <type>    -- 'globals'

//-----------------------------------------------------------------------------------------------
// callbacks

/* nothing useful here yet */

// static void cb_demo(void)
// {
//     post("==> demo callback is called!");
// }

//-----------------------------------------------------------------------------------------------
// initialization / destruction


static void *ck_new(t_symbol *s, int argc, t_atom *argv)
{
    /* Instantiate a new object */
    t_ck *x = (t_ck *) pd_new(ck_class);

    if (x) {
        // Parse arguments: [channels] [tap_channels] [filename]
        // Numeric args come first, symbol (filename) can be at end
        int channels = 2;       // default
        int tap_channels = 0;   // default
        t_symbol* filename = gensym("");

        int num_idx = 0;
        for (int i = 0; i < argc; i++) {
            if (argv[i].a_type == A_FLOAT) {
                int val = (int)atom_getfloat(&argv[i]);
                if (num_idx == 0) {
                    channels = (val > 0 && val <= 16) ? val : 2;
                } else if (num_idx == 1) {
                    tap_channels = (val >= 0 && val <= MAX_TAP_CHANNELS) ? val : 0;
                }
                num_idx++;
            } else if (argv[i].a_type == A_SYMBOL) {
                filename = atom_getsymbol(&argv[i]);
            }
        }

        x->channels = channels;
        x->tap_channels = tap_channels;

        // Initialize tap UGens to empty
        for (int i = 0; i < MAX_TAP_CHANNELS; i++) {
            x->tap_ugens[i] = gensym("");
            x->tap_ugen_nchans[i] = 0;
            x->tap_ugen_chan[i] = 0;
        }
        x->tap_buffer = NULL;
        x->tap_buffer_frames = 0;

        // Create signal inlets (first one created by CLASS_MAINSIGNALIN)
        x->extra_inlets = NULL;
        if (x->channels > 1) {
            x->extra_inlets = (t_inlet**)getbytes(sizeof(t_inlet*) * (x->channels - 1));
            for (int i = 0; i < x->channels - 1; i++) {
                x->extra_inlets[i] = signalinlet_new((t_object *)x, 0);
            }
        }

        // Create signal outlets (main + tap)
        int total_outlets = x->channels + x->tap_channels;
        x->signal_outlets = (t_outlet**)getbytes(sizeof(t_outlet*) * total_outlets);
        for (int i = 0; i < total_outlets; i++) {
            x->signal_outlets[i] = outlet_new((t_object *)x, &s_signal);
        }

        // reply outlet, created AFTER the signal outlets so it is rightmost.
        // NOTE pd creates outlets left to right, the opposite of Max, where the
        // equivalent outlet has to be created first
        x->reply_outlet = outlet_new((t_object *)x, NULL);
        x->reply_clock = clock_new(x, (t_method)ck_reply_drain);
        x->reply_enabled.store(0);
        x->reply_head.store(0);
        x->reply_tail.store(0);
        x->pending_ticket = 0;
        x->slot = -1;
        for (int i = 0; i < CK_PENDING_SIZE; i++) {
            x->pending_names[i] = NULL;
        }
        for (int i = 0; i < CK_MAX_LISTENS; i++) {
            x->listen_names[i] = NULL;
        }

        // claim an instance slot so chuck's callbacks can find their way back
        for (long i = 0; i < CK_MAX_INSTANCES; i++) {
            t_ck* expected = NULL;
            if (CK_INSTANCE_SLOTS[i].compare_exchange_strong(expected, x)) {
                x->slot = i;
                break;
            }
        }
        if (x->slot < 0) {
            ck_error(x, "more than %d chuck~ objects: replies from this "
                        "instance will not be routed", CK_MAX_INSTANCES);
        }

        // Initialize vector pointers (will be set in ck_dsp)
        x->in_vectors = NULL;
        x->out_vectors = NULL;

        // initial inits
        x->patcher_dir = canvas_getcurrentdir();
        const char* external_dir = class_gethelpdir(ck_class);
        x->external_dir = gensym(external_dir);
        char examples_dir_cstr[MAXPDSTRING];
        snprintf(examples_dir_cstr, MAXPDSTRING, "%s/examples", x->external_dir->s_name);
        x->examples_dir = gensym(examples_dir_cstr);
        std::string examples_dir_str = std::string(x->examples_dir->s_name);

        // get external editor
        if (const char* editor = std::getenv("EDITOR")) {
            ck_debug(x, "editor from env: %s", editor);
            x->editor = gensym(editor);
        } else {
            x->editor = gensym("");
        }
        x->edit_file = gensym("");

        x->filename = ck_check_file(x, filename);
        x->current_shred_id = 0;
        x->run_needs_audio = 0;

        // chuck-related inits
        x->in_chuck_buffer = NULL;
        x->out_chuck_buffer = NULL;

        x->chuck = new ChucK();
        // set sample rate and number of in/out channels on our chuck
        x->srate = sys_getsr();

        x->chuck->setParam(CHUCK_PARAM_SAMPLE_RATE, (t_CKINT) x->srate);
        x->chuck->setParam(CHUCK_PARAM_INPUT_CHANNELS, (t_CKINT) x->channels);
        x->chuck->setParam(CHUCK_PARAM_OUTPUT_CHANNELS, (t_CKINT) x->channels);
        x->chuck->setParam(CHUCK_PARAM_VM_HALT, (t_CKINT) 0);
        x->chuck->setParam(CHUCK_PARAM_DUMP_INSTRUCTIONS, (t_CKINT) 0);
        // chuck~ is always driven from pd's audio callback, so tell the VM so;
        // this defaults to 0 and affects realtime-dependent behaviour in the engine
        x->chuck->setParam(CHUCK_PARAM_IS_REALTIME_AUDIO_HINT, (t_CKINT)1);
        // enable chugins
        x->chuck->setParam(CHUCK_PARAM_CHUGIN_ENABLE, (t_CKINT)1);
        // directory for compiled code
        x->chuck->setParam(CHUCK_PARAM_WORKING_DIRECTORY, examples_dir_str);
        std::list<std::string> chugin_search;
        std::string chugins_dir = examples_dir_str + "/chugins";
        x->chugins_dir = gensym(chugins_dir.c_str());
        chugin_search.push_back(chugins_dir);
        x->chuck->setParam( CHUCK_PARAM_IMPORT_PATH_SYSTEM, chugin_search);
        // these are process-wide, but they are the ONLY route for the VM's own
        // printing: 'status' output, the "(VM) ..." messages, EM_log and so on
        x->chuck->setStdoutCallback(ck_stdout_print);
        x->chuck->setStderrCallback(ck_stderr_print);
        x->oid = ++CK_INSTANCE_COUNT;
        CK_INSTANCE_LIVE++;
        x->verbose = 1;   // preserves this external's long-standing output

        // initialize our chuck
        x->chuck->init();
        x->chuck->start();

        // loglevel is process-global (ChucK::setLogLevel is static). set the
        // package default just once, on the first instance, so creating a
        // second chuck~ does not reset a level the user changed on the first.
        // ChucK itself defaults to CK_LOG_CORE (1); we prefer LOG_LEVEL
        static bool s_loglevel_defaulted = false;
        if (!s_loglevel_defaulted) {
            ChucK::setLogLevel(LOG_LEVEL);
            s_loglevel_defaulted = true;
        }

        // chout/cherr carry <<< >>> output from chuck code and are per-instance.
        // they must be set AFTER init(): setChoutCallback() bails out early
        // unless m_init is true and the carrier's chout already exists
        x->chuck->setChoutCallback(ck_stdout_print);
        x->chuck->setCherrCallback(ck_stderr_print);

        // report shred lifecycle out the reply outlet; the bindle carries the
        // instance, so this needs no id packing. pushes are dropped unless
        // 'reply 1' has been sent
        x->chuck->vm()->subscribe_watcher(
            cb_shreds_watcher,
            ckvm_shreds_watch_SPORK | ckvm_shreds_watch_REMOVE,
            x);

        /* Print message to Max window */
        ck_info(x, "ChucK %s", x->chuck->version());
        ck_info(x, "object created (%d channels, %d tap outlets)",
                x->channels, x->tap_channels);
        ck_debug(x, "patcher_dir: %s", x->patcher_dir->s_name);
        ck_debug(x, "external_dir: %s", x->external_dir->s_name);
        ck_debug(x, "examples_dir: %s", x->examples_dir->s_name);
        ck_debug(x, "chugins_dir: %s", x->chugins_dir->s_name);
    }
    return (void *)x;
}


static void ck_free(t_ck *x)
{
    if (x->in_chuck_buffer) {
        delete[] x->in_chuck_buffer;
        x->in_chuck_buffer = NULL;
    }
    if (x->out_chuck_buffer) {
        delete[] x->out_chuck_buffer;
        x->out_chuck_buffer = NULL;
    }

    // free tap buffer
    if (x->tap_buffer) {
        delete[] x->tap_buffer;
        x->tap_buffer = NULL;
    }

    // release the instance slot first so any callback still in flight on the
    // audio thread resolves to NULL and bails out instead of touching a
    // half-destroyed object
    if (x->slot >= 0) {
        CK_INSTANCE_SLOTS[x->slot].store(NULL, std::memory_order_release);
        x->slot = -1;
    }
    if (x->reply_clock) {
        clock_unset(x->reply_clock);
    }
    if (x->chuck) {
        if (x->chuck->vm()) {
            x->chuck->vm()->remove_watcher(cb_shreds_watcher);
        }
        delete x->chuck;
        x->chuck = NULL;
    }
    // no further pushes can occur now that the VM is gone
    if (x->reply_clock) {
        clock_free(x->reply_clock);
        x->reply_clock = NULL;
    }
    if (x->reply_outlet) {
        outlet_free(x->reply_outlet);
        x->reply_outlet = NULL;
    }

    // globalCleanup() shuts down the HID manager, serial IO and the
    // keyboard-hit manager process-wide, so it may only run once the last
    // chuck~ in the process is gone; calling it per-instance would pull those
    // subsystems out from under any still-running siblings
    if (--CK_INSTANCE_LIVE <= 0) {
        CK_INSTANCE_LIVE = 0;
        ChucK::globalCleanup();
    }

    // free vector pointer arrays
    if (x->in_vectors) {
        freebytes(x->in_vectors, sizeof(t_sample*) * x->channels);
    }
    if (x->out_vectors) {
        freebytes(x->out_vectors, sizeof(t_sample*) * (x->channels + x->tap_channels));
    }

    // free extra inlet resources
    if (x->extra_inlets) {
        for (int i = 0; i < x->channels - 1; i++) {
            inlet_free(x->extra_inlets[i]);
        }
        freebytes(x->extra_inlets, sizeof(t_inlet*) * (x->channels - 1));
    }

    // free signal outlets
    int total_outlets = x->channels + x->tap_channels;
    for (int i = 0; i < total_outlets; i++) {
        outlet_free(x->signal_outlets[i]);
    }
    freebytes(x->signal_outlets, sizeof(t_outlet*) * total_outlets);

    ck_debug(x, "memory was freed");
}


extern "C" void chuck_tilde_setup(void)
{
    /* Initialize the class with A_GIMME for variable arguments */
    ck_class = class_new(gensym("chuck~"),
        (t_newmethod)ck_new,
        (t_method)ck_free,
        sizeof(t_ck),
        CLASS_DEFAULT,
        A_GIMME,
        A_NULL);

    /* Specify signal input, with automatic float to signal conversion */
    CLASS_MAINSIGNALIN(ck_class, t_ck, x_f);

    /* Bind the DSP method, which is called when the DACs are turned on */
    class_addmethod(ck_class, (t_method)ck_dsp,        gensym("dsp"),        A_CANT,      A_NULL);

    class_addmethod(ck_class, (t_method)ck_add,        gensym("add"),        A_GIMME,     A_NULL);
    class_addmethod(ck_class, (t_method)ck_run,        gensym("run"),        A_SYMBOL,    A_NULL);
    class_addmethod(ck_class, (t_method)ck_eval,       gensym("eval"),       A_GIMME,     A_NULL);
    class_addmethod(ck_class, (t_method)ck_remove,     gensym("remove"),     A_GIMME,     A_NULL);
    class_addmethod(ck_class, (t_method)ck_removeall,  gensym("removeall"),               A_NULL);
    class_addmethod(ck_class, (t_method)ck_abort,      gensym("abort"),                   A_NULL);
    class_addmethod(ck_class, (t_method)ck_reply,      gensym("reply"),      A_GIMME,     A_NULL);
    class_addmethod(ck_class, (t_method)ck_replace,    gensym("replace"),    A_GIMME,     A_NULL);
    class_addmethod(ck_class, (t_method)ck_clear,      gensym("clear"),      A_GIMME,     A_NULL);
    class_addmethod(ck_class, (t_method)ck_reset,      gensym("reset"),      A_GIMME,     A_NULL);
    class_addmethod(ck_class, (t_method)ck_status,     gensym("status"),                  A_NULL);
    class_addmethod(ck_class, (t_method)ck_time,       gensym("time"),                    A_NULL);
    class_addmethod(ck_class, (t_method)ck_adaptive,   gensym("adaptive"),   A_GIMME,     A_NULL);
    class_addmethod(ck_class, (t_method)ck_param,      gensym("param"),      A_GIMME,     A_NULL);
    class_addmethod(ck_class, (t_method)ck_shreds,     gensym("shreds"),     A_GIMME,     A_NULL);
    class_addmethod(ck_class, (t_method)ck_tap,        gensym("tap"),        A_GIMME,     A_NULL);

    class_addmethod(ck_class, (t_method)ck_signal,     gensym("sig"),        A_SYMBOL,    A_NULL);
    class_addmethod(ck_class, (t_method)ck_broadcast,  gensym("broadcast"),  A_SYMBOL,    A_NULL);

    class_addmethod(ck_class, (t_method)ck_safe,       gensym("safe"),       A_FLOAT,     A_NULL);
    class_addmethod(ck_class, (t_method)ck_file,       gensym("file"),       A_DEFSYMBOL, A_NULL);
    class_addmethod(ck_class, (t_method)ck_editor,     gensym("editor"),     A_DEFSYMBOL, A_NULL);
    class_addmethod(ck_class, (t_method)ck_edit,       gensym("edit"),       A_DEFSYMBOL, A_NULL);

    class_addmethod(ck_class, (t_method)ck_get,        gensym("get"),        A_GIMME,     A_NULL);
    class_addmethod(ck_class, (t_method)ck_set,        gensym("set"),        A_GIMME,     A_NULL);
    class_addmethod(ck_class, (t_method)ck_listen,     gensym("listen"),     A_SYMBOL,    A_DEFFLOAT, A_NULL);
    class_addmethod(ck_class, (t_method)ck_unlisten,   gensym("unlisten"),   A_SYMBOL,    A_NULL);

    class_addmethod(ck_class, (t_method)ck_chugins,    gensym("chugins"),                 A_NULL);
    class_addmethod(ck_class, (t_method)ck_globals,    gensym("globals"),                 A_NULL);
    class_addmethod(ck_class, (t_method)ck_vm,         gensym("vm"),                      A_NULL);
    class_addmethod(ck_class, (t_method)ck_docs,       gensym("docs"),                    A_NULL);
    class_addmethod(ck_class, (t_method)ck_loglevel,   gensym("loglevel"),   A_GIMME,     A_NULL);
    class_addmethod(ck_class, (t_method)ck_verbose,    gensym("verbose"),    A_GIMME,     A_NULL);

    class_addbang(ck_class,   (t_method)ck_bang);
    class_addanything(ck_class, (t_method)ck_anything);

    // set name of default help file
    class_sethelpsymbol(ck_class, gensym("help-chuck"));

    // cache the reply selectors once
    ps_val = gensym("val");
    ps_event = gensym("event");
    ps_shred = gensym("shred");
    ps_global = gensym("global");
}
// clang-format on

//-----------------------------------------------------------------------------------------------
// helpers

static void ck_stdout_print(const char* msg) { post("%s", msg); }

static void ck_stderr_print(const char* msg) { post("%s", msg); }

static void ck_send_chuck_vm_msg(t_ck* x, Chuck_Msg_Type msg_type)
{
    Chuck_Msg* msg = new Chuck_Msg;
    msg->type = msg_type;

    // null reply so that VM will delete for us when it's done
    msg->reply_cb = (ck_msg_func)NULL;

    x->chuck->vm()->globals_manager()->execute_chuck_msg_with_globals(msg);
}

/* x-platform solution to check if a path exists
 *
 * since ext_path.h (`path_exists`) is not available
 * and std::filesystem::exists requires macos >= 10.15
 */
#ifdef __APPLE__
static bool path_exists(const char* name) { return access(name, 0) == 0; }
#else
static bool path_exists(const char* name)
{

    if (FILE* file = fopen(name, "r")) {
        fclose(file);
        return true;
    } else {
        return false;
    }
}
#endif

static bool is_safe_path(const char* path)
{
    if (!path) return false;

    std::string p(path);

    // Check for directory traversal attempts
    if (p.find("..") != std::string::npos) return false;

    // Check for absolute paths on Unix-like systems
    if (p.length() > 0 && p[0] == '/') return false;

    // Check for Windows absolute paths
    if (p.length() > 2 && p[1] == ':') return false;

    // Check for Windows UNC paths
    if (p.length() > 1 && p[0] == '\\' && p[1] == '\\') return false;

    // Check for null bytes (can be used to bypass checks)
    if (p.find('\0') != std::string::npos) return false;

    return true;
}

static t_symbol* ck_check_file(t_ck* x, t_symbol* name)
{
    // validate path safety for relative paths
    if (!is_safe_path(name->s_name)) {
        // only allow if it's an existing absolute path (for backward compatibility)
        if (path_exists(name->s_name)) {
            return name;
        }
        return gensym("");
    }

    // 1. check if file exists as given
    if (path_exists(name->s_name)) {
        return name;
    }

    // 2. check if exists with an `examples` folder prefix
    char examples_path[MAXPDSTRING];
    snprintf(examples_path, MAXPDSTRING, "%s/%s", x->examples_dir->s_name,
             name->s_name);
    if (path_exists(examples_path)) {
        return gensym(examples_path);
    }

    // 3. check if file exists in the patcher directory
    char patcher_path[MAXPDSTRING];
    snprintf(patcher_path, MAXPDSTRING, "%s/%s", x->patcher_dir->s_name,
             name->s_name);
    if (path_exists(patcher_path)) {
        return gensym(patcher_path);
    }

    return gensym("");
}


static void ck_compile_file(t_ck* x, const char* filename)
{
    post("compile: %s", filename);
    if (!x->chuck->compileFile(std::string(filename), "", 1)) {
        ck_error(x, "compilation error!: %s", filename);
    }
}

static void ck_run_file(t_ck* x)
{
    if (x->filename != gensym("")) {
        ck_compile_file(x, x->filename->s_name);
    }
}


static t_symbol* ck_get_loglevel_name(long level)
{
    t_symbol* name = NULL;

    switch (level) {
    case CK_LOG_NONE:
        name = gensym("CK_LOG_NONE");
        break;
    case CK_LOG_CORE:
        name = gensym("CK_LOG_CORE");
        break;
    case CK_LOG_SYSTEM:
        name = gensym("CK_LOG_SYSTEM");
        break;
    case CK_LOG_HERALD:
        name = gensym("CK_LOG_HERALD");
        break;
    case CK_LOG_WARNING:
        name = gensym("CK_LOG_WARNING");
        break;
    case CK_LOG_INFO:
        name = gensym("CK_LOG_INFO");
        break;
    case CK_LOG_DEBUG:
        name = gensym("CK_LOG_DEBUG");
        break;
    case CK_LOG_FINE:
        name = gensym("CK_LOG_NONE");
        break;
    case CK_LOG_FINER:
        name = gensym("CK_LOG_FINER");
        break;
    case CK_LOG_FINEST:
        name = gensym("CK_LOG_FINEST");
        break;
    case CK_LOG_ALL:
        name = gensym("CK_LOG_ALL");
        break;
    default:
        name = gensym("CK_LOG_SYSTEM");
    }
    return name;
}


//-----------------------------------------------------------------------------------------------
// general message handlers

static void ck_safe(t_ck* x, t_float f) { x->run_needs_audio = (int)f; }

static void ck_file(t_ck* x, t_symbol* s)
{
    if (s != gensym("")) {
        t_symbol* filename = ck_check_file(x, s);
        if (filename != gensym("")) {
            post("filename set: %s", filename->s_name);
            x->filename = filename;
            return;
        }
        ck_error(x, "could not set file as %s", s->s_name);
        return;
    }

    if (x->filename != gensym("")) {
        post("filename get: %s", x->filename->s_name);
    }
}


static void ck_editor(t_ck* x, t_symbol* s)
{
    if (s != gensym("")) {
        if (path_exists(s->s_name)) {
            post("editor set: %s", s->s_name);
            x->editor = s;
            return;
        }
    }

    if (x->editor != gensym("")) {
        post("editor get: %s", x->editor->s_name);
    }
}


static void ck_edit(t_ck* x, t_symbol* s)
{
    if (x->editor == gensym("")) {
        ck_error(x, "ck_edit: editor attribute not set to full path of editor");
        return;
    }

    if (s != gensym("")) {
        x->edit_file = ck_check_file(x, s);
        if (x->edit_file != gensym("")) {
            post("edit: %s", x->edit_file->s_name);
            // use fork/exec for safer command execution on Unix
#ifdef __APPLE__
            pid_t pid = fork();
            if (pid == 0) {
                // child process
                execl(x->editor->s_name, x->editor->s_name,
                      x->edit_file->s_name, (char*)NULL);
                _exit(1);  // exec failed
            } else if (pid < 0) {
                ck_error(x, "ck_edit: fork failed");
            }
            // parent continues
#else
            // fallback for other platforms - use quoted paths
            std::string cmd = "\"";
            cmd += x->editor->s_name;
            cmd += "\" \"";
            cmd += x->edit_file->s_name;
            cmd += "\"";
            std::system(cmd.c_str());
#endif
            return;
        }
    }

    if (x->filename != gensym("")) {
        ck_edit(x, x->filename);
        return;
    }
    ck_error(x, "ck_edit: reguires a valid filename");
    return;
}


static void ck_loglevel(t_ck* x, t_symbol* s, long argc, t_atom* argv)
{
    // loglevel controls the ChucK VM's own logging, which is process-global:
    // ChucK::setLogLevel/getLogLevel are static, so there is one level shared by
    // every chuck~ in the process. This reads and writes that shared state
    // directly rather than caching a per-instance copy. For per-object
    // reporting, use 'verbose' instead.
    if (argc == 0) {
        long level = (long)ChucK::getLogLevel();
        t_symbol* name = ck_get_loglevel_name(level);
        post("loglevel %ld (%s), shared by all chuck~ in this process",
             level, name->s_name);
        return;
    }
    if (argc == 1 && argv->a_type == A_FLOAT) {
        long level = (long)atom_getfloat(argv);
        if ((level >= 0) && (level <= 10)) {
            t_symbol* name = ck_get_loglevel_name(level);
            ChucK::setLogLevel(level);
            post("loglevel %ld (%s), applies to all chuck~ in this process",
                 level, name->s_name);
            return;
        }
        ck_error(x, "loglevel out of range: must be 0-10 inclusive");
        return;
    }
    ck_error(x, "could not get or set loglevel");
}
static void ck_bang(t_ck* x) { ck_run_file(x); }

static void ck_anything(t_ck* x, t_symbol* s, int argc, t_atom* argv)
{
    // TODO:
    //  - should check set op (true if succeed)
    //  - handle case of 2 length array (maybe)

    if (s == gensym("") || argc == 0) {
        goto error;
    }

    // set '+' as shorthand for ck_add method
    if (s == gensym("+")) {
        // ck_add(x, atom_getsymbol(argv));
        ck_add(x, gensym(""), argc, argv);
        return;
    }

    // set '-' as shorthand for ck_remove method
    if (s == gensym("-")) {
        ck_remove(x, gensym(""), argc, argv);
        return;
    }

    // FIXME: if possible `--` doesn't work in puredata
    // set '--' as shorthand for ck_remove (last) method
    // if (s == gensym("--")) {
    //     t_atom atoms[1];
    //     SETSYMBOL(atoms, gensym("last"));
    //     ck_remove(x, gensym(""), 1, atoms);
    //     return;
    // }

    // set '=' as shorthand for ck_replace method
    if (s == gensym("=")) {
        ck_replace(x, gensym(""), argc, argv);
        return;
    }

    // set '^' as shorthand for ck_status method
    if (s == gensym("^")) {
        ck_status(x);
        return;
    }

    if (argc == 1) {            // <param-name> <value>
        switch (argv->a_type) { // really argv[0]
        case A_FLOAT: {
            float p_float = atom_getfloat(argv);
            x->chuck->vm()->globals_manager()->setGlobalFloat(s->s_name,
                                                              p_float);
            break;
        }
        case A_SYMBOL: {
            t_symbol* p_sym = atom_getsymbol(argv);
            if (p_sym == NULL) {
                goto error;
            }
            x->chuck->vm()->globals_manager()->setGlobalString(s->s_name,
                                                               p_sym->s_name);
            break;
        }
        default:
            goto error;
            break;
        }

    } else { // type is a list

        if (argv->a_type == A_FLOAT) { // list of doubles
            double* float_array = (double*)getbytes(sizeof(double) * argc);
            for (int i = 0; i < argc; i++) {
                float_array[i] = atom_getfloat(argv + i);
            }
            x->chuck->vm()->globals_manager()->setGlobalFloatArray(
                s->s_name, float_array, argc);
            freebytes(float_array, sizeof(double) * argc);
        }
    }

    if (argc == 2) {                   // <param-name> <index|key> <value
        if (argv->a_type == A_FLOAT) { // int index
            int index = atom_getint(argv);
            float p_float = atom_getfloat(argv + 1);
            x->chuck->vm()->globals_manager()->setGlobalFloatArrayValue(
                s->s_name, index, p_float);

        } else if (argv->a_type == A_SYMBOL) { // key/value
            t_symbol* key = atom_getsymbol(argv);
            float p_float = atom_getfloat(argv + 1);
            x->chuck->vm()
                ->globals_manager()
                ->setGlobalAssociativeFloatArrayValue(s->s_name, key->s_name,
                                                      p_float);
        }
    }
    return;

error:
    ck_error(x, "[ck_anything] cannot set chuck global param");
}

//-----------------------------------------------------------------------------------------------
// informational message handlers

static void ck_docs(t_ck* x)
{
    post("open chuck docs");
    pdgui_vmess("::pd_menucommands::menu_openfile", "s",
                "https://chuck.stanford.edu/doc");
}

static void ck_globals(t_ck* x)
{
    if (x->chuck->vm()->globals_manager()->getAllGlobalVariables(
            cb_get_all_global_vars, x)) {
        return;
    }
    ck_error(x, "could not dump global variable to console");
}

static void ck_vm(t_ck* x)
{
    post("VM %d / %d status", x->oid, CK_INSTANCE_COUNT);
    post("- initialized: %d", x->chuck->vm()->has_init());
    post("- running: %d", x->chuck->vm()->running());
}

//-----------------------------------------------------------------------------------------------
// special message handlers

static void ck_chugins(t_ck* x)
{
    post("probe chugins:");
    x->chuck->probeChugins();
}


static void ck_run(t_ck* x, t_symbol* s)
{
    if (s != gensym("")) {
        if (x->run_needs_audio && !pd_getdspstate()) {
            ck_error(x, "can only run/add shred when audio is on");
            return;
        }
        t_symbol* checked_file = ck_check_file(x, s);

        if (checked_file == gensym("")) {
            ck_error(x, "could not add file");
            return;
        }
        post("filename: %s", checked_file);
        x->filename = checked_file;
        ck_run_file(x);
    }
}

static void ck_add(t_ck* x, t_symbol* s, long argc, t_atom* argv)
{
    t_symbol* filename_sym = NULL;

    if (argc < 1) {
        ck_error(x, "add message needs at least one <filename> argument");
        return;
    }

    if ((argv)->a_type != A_SYMBOL) {
        ck_error(x, "first argument must be a symbol");
        return;
    }

    if (x->run_needs_audio && !pd_getdspstate()) {
        ck_error(x, "can only run/add shred when audio is on");
        return;
    }

    if (argc > 1) { // args provided

        char atombuf[MAXPDSTRING];
        std::string str;

        for (int i = 0; i < argc; i++) {
            atom_string(argv + i, atombuf, MAXPDSTRING);
            // test if ':' is in the filename
            if (i == 0) {
                std::size_t found = std::string(atombuf).find(":");
                if (found != std::string::npos) {
                    ck_error(x, "cannot use colon-separated args, use "
                                "space-separated args instead");
                    return;
                }
            }
            str.append(atombuf);
            if (i < argc - 1)
                str.append(" ");
        }

        std::replace(str.begin(), str.end(), ' ', ':');
        filename_sym = gensym(str.c_str());
    } else {
        filename_sym = atom_getsymbol(argv);
    }

    std::string path = std::string(filename_sym->s_name);
    std::string filename;
    std::string args;
    // extract args FILE:arg1:arg2:arg3
    extract_args(path, filename, args);

    t_symbol* checked_file = ck_check_file(x, gensym(filename.c_str()));

    if (checked_file == gensym("")) {
        ck_error(x, "could not add file");
        return;
    }

    std::string full_path = std::string(checked_file->s_name);

    // compile but don't run yet (instance == 0)
    if (!x->chuck->compileFile(full_path, args, 0)) {
        ck_error(x, "could not compile file");
        return;
    }

    // construct chuck msg (must allocate on heap, as VM will clean up)
    Chuck_Msg* msg = new Chuck_Msg();
    msg->type = CK_MSG_ADD;
    msg->code = x->chuck->vm()->carrier()->compiler->output();
    msg->args = new vector<string>;
    extract_args(path, filename, *(msg->args));
    x->current_shred_id = x->chuck->vm()->process_msg(msg);
}


static void ck_eval(t_ck* x, t_symbol* s, long argc, t_atom* argv)
{
    char atombuf[MAXPDSTRING];
    std::string str;

    for (int i = 0; i < argc; i++) {
        atom_string(argv + i, atombuf, MAXPDSTRING);
        str.append(atombuf);
        if (i < argc - 1)
            str.append(" ");
    }
    // remove escapes
    str.erase(std::remove(str.begin(), str.end(), '\\'), str.end());
    post("code: %s", str.c_str());
    if (x->chuck->compileCode(str.c_str())) {
        post("compiled: success");
        return;
    }
    ck_error(x, "compiled: failed");
}


static void ck_remove(t_ck* x, t_symbol* s, long argc, t_atom* argv)
{
    Chuck_Msg* msg = new Chuck_Msg;

    if (argc == 1) {

        if (argv->a_type == A_FLOAT) {
            t_int shred_id = atom_getint(argv);
            msg->type = CK_MSG_REMOVE;
            msg->param = shred_id;

        } else if (argv->a_type == A_SYMBOL) {

            t_symbol* cmd = atom_getsymbol(argv);

            if (cmd == gensym("all")) {
                msg->type = CK_MSG_REMOVEALL;

            } else if (cmd == gensym("last")) {
                msg->type = CK_MSG_REMOVE;
                msg->param = 0xffffffff;
            }
        }

        // handle one arg case
        msg->reply_cb = (ck_msg_func)0;
        x->chuck->vm()->queue_msg(msg, 1);
        return;

    } else {
        // assume message is along :-) the lines of (remove 2 4 1 [..])
        for (int i = 0; i < argc; i++) {
            // post("removing: long_array[%d] = %d", i, long_array[i]);
            Chuck_Msg* m = new Chuck_Msg;
            m->type = CK_MSG_REMOVE;
            m->param = atom_getint(argv + i); // shred id
            m->reply_cb = (ck_msg_func)0;
            x->chuck->vm()->queue_msg(m, 1);
        }
    }
}


static void ck_replace(t_ck* x, t_symbol* s, long argc, t_atom* argv)
{
    long shred_id;
    t_symbol* filename_sym = NULL;

    if (argc < 2) {
        ck_error(x, "ck_replace: two arguments are required");
        return;
    }
    if (argv->a_type != A_FLOAT) {
        ck_error(x, "first argument must a shred id number");
        return;
    }
    shred_id = (long)atom_getfloat(argv);

    if ((argv + 1)->a_type != A_SYMBOL) {
        ck_error(x, "second argument must be a symbol");
        return;
    }
    filename_sym = atom_getsymbol(argv + 1);

    // get string
    std::string path = std::string(filename_sym->s_name);
    // filename
    std::string filename;
    // arguments
    std::string args;
    // extract args FILE:arg1:arg2:arg3
    extract_args(path, filename, args);

    std::string full_path = std::string(x->examples_dir->s_name) + "/"
        + filename; // not portable

    // compile but don't run yet (instance == 0)
    if (!x->chuck->compileFile(full_path, args, 0)) {
        ck_error(x, "could not compile file");
        return;
    }

    // construct chuck msg (must allocate on heap, as VM will clean up)
    Chuck_Msg* msg = new Chuck_Msg();
    // set type
    msg->type = CK_MSG_REPLACE;
    // set shred id to replace
    msg->param = shred_id;
    // set code for incoming shred
    msg->code = x->chuck->vm()->carrier()->compiler->output();
    // create args array
    msg->args = new vector<string>;
    // extract args again but this time into vector
    extract_args(path, filename, *(msg->args));
    // process REPLACE message, return new shred ID
    x->chuck->vm()->process_msg(msg);
}

static void ck_clear(t_ck* x, t_symbol* s, long argc, t_atom* argv)
{
    if (argc == 0) {
        return ck_send_chuck_vm_msg(x, CK_MSG_CLEARVM);
    }

    if (argc == 1) {
        if (argv->a_type == A_SYMBOL) {
            t_symbol* target = atom_getsymbol(argv);
            if (target == gensym("globals")) {
                post("=> [chuck]: clean up global variables without clearing "
                     "the whole VM");
                return ck_send_chuck_vm_msg(x, CK_MSG_CLEARGLOBALS);
            }
            if (target == gensym("vm")) {
                return ck_send_chuck_vm_msg(x, CK_MSG_CLEARVM);
            }
        }
    }
    ck_error(x, "must be 'clear globals' or 'clear vm'");
}

static void ck_reset(t_ck* x, t_symbol* s, long argc, t_atom* argv)
{
    if (argc == 0) {
        return ck_send_chuck_vm_msg(x, CK_MSG_CLEARVM);
    }

    if (argc == 1) {
        if (argv->a_type == A_SYMBOL) {
            t_symbol* target = atom_getsymbol(argv);
            if (target == gensym("id")) {
                return ck_send_chuck_vm_msg(x, CK_MSG_RESET_ID);
            }
        }
    }
    ck_error(x, "must be 'reset id' or just 'reset' for clearvm");
}


static void ck_status(t_ck* x)
{
    Chuck_VM_Shreduler* shreduler = x->chuck->vm()->shreduler();
    shreduler->status();

    if (1) {
        std::vector<Chuck_VM_Shred*> shreds;
        shreduler->get_all_shreds(shreds);
        for (const Chuck_VM_Shred* i : shreds) {
            post("%d:%s", i->get_id(), i->name.c_str());
        }
    }
}

static void ck_time(t_ck* x) { return ck_send_chuck_vm_msg(x, CK_MSG_TIME); }


static void ck_removeall(t_ck* x)
{
    // NOTE: this removes shreds only. global UGens belong to the VM rather than
    // to any shred, so a 'global SinOsc g => dac' keeps sounding after every
    // shred is gone -- which reads as "removeall did nothing" when the patch
    // gets its sound from globals. 'reset' (CK_MSG_CLEARVM) clears the type
    // system and globals too, which is why that one goes silent.
    std::vector<t_CKUINT> shred_ids;
    x->chuck->vm()->shreduler()->get_all_shred_ids(shred_ids);

    Chuck_Msg* msg = new Chuck_Msg;
    msg->type = CK_MSG_REMOVEALL;
    msg->reply_cb = (ck_msg_func)0;
    x->chuck->vm()->queue_msg(msg, 1);

    post("removeall: removing %ld shred(s); global UGens are VM state "
         "and keep running -- use 'reset' to clear those too",
         (long)shred_ids.size());
}

static void ck_abort(t_ck* x)
{
    // abort the shred currently executing in the VM. unlike 'remove', this can
    // break out of a shred stuck in a loop that never advances time.
    //
    // NOTE: this only has a target while the VM is inside a compute() cycle,
    // because Chuck_VM::abort_current_shred() reads m_shreduler->m_current_shred
    // and that is only non-NULL during compute. sending 'abort' from a message
    // box on an otherwise healthy patch therefore finds nothing to abort, and
    // correctly reports so. it bites precisely when it is needed: a runaway
    // shred leaves the audio thread stuck inside compute(), and the abort
    // arriving from the main thread then does have a current shred to flag.
    if (x->chuck == NULL || x->chuck->vm() == NULL) {
        ck_error(x, "abort: vm not available");
        return;
    }
    if (x->chuck->vm()->abort_current_shred()) {
        post("abort: aborted the running shred");
        return;
    }
    post("abort: no shred is currently executing; abort only takes "
         "effect on a shred that is stuck inside the VM");
}


static void ck_adaptive(t_ck* x, t_symbol* s, long argc, t_atom* argv)
{
    Chuck_VM_Shreduler* shreduler = x->chuck->vm()->shreduler();

    // no args: get current adaptive mode status
    if (argc == 0) {
        t_CKBOOL adaptive = shreduler->m_adaptive;
        t_CKUINT max_block = shreduler->m_max_block_size;

        if (adaptive) {
            post("adaptive: ON (max block size: %d samples)", (int)max_block);
        } else {
            post("adaptive: OFF");
        }
        return;
    }

    // one arg: set adaptive mode
    if (argc == 1 && argv->a_type == A_FLOAT) {
        t_CKUINT size = (t_CKUINT)atom_getint(argv);
        shreduler->set_adaptive(size);

        if (size > 1) {
            post("adaptive: enabled with max block size %d samples", (int)size);
        } else {
            post("adaptive: disabled");
        }
        return;
    }

    ck_error(x, "adaptive: expected no args (get) or integer (set)");
}


static void ck_param(t_ck* x, t_symbol* s, long argc, t_atom* argv)
{
    // list of known int params
    const char* int_params[] = {
        CHUCK_PARAM_SAMPLE_RATE,
        CHUCK_PARAM_INPUT_CHANNELS,
        CHUCK_PARAM_OUTPUT_CHANNELS,
        CHUCK_PARAM_VM_ADAPTIVE,
        CHUCK_PARAM_VM_HALT,
        CHUCK_PARAM_OTF_ENABLE,
        CHUCK_PARAM_OTF_PORT,
        CHUCK_PARAM_OTF_PRINT_WARNINGS,
        CHUCK_PARAM_DUMP_INSTRUCTIONS,
        CHUCK_PARAM_AUTO_DEPEND,
        CHUCK_PARAM_DEPRECATE_LEVEL,
        CHUCK_PARAM_CHUGIN_ENABLE,
        CHUCK_PARAM_IS_REALTIME_AUDIO_HINT,
        CHUCK_PARAM_COMPILER_HIGHLIGHT_ON_ERROR,
        CHUCK_PARAM_TTY_COLOR,
        CHUCK_PARAM_TTY_WIDTH_HINT,
        NULL
    };

    // list of known string params
    const char* string_params[] = {
        CHUCK_PARAM_VERSION,
        CHUCK_PARAM_WORKING_DIRECTORY,
        NULL
    };

    // list of known string list params
    const char* string_list_params[] = {
        CHUCK_PARAM_USER_CHUGINS,
        CHUCK_PARAM_IMPORT_PATH_SYSTEM,
        CHUCK_PARAM_IMPORT_PATH_PACKAGES,
        CHUCK_PARAM_IMPORT_PATH_USER,
        NULL
    };

    // helper lambdas to check param type
    auto is_int_param = [&](const std::string& name) {
        for (int i = 0; int_params[i] != NULL; i++) {
            if (name == int_params[i]) return true;
        }
        return false;
    };
    auto is_string_param = [&](const std::string& name) {
        for (int i = 0; string_params[i] != NULL; i++) {
            if (name == string_params[i]) return true;
        }
        return false;
    };
    auto is_string_list_param = [&](const std::string& name) {
        for (int i = 0; string_list_params[i] != NULL; i++) {
            if (name == string_list_params[i]) return true;
        }
        return false;
    };

    // no args: list all available params
    if (argc == 0) {
        post("ChucK VM Parameters:");
        post("  Integer parameters:");
        for (int i = 0; int_params[i] != NULL; i++) {
            t_CKINT val = x->chuck->getParamInt(int_params[i]);
            post("    %s = %lld", int_params[i], (long long)val);
        }
        post("  String parameters:");
        for (int i = 0; string_params[i] != NULL; i++) {
            std::string val = x->chuck->getParamString(string_params[i]);
            post("    %s = %s", string_params[i], val.c_str());
        }
        post("  String list parameters:");
        for (int i = 0; string_list_params[i] != NULL; i++) {
            std::list<std::string> val = x->chuck->getParamStringList(string_list_params[i]);
            post("    %s (%d items):", string_list_params[i], (int)val.size());
            for (const auto& item : val) {
                post("      - %s", item.c_str());
            }
        }
        return;
    }

    // get param name
    if (argv->a_type != A_SYMBOL) {
        ck_error(x, "param: first argument must be parameter name");
        return;
    }
    t_symbol* param_name = atom_getsymbol(argv);
    std::string name = std::string(param_name->s_name);

    // one arg: get param value
    if (argc == 1) {
        if (is_int_param(name)) {
            t_CKINT val = x->chuck->getParamInt(name);
            post("param %s = %lld", name.c_str(), (long long)val);
        } else if (is_string_param(name)) {
            std::string val = x->chuck->getParamString(name);
            post("param %s = %s", name.c_str(), val.c_str());
        } else if (is_string_list_param(name)) {
            std::list<std::string> val = x->chuck->getParamStringList(name);
            post("param %s (%d items):", name.c_str(), (int)val.size());
            for (const auto& item : val) {
                post("  - %s", item.c_str());
            }
        } else {
            ck_error(x, "param: unknown parameter '%s'", name.c_str());
        }
        return;
    }

    // two+ args: set param value
    if (argc >= 2) {
        if (is_int_param(name)) {
            if ((argv + 1)->a_type != A_FLOAT) {
                ck_error(x, "param: %s requires an integer value", name.c_str());
                return;
            }
            t_CKINT val = (t_CKINT)atom_getint(argv + 1);
            x->chuck->setParam(name, val);
            post("param %s set to %lld", name.c_str(), (long long)val);
        } else if (is_string_param(name)) {
            if ((argv + 1)->a_type != A_SYMBOL) {
                ck_error(x, "param: %s requires a string value", name.c_str());
                return;
            }
            std::string val = std::string(atom_getsymbol(argv + 1)->s_name);
            x->chuck->setParam(name, val);
            post("param %s set to %s", name.c_str(), val.c_str());
        } else if (is_string_list_param(name)) {
            // build list from remaining args
            std::list<std::string> val;
            for (int i = 1; i < argc; i++) {
                if ((argv + i)->a_type == A_SYMBOL) {
                    val.push_back(std::string(atom_getsymbol(argv + i)->s_name));
                }
            }
            x->chuck->setParam(name, val);
            post("param %s set to %d items", name.c_str(), (int)val.size());
        } else {
            ck_error(x, "param: unknown parameter '%s'", name.c_str());
        }
        return;
    }
}


// helper functions for shred introspection
static std::vector<t_CKUINT> ck_get_ready_shred_ids(t_ck* x)
{
    std::vector<t_CKUINT> shred_ids;
    Chuck_VM_Shreduler* shreduler = x->chuck->vm()->shreduler();
    shreduler->get_ready_shred_ids(shred_ids);
    return shred_ids;
}

static std::vector<t_CKUINT> ck_get_blocked_shred_ids(t_ck* x)
{
    std::vector<t_CKUINT> shred_ids;
    Chuck_VM_Shreduler* shreduler = x->chuck->vm()->shreduler();
    shreduler->get_blocked_shred_ids(shred_ids);
    return shred_ids;
}

static std::vector<t_CKUINT> ck_get_all_shred_ids(t_ck* x)
{
    std::vector<t_CKUINT> shred_ids;
    Chuck_VM_Shreduler* shreduler = x->chuck->vm()->shreduler();
    shreduler->get_all_shred_ids(shred_ids);
    return shred_ids;
}

static long ck_spork_highest_id(t_ck* x)
{
    Chuck_VM_Shreduler* shreduler = x->chuck->vm()->shreduler();
    return shreduler->highest();
}


static void ck_shreds(t_ck* x, t_symbol* s, long argc, t_atom* argv)
{
    Chuck_VM_Shreduler* shreduler = x->chuck->vm()->shreduler();

    // no args: list all shreds
    if (argc == 0) {
        std::vector<Chuck_VM_Shred*> shreds;
        shreduler->get_all_shreds(shreds);

        if (shreds.empty()) {
            post("shreds: no shreds running");
        } else {
            post("shreds: %d running", (int)shreds.size());
            for (const Chuck_VM_Shred* shred : shreds) {
                const char* state = shred->is_running ? "running" :
                                   (shred->event ? "blocked" : "ready");
                post("  [%d] %s (%s)", shred->xid, shred->name.c_str(), state);
            }
        }
        return;
    }

    // handle subcommands
    if (argv->a_type == A_SYMBOL) {
        t_symbol* cmd = atom_getsymbol(argv);

        if (cmd == gensym("all")) {
            std::vector<t_CKUINT> ids = ck_get_all_shred_ids(x);
            post("shreds all: %d shreds", (int)ids.size());
            for (t_CKUINT id : ids) {
                Chuck_VM_Shred* shred = shreduler->lookup(id);
                if (shred) {
                    post("  [%d] %s", shred->xid, shred->name.c_str());
                }
            }
            return;
        }

        if (cmd == gensym("ready")) {
            std::vector<t_CKUINT> ids = ck_get_ready_shred_ids(x);
            post("shreds ready: %d shreds", (int)ids.size());
            for (t_CKUINT id : ids) {
                Chuck_VM_Shred* shred = shreduler->lookup(id);
                if (shred) {
                    post("  [%d] %s", shred->xid, shred->name.c_str());
                }
            }
            return;
        }

        if (cmd == gensym("blocked")) {
            std::vector<t_CKUINT> ids = ck_get_blocked_shred_ids(x);
            post("shreds blocked: %d shreds", (int)ids.size());
            for (t_CKUINT id : ids) {
                Chuck_VM_Shred* shred = shreduler->lookup(id);
                if (shred) {
                    post("  [%d] %s", shred->xid, shred->name.c_str());
                }
            }
            return;
        }

        if (cmd == gensym("highest")) {
            long id = ck_spork_highest_id(x);
            post("shreds highest: %ld", id);
            return;
        }

        if (cmd == gensym("last")) {
            long id = x->chuck->vm()->last_id();
            post("shreds last: %ld", id);
            return;
        }

        if (cmd == gensym("next")) {
            long id = x->chuck->vm()->next_id();
            post("shreds next: %ld", id);
            return;
        }

        if (cmd == gensym("count")) {
            std::vector<t_CKUINT> ids = ck_get_all_shred_ids(x);
            post("shreds count: %d", (int)ids.size());
            return;
        }

        ck_error(x, "shreds: unknown subcommand '%s'", cmd->s_name);
        return;
    }

    // numeric arg: get info about specific shred
    if (argv->a_type == A_FLOAT) {
        t_CKUINT id = (t_CKUINT)atom_getint(argv);
        Chuck_VM_Shred* shred = shreduler->lookup(id);

        if (!shred) {
            ck_error(x, "shreds: shred %d not found", (int)id);
            return;
        }

        post("shred [%d]:", shred->xid);
        post("  name: %s", shred->name.c_str());
        post("  running: %s", shred->is_running ? "yes" : "no");
        post("  done: %s", shred->is_done ? "yes" : "no");
        post("  blocked: %s", shred->event ? "yes (waiting on event)" : "no");
        post("  wake_time: %.2f samples", shred->wake_time);
        post("  start: %.2f samples", shred->start);

        if (!shred->args.empty()) {
            post("  args:");
            for (const auto& arg : shred->args) {
                post("    - %s", arg.c_str());
            }
        }
        return;
    }

    ck_error(x, "shreds: invalid argument");
}


// clear a single tap slot back to "unassigned, mono"
static void ck_tap_clear_slot(t_ck* x, int i)
{
    x->tap_ugens[i] = gensym("");
    x->tap_ugen_nchans[i] = 0;
    x->tap_ugen_chan[i] = 0;
}

// drop any multichannel group that is no longer internally consistent.
// assigning a group over part of an existing one can otherwise strand the
// leftover members, which the perform loop would skip forever and leave
// holding stale audio
static void ck_tap_normalize(t_ck* x)
{
    for (int i = 0; i < x->tap_channels; i++) {
        int nchans = x->tap_ugen_nchans[i];
        if (nchans <= 1) {
            continue;
        }
        int base = i - x->tap_ugen_chan[i];
        bool ok = (base >= 0) && (base + nchans <= x->tap_channels);
        for (int c = 0; ok && c < nchans; c++) {
            int j = base + c;
            ok = (x->tap_ugens[j] == x->tap_ugens[i])
                 && (x->tap_ugen_nchans[j] == nchans)
                 && (x->tap_ugen_chan[j] == c);
        }
        if (!ok) {
            ck_tap_clear_slot(x, i);
        }
    }
}

static void ck_tap(t_ck* x, t_symbol* s, long argc, t_atom* argv)
{
    if (x->tap_channels == 0) {
        ck_error(x, "tap: no tap outlets configured (use [chuck~ channels tap_outlets])");
        return;
    }

    if (argc == 0) {
        // tap (no args): clear all taps
        for (int i = 0; i < x->tap_channels; i++) {
            ck_tap_clear_slot(x, i);
        }
        ck_info(x, "tap: cleared all");
    }
    else if (argc == 1) {
        if (argv[0].a_type == A_SYMBOL) {
            // tap ugen_name: set all outlets to tap the same UGen
            t_symbol* ugen_name = atom_getsymbol(&argv[0]);
            for (int i = 0; i < x->tap_channels; i++) {
                ck_tap_clear_slot(x, i);
                x->tap_ugens[i] = ugen_name;
            }
            ck_info(x, "tap: all outlets set to '%s'", ugen_name->s_name);
        }
        else if (argv[0].a_type == A_FLOAT) {
            // tap outlet_index: clear specific outlet
            long outlet = (long)atom_getfloat(&argv[0]);
            if (outlet < 1 || outlet > x->tap_channels) {
                ck_error(x, "tap: outlet %ld out of range (1-%d)", outlet, x->tap_channels);
                return;
            }
            ck_tap_clear_slot(x, (int)(outlet - 1));
            ck_info(x, "tap: outlet %ld cleared", outlet);
        }
        else {
            ck_error(x, "tap: invalid argument type");
        }
    }
    else if (argc == 2 || argc == 3) {
        // tap outlet_index ugen_name [nchannels]: set specific outlet, where a
        // multichannel UGen spans nchannels consecutive outlets
        if (argv[0].a_type != A_FLOAT) {
            ck_error(x, "tap: first argument must be outlet number (1-%d)", x->tap_channels);
            return;
        }
        long outlet = (long)atom_getfloat(&argv[0]);
        if (outlet < 1 || outlet > x->tap_channels) {
            ck_error(x, "tap: outlet %ld out of range (1-%d)", outlet, x->tap_channels);
            return;
        }
        if (argv[1].a_type != A_SYMBOL) {
            ck_error(x, "tap: second argument must be UGen name");
            return;
        }
        t_symbol* ugen_name = atom_getsymbol(&argv[1]);

        long nchans = 1;
        if (argc == 3) {
            if (argv[2].a_type != A_FLOAT) {
                ck_error(x, "tap: third argument must be a channel count");
                return;
            }
            nchans = (long)atom_getfloat(&argv[2]);
            if (nchans < 1) {
                ck_error(x, "tap: channel count must be at least 1");
                return;
            }
            if (outlet - 1 + nchans > x->tap_channels) {
                ck_error(x, "tap: %ld channels from outlet %ld exceeds %d tap outlets",
                         nchans, outlet, x->tap_channels);
                return;
            }
        }

        if (nchans > 1) {
            for (long c = 0; c < nchans; c++) {
                int j = (int)(outlet - 1 + c);
                x->tap_ugens[j] = ugen_name;
                x->tap_ugen_nchans[j] = (int)nchans;
                x->tap_ugen_chan[j] = (int)c;
            }
            ck_info(x, "tap: outlets %ld-%ld set to '%s' (%ld channels)",
                    outlet, outlet + nchans - 1, ugen_name->s_name, nchans);
        } else {
            ck_tap_clear_slot(x, (int)(outlet - 1));
            x->tap_ugens[outlet - 1] = ugen_name;
            ck_info(x, "tap: outlet %ld set to '%s'", outlet, ugen_name->s_name);
        }
    }
    else {
        ck_error(x, "tap: too many arguments");
        return;
    }

    ck_tap_normalize(x);
}


static void ck_signal(t_ck* x, t_symbol* s)
{
    if (!x->chuck->vm()->globals_manager()->signalGlobalEvent(s->s_name))
        ck_error(x, "[ck_signal] signal global event '%s' failed", s->s_name);
}

static void ck_broadcast(t_ck* x, t_symbol* s)
{
    if (!x->chuck->vm()->globals_manager()->broadcastGlobalEvent(s->s_name))
        ck_error(x, "[ck_broadcast] broadcast global event '%s' failed",
                 s->s_name);
}


//-----------------------------------------------------------------------------------------------
// global event callback

//-----------------------------------------------------------------------------------------------
// reply plumbing
//
// NOTE on the deferral mechanism, which differs from the Max sibling of this
// file on purpose. Max defers with qelem_set(), which is documented safe from
// the audio thread. Pd has no equivalent: calling outlet_anything() from the
// perform routine would be reentrant into the DSP graph currently being
// traversed, and clock_delay() mutates the scheduler's clock list, which is
// only safe when DSP runs on the scheduler thread -- not the case when pd runs
// audio in callback mode, and there is no public API to detect which mode is
// active. So the audio thread only ever does a bounded, lock-free write into
// the ring below, and a clock running on the scheduler thread drains it. The
// clock only runs while replies are enabled.

// resolve the instance that issued a globals request from its callback id.
// returns NULL if the object was freed while the request was in flight
static t_ck* ck_instance_from_id(t_CKINT id)
{
    long slot = CK_ID_SLOT(id);
    if (slot < 0 || slot >= CK_MAX_INSTANCES) {
        return NULL;
    }
    return CK_INSTANCE_SLOTS[slot].load(std::memory_order_acquire);
}

// remember the variable name for an outgoing request and return the id to hand
// to chuck. the ticket ring is only consulted when the reply arrives, which is
// within an audio block or two, so wraparound is not a practical concern
static t_CKINT ck_pending_issue(t_ck* x, t_symbol* name)
{
    long ticket = x->pending_ticket++;
    x->pending_names[ticket % CK_PENDING_SIZE] = name;
    return CK_ID_PACK(x->slot, ticket);
}

// recover the variable name a reply belongs to
static t_symbol* ck_pending_name(t_ck* x, t_CKINT id)
{
    long ticket = CK_ID_TICKET(id);
    t_symbol* name = x->pending_names[ticket % CK_PENDING_SIZE];
    return name ? name : gensym("?");
}

// queue a reply from the audio thread. drops the reply rather than block if the
// queue is full, which is the right trade in a realtime context
static void ck_reply_push(t_ck* x, t_symbol* selector, int argc, t_atom* argv)
{
    if (x == NULL || x->reply_outlet == NULL) {
        return;
    }
    if (!x->reply_enabled.load(std::memory_order_relaxed)) {
        return; // opt-in: 'reply 1' has not been sent
    }
    long tail = x->reply_tail.load(std::memory_order_relaxed);
    long next = (tail + 1) % CK_REPLY_QUEUE_SIZE;
    if (next == x->reply_head.load(std::memory_order_acquire)) {
        return; // full
    }
    t_ck_reply* r = &x->reply_queue[tail];
    r->selector = selector;
    r->argc = (argc > CK_REPLY_MAX_ATOMS) ? CK_REPLY_MAX_ATOMS : argc;
    for (int i = 0; i < r->argc; i++) {
        r->argv[i] = argv[i];
    }
    x->reply_tail.store(next, std::memory_order_release);
}

// drain queued replies out the reply outlet; runs on the scheduler thread
static void ck_reply_drain(t_ck* x)
{
    long head = x->reply_head.load(std::memory_order_relaxed);
    while (head != x->reply_tail.load(std::memory_order_acquire)) {
        t_ck_reply* r = &x->reply_queue[head];
        outlet_anything(x->reply_outlet, r->selector, r->argc, r->argv);
        head = (head + 1) % CK_REPLY_QUEUE_SIZE;
        x->reply_head.store(head, std::memory_order_release);
    }
    if (x->reply_enabled.load(std::memory_order_relaxed)) {
        clock_delay(x->reply_clock, CK_REPLY_POLL_MS);
    }
}

// 'reply' / 'reply 0|1': turn the data outlet on or off
static void ck_reply(t_ck* x, t_symbol* s, int argc, t_atom* argv)
{
    if (argc == 0) {
        post("reply: %s", x->reply_enabled.load() ? "on" : "off");
        return;
    }
    int on = (atom_getfloat(argv) != 0) ? 1 : 0;
    int was = x->reply_enabled.load();
    x->reply_enabled.store(on, std::memory_order_relaxed);

    if (on && !was) {
        clock_delay(x->reply_clock, CK_REPLY_POLL_MS);
        post("reply: on (values, events and shred changes out the right outlet)");
    } else if (!on && was) {
        clock_unset(x->reply_clock);
        // discard anything still queued so a later enable does not replay it
        x->reply_head.store(x->reply_tail.load());
        post("reply: off");
    }
}

//-----------------------------------------------------------------------------------------------
// global event callback

void cb_event(t_CKINT id)
{
    t_ck* x = ck_instance_from_id(id);
    if (x == NULL) {
        return;
    }
    long ticket = CK_ID_TICKET(id);
    if (ticket < 0 || ticket >= CK_MAX_LISTENS) {
        return;
    }
    t_symbol* name = x->listen_names[ticket];
    if (name == NULL) {
        return; // listener was cancelled
    }
    t_atom a[1];
    SETSYMBOL(a, name);
    ck_reply_push(x, ps_event, 1, a);
}

//-----------------------------------------------------------------------------------------------
// global variable callbacks
//
// each also posts to the pd window, which is the long-standing behaviour of
// this external and stays on regardless of whether the reply outlet is enabled

void cb_get_int(t_CKINT id, t_CKINT val)
{
    t_ck* x = ck_instance_from_id(id);
    if (x == NULL) return;
    t_symbol* name = ck_pending_name(x, id);
    post("get %s: %ld", name->s_name, (long)val);
    t_atom a[2];
    SETSYMBOL(a, name);
    SETFLOAT(a + 1, (t_float)val);
    ck_reply_push(x, ps_val, 2, a);
}

void cb_get_float(t_CKINT id, t_CKFLOAT val)
{
    t_ck* x = ck_instance_from_id(id);
    if (x == NULL) return;
    t_symbol* name = ck_pending_name(x, id);
    post("get %s: %f", name->s_name, (double)val);
    t_atom a[2];
    SETSYMBOL(a, name);
    SETFLOAT(a + 1, (t_float)val);
    ck_reply_push(x, ps_val, 2, a);
}

void cb_get_string(t_CKINT id, const char* val)
{
    t_ck* x = ck_instance_from_id(id);
    if (x == NULL) return;
    t_symbol* name = ck_pending_name(x, id);
    post("get %s: %s", name->s_name, val ? val : "");
    t_atom a[2];
    SETSYMBOL(a, name);
    SETSYMBOL(a + 1, gensym(val ? val : ""));
    ck_reply_push(x, ps_val, 2, a);
}

// 'get' replies are emitted as 'val <name> <value...>'. the variable name is
// user-controlled text, so it travels as an argument rather than as the
// selector; putting it in the selector would let a ChucK global named 'shred'
// or 'event' masquerade as a control message
#define CK_VAL_MAX_VALUES (CK_REPLY_MAX_ATOMS - 1)

void cb_get_int_array(t_CKINT id, t_CKINT array[], t_CKUINT n)
{
    t_ck* x = ck_instance_from_id(id);
    if (x == NULL) return;
    t_symbol* name = ck_pending_name(x, id);
    post("get %s: %lu values", name->s_name, (unsigned long)n);
    t_atom a[CK_REPLY_MAX_ATOMS];
    int count = (n > (t_CKUINT)CK_VAL_MAX_VALUES) ? CK_VAL_MAX_VALUES : (int)n;
    SETSYMBOL(a, name);
    for (int i = 0; i < count; i++) {
        SETFLOAT(a + 1 + i, (t_float)array[i]);
    }
    ck_reply_push(x, ps_val, count + 1, a);
}

void cb_get_float_array(t_CKINT id, t_CKFLOAT array[], t_CKUINT n)
{
    t_ck* x = ck_instance_from_id(id);
    if (x == NULL) return;
    t_symbol* name = ck_pending_name(x, id);
    post("get %s: %lu values", name->s_name, (unsigned long)n);
    t_atom a[CK_REPLY_MAX_ATOMS];
    int count = (n > (t_CKUINT)CK_VAL_MAX_VALUES) ? CK_VAL_MAX_VALUES : (int)n;
    SETSYMBOL(a, name);
    for (int i = 0; i < count; i++) {
        SETFLOAT(a + 1 + i, (t_float)array[i]);
    }
    ck_reply_push(x, ps_val, count + 1, a);
}

void cb_get_int_array_value(t_CKINT id, t_CKINT value)
{
    cb_get_int(id, value);
}

void cb_get_float_array_value(t_CKINT id, t_CKFLOAT value)
{
    cb_get_float(id, value);
}

void cb_get_assoc_int_array_value(t_CKINT id, t_CKINT val)
{
    cb_get_int(id, val);
}

void cb_get_assoc_float_array_value(t_CKINT id, t_CKFLOAT val)
{
    cb_get_float(id, val);
}

//-----------------------------------------------------------------------------------------------
// shred lifecycle watcher

static void CK_DLL_CALL cb_shreds_watcher(Chuck_VM_Shred* shred, t_CKINT code,
                                          t_CKINT param, Chuck_VM* vm, void* bindle)
{
    t_ck* x = (t_ck*)bindle;
    if (x == NULL || shred == NULL) {
        return;
    }

    const char* what = NULL;
    switch (code) {
    case ckvm_shreds_watch_SPORK:    what = "add";      break;
    case ckvm_shreds_watch_REMOVE:   what = "remove";   break;
    default: return;
    }

    t_atom a[2];
    SETSYMBOL(a, gensym(what));
    SETFLOAT(a + 1, (t_float)shred->get_id());
    ck_reply_push(x, ps_shred, 2, a);
}

//-----------------------------------------------------------------------------------------------
// dump all global variables

void cb_get_all_global_vars(const std::vector<Chuck_Globals_TypeValue> & list, void * data)
{
    t_ck* x = (t_ck*)data;
    post("global variables:");
    for (auto v : list) {
        post("  type: %s name: %s", v.type.c_str(), v.name.c_str());
        if (x != NULL) {
            // name before type. once a patch strips our selector with
            // [route global], whatever follows becomes the new selector, and
            // ChucK's 'int' and 'float' type names collide with pd's typed
            // float method there. variable names cannot be ChucK keywords, so
            // leading with the name avoids that, and matches 'val <name> ...'
            t_atom a[2];
            SETSYMBOL(a, gensym(v.name.c_str()));
            SETSYMBOL(a + 1, gensym(v.type.c_str()));
            ck_reply_push(x, ps_global, 2, a);
        }
    }
}


//-----------------------------------------------------------------------------------------------
// set/get chuck global variables


void ck_set(t_ck* x, t_symbol* s, long argc, t_atom* argv)
{
    if (argc < 3) {
        ck_error(x, "ck_set: too few # of arguments");
        return;
    }

    if (!(argv->a_type == A_SYMBOL && (argv + 1)->a_type == A_SYMBOL)) {
        ck_error(x, "ck_get: first two args must be symbols");
        return;
    }

    t_symbol* type = atom_getsymbol(argv);
    t_symbol* name = atom_getsymbol(argv + 1);

    if (argc == 3) {
        if (type == gensym("int") && (argv + 2)->a_type == A_FLOAT) {
            // t_int value = atom_getintarg(2, argc, argv);
            t_int value = atom_getint(argv + 2);
            if (x->chuck->vm()->globals_manager()->setGlobalInt(
                    name->s_name, (t_CKINT)value)) {
                post("set %s -> %d", name->s_name, value);
                return;
            }
        } else if (type == gensym("float") && (argv + 2)->a_type == A_FLOAT) {
            t_float value = atom_getfloat(argv + 2);
            // t_float value = atom_getfloatarg(2, argc, argv);
            if (x->chuck->vm()->globals_manager()->setGlobalFloat(
                    name->s_name, (t_CKFLOAT)value)) {
                post("set %s -> %f", name->s_name, value);
                return;
            }
        } else if (type == gensym("string")
                   && (argv + 2)->a_type == A_SYMBOL) {
            t_symbol* value = atom_getsymbol(argv + 2);
            // t_symbol* value = atom_getsymbolarg(2, argc, argv);
            if (x->chuck->vm()->globals_manager()->setGlobalString(
                    name->s_name, value->s_name)) {
                post("set %s -> %s", name->s_name, value->s_name);
                return;
            }
        }
        return;
    } else if (argc > 3) {
        int offset = 2;
        int length = (int)argc - offset;

        if (type == gensym("int[]")) { // list of longs
            long* long_array = (long*)getbytes(sizeof(long) * length);
            for (int i = 0; i < length; i++) {
                post("set %s[%d] -> %d ", name->s_name, i,
                     atom_getint((argv + offset) + i));
                long_array[i] = atom_getint((argv + offset) + i);
            }
            if (x->chuck->vm()->globals_manager()->setGlobalIntArray(
                    name->s_name, long_array, length)) {
                freebytes(long_array, sizeof(long) * length);
                return;
            }
        } else if (type == gensym("float[]")) { // list of doubles
            double* float_array = (double*)getbytes(sizeof(double) * length);
            for (int i = 0; i < length; i++) {
                post("set %s[%d] -> %f ", name->s_name, i,
                     atom_getfloat((argv + offset) + i));
                float_array[i] = atom_getfloat((argv + offset) + i);
            }
            if (x->chuck->vm()->globals_manager()->setGlobalFloatArray(
                    name->s_name, float_array, length)) {
                freebytes(float_array, sizeof(double) * length);
                return;
            }
        } else if (type == gensym("int[i]")) {
            long index = atom_getint((argv + 2));
            long value = atom_getint((argv + 3));
            if (x->chuck->vm()->globals_manager()->setGlobalIntArrayValue(
                    name->s_name, (t_CKUINT)index, (t_CKINT)value)) {
                post("set %s[%ld] -> %ld", name->s_name, index, value);
                return;
            }
        } else if (type == gensym("float[i]")) {
            long index = atom_getint((argv + 2));
            // must be a float type: holding this in a long truncated every
            // fractional value on its way to the VM, so
            // 'set float[i] a 0 0.5' stored 0.0
            t_float value = atom_getfloat((argv + 3));
            if (x->chuck->vm()->globals_manager()->setGlobalFloatArrayValue(
                    name->s_name, (t_CKUINT)index, (t_CKFLOAT)value)) {
                post("set %s[%ld] -> %f", name->s_name, index, (double)value);
                return;
            }
        } else if (type == gensym("int[k]")) {
            t_symbol* key = atom_getsymbol((argv + 2));
            long value = atom_getint((argv + 3));
            if (x->chuck->vm()
                    ->globals_manager()
                    ->setGlobalAssociativeIntArrayValue(
                        name->s_name, key->s_name, (t_CKINT)value))
                return;
        } else if (type == gensym("float[k]")) {
            t_symbol* key = atom_getsymbol((argv + 2));
            // as above: truncating here silently discarded the fractional part
            t_float value = atom_getfloat((argv + 3));
            if (x->chuck->vm()
                    ->globals_manager()
                    ->setGlobalAssociativeFloatArrayValue(
                        name->s_name, key->s_name, (t_CKFLOAT)value))
                return;
        }
    }
    return;
}

void ck_get(t_ck* x, t_symbol* s, long argc, t_atom* argv)
{
    if (argc < 2 || argc > 3) {
        ck_error(x, "ck_get: invalid # of arguments");
        return;
    }

    if (!(argv->a_type == A_SYMBOL && (argv + 1)->a_type == A_SYMBOL)) {
        ck_error(x, "ck_get: first two args must be symbols");
        return;
    }

    t_symbol* type = atom_getsymbol(argv);
    t_symbol* name = atom_getsymbol(argv + 1);

    // the callback-id overloads let the reply be routed back to this object;
    // the plain-name overloads cannot be attributed when several chuck~ objects
    // are present. the id also carries a ticket that recovers the variable name
    Chuck_Globals_Manager* gm = x->chuck->vm()->globals_manager();
    t_CKINT id = ck_pending_issue(x, name);

    if (argc == 2) {
        if (type == gensym("int")) {
            if (gm->getGlobalInt(name->s_name, id, cb_get_int)) return;
        } else if (type == gensym("float")) {
            if (gm->getGlobalFloat(name->s_name, id, cb_get_float)) return;
        } else if (type == gensym("string")) {
            if (gm->getGlobalString(name->s_name, id, cb_get_string)) return;
        } else if (type == gensym("int[]")) {
            if (gm->getGlobalIntArray(name->s_name, id, cb_get_int_array)) return;
        } else if (type == gensym("float[]")) {
            if (gm->getGlobalFloatArray(name->s_name, id, cb_get_float_array)) return;
        }
        return;
    } else if (argc == 3) {
        if ((argv + 2)->a_type == A_FLOAT) {
            t_int index = atom_getint(argv + 2);
            if (type == gensym("int[]") || type == gensym("int[i]")) {
                if (gm->getGlobalIntArrayValue(name->s_name, id, (t_CKUINT)index,
                                               cb_get_int_array_value))
                    return;
            } else if (type == gensym("float[]") || type == gensym("float[i]")) {
                if (gm->getGlobalFloatArrayValue(name->s_name, id, (t_CKUINT)index,
                                                 cb_get_float_array_value))
                    return;
            }
            return;
        } else if ((argv + 2)->a_type == A_SYMBOL) {
            t_symbol* key = atom_getsymbol(argv + 2);
            if (type == gensym("int[]") || type == gensym("int[k]")) {
                if (gm->getGlobalAssociativeIntArrayValue(name->s_name, id, key->s_name,
                                                          cb_get_assoc_int_array_value))
                    return;
            } else if (type == gensym("float[]") || type == gensym("float[k]")) {
                if (gm->getGlobalAssociativeFloatArrayValue(name->s_name, id, key->s_name,
                                                            cb_get_assoc_float_array_value))
                    return;
            }
        }
    }
}

static void ck_listen(t_ck* x, t_symbol* s, t_float listen_forever)
{
    // listeners are long-lived, so each gets a dedicated ticket slot rather than
    // a ring entry; the ticket is needed again to cancel the listener later
    long ticket = -1;
    for (long i = 0; i < CK_MAX_LISTENS; i++) {
        if (x->listen_names[i] == NULL) {
            ticket = i;
            break;
        }
        if (x->listen_names[i] == s) {
            ck_warn(x, "listen: already listening to event %s", s->s_name);
            return;
        }
    }
    if (ticket < 0) {
        ck_error(x, "listen: too many active listeners (max %d)", CK_MAX_LISTENS);
        return;
    }

    x->listen_names[ticket] = s;
    t_CKINT id = CK_ID_PACK(x->slot, ticket);

    if (x->chuck->vm()->globals_manager()->listenForGlobalEvent(
            s->s_name, id, cb_event, (t_CKBOOL)listen_forever)) {
        ck_info(x, "listening to event %s", s->s_name);
        return;
    }
    x->listen_names[ticket] = NULL;
}

static void ck_unlisten(t_ck* x, t_symbol* s)
{
    long ticket = -1;
    for (long i = 0; i < CK_MAX_LISTENS; i++) {
        if (x->listen_names[i] == s) {
            ticket = i;
            break;
        }
    }
    if (ticket < 0) {
        ck_error(x, "unlisten: not listening to event %s", s->s_name);
        return;
    }

    t_CKINT id = CK_ID_PACK(x->slot, ticket);
    if (x->chuck->vm()->globals_manager()->stopListeningForGlobalEvent(
            s->s_name, id, cb_event)) {
        x->listen_names[ticket] = NULL;
        ck_info(x, "stop listening to event %s", s->s_name);
    }
}


//-----------------------------------------------------------------------------------------------
// audio processing

static void ck_dsp(t_ck* x, t_signal** sp)
{
    // propagate the host sample rate to the VM. the rate is otherwise fixed at
    // object creation from sys_getsr(), so changing pd's sample rate afterwards
    // left chuck computing at the old rate (drifting pitch and timing).
    // setParam() forwards to Chuck_VM::update_srate() on a running VM.
    // pd's dsp method has no samplerate argument; the rate is on the signal.
    if (x->chuck != NULL && sp[0]->s_sr > 0) {
        t_CKINT sr = (t_CKINT)sp[0]->s_sr;
        if (sr != x->chuck->getParamInt(CHUCK_PARAM_SAMPLE_RATE)) {
            x->chuck->setParam(CHUCK_PARAM_SAMPLE_RATE, sr);
            x->srate = (int)sr;
                ck_info(x, "sample rate updated to %d", x->srate);
        }
    }

    delete[] x->in_chuck_buffer;
    delete[] x->out_chuck_buffer;

    x->buffer_size = sp[0]->s_n;

    // allocate chuck buffers based on configurable channel count
    x->in_chuck_buffer = new float[x->buffer_size * x->channels];
    x->out_chuck_buffer = new float[x->buffer_size * x->channels];

    memset(x->in_chuck_buffer, 0.f,
           sizeof(float) * x->buffer_size * x->channels);
    memset(x->out_chuck_buffer, 0.f,
           sizeof(float) * x->buffer_size * x->channels);

    // allocate tap buffer if tap is enabled
    if (x->tap_channels > 0) {
        delete[] x->tap_buffer;
        // sized for the widest possible multichannel fetch:
        // getGlobalUGenSamplesMulti() writes numFrames samples per channel,
        // non-interleaved, for up to tap_channels channels
        x->tap_buffer = new float[x->buffer_size * x->tap_channels];
        memset(x->tap_buffer, 0.f,
               sizeof(float) * x->buffer_size * x->tap_channels);
        x->tap_buffer_frames = x->buffer_size;
    }

    // store vector pointers in the struct
    // free old ones first
    if (x->in_vectors) {
        freebytes(x->in_vectors, sizeof(t_sample*) * x->channels);
    }
    if (x->out_vectors) {
        freebytes(x->out_vectors, sizeof(t_sample*) * (x->channels + x->tap_channels));
    }

    x->in_vectors = (t_sample**)getbytes(sizeof(t_sample*) * x->channels);
    x->out_vectors = (t_sample**)getbytes(sizeof(t_sample*) * (x->channels + x->tap_channels));

    // sp layout: [in0, in1, ..., inN-1, out0, out1, ..., outN-1, tap0, tap1, ...]
    for (int i = 0; i < x->channels; i++) {
        x->in_vectors[i] = sp[i]->s_vec;
        x->out_vectors[i] = sp[x->channels + i]->s_vec;
    }
    // tap outlets come after main outputs
    for (int i = 0; i < x->tap_channels; i++) {
        x->out_vectors[x->channels + i] = sp[x->channels * 2 + i]->s_vec;
    }

    /* Attach the object to the DSP chain - just pass object and vector size */
    dsp_add(ck_perform, 2, x, sp[0]->s_n);

    /* Print message to Pd window */
    ck_debug(x, "executing perform routine");
    ck_debug(x, "sample rate: %d", x->srate);
    ck_debug(x, "buffer size: %d", x->buffer_size);
    ck_debug(x, "channels: %d (+ %d tap outlets)", x->channels, x->tap_channels);
}

static t_int* ck_perform(t_int* w)
{
    t_ck* x = (t_ck*)(w[1]);
    int n = (int)(w[2]);

    float* in_ptr = x->in_chuck_buffer;

    // interleave input: convert from Pd's non-interleaved to ChucK's interleaved
    for (int i = 0; i < n; i++) {
        for (int chan = 0; chan < x->channels; chan++) {
            in_ptr[i * x->channels + chan] = x->in_vectors[chan][i];
        }
    }

    // run ChucK (interleaved buffers)
    x->chuck->run(x->in_chuck_buffer, x->out_chuck_buffer, n);

    // de-interleave output: convert from ChucK's interleaved to Pd's non-interleaved
    float* out_ptr = x->out_chuck_buffer;
    for (int i = 0; i < n; i++) {
        for (int chan = 0; chan < x->channels; chan++) {
            x->out_vectors[chan][i] = out_ptr[i * x->channels + chan];
        }
    }

    // tap global UGen samples if enabled (each outlet taps independently)
    if (x->tap_channels > 0 && x->tap_buffer) {
        for (int chan = 0; chan < x->tap_channels; chan++) {
            t_symbol* ugen_name = x->tap_ugens[chan];
            int tap_outlet = x->channels + chan;

            if (ugen_name != gensym("")) {
                int nchans = x->tap_ugen_nchans[chan];

                if (nchans > 1) {
                    // multichannel UGen: fetch every channel once, on the outlet
                    // carrying channel 0, then fan the block out to the outlets
                    // that follow. chuck writes non-interleaved, channel-major.
                    if (x->tap_ugen_chan[chan] != 0) {
                        continue; // already filled by this group's channel 0
                    }
                    t_CKBOOL ok = x->chuck->vm()->globals_manager()->getGlobalUGenSamplesMulti(
                        ugen_name->s_name, x->tap_buffer, (int)n, nchans);

                    for (int c = 0; c < nchans; c++) {
                        int out_index = tap_outlet + c;
                        for (int i = 0; i < n; i++) {
                            x->out_vectors[out_index][i] = ok
                                ? x->tap_buffer[c * n + i]
                                : 0.0f;
                        }
                    }
                    continue;
                }

                // tap this outlet's UGen (mono)
                t_CKBOOL success = x->chuck->vm()->globals_manager()->getGlobalUGenSamples(
                    ugen_name->s_name, x->tap_buffer, (int)n);

                if (success) {
                    for (int i = 0; i < n; i++) {
                        x->out_vectors[tap_outlet][i] = x->tap_buffer[i];
                    }
                } else {
                    // UGen not found or not ready - output silence
                    for (int i = 0; i < n; i++) {
                        x->out_vectors[tap_outlet][i] = 0.0f;
                    }
                }
            } else {
                // no UGen assigned to this outlet - output silence
                for (int i = 0; i < n; i++) {
                    x->out_vectors[tap_outlet][i] = 0.0f;
                }
            }
        }
    }

    /* Return the next address in the DSP chain */
    return w + 3;
}
