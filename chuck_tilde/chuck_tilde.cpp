#include "m_pd.h"

#include "chuck.h"
#include "chuck_globals.h"

#include <algorithm>
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

// object struct
typedef struct _ck {
    t_object obj;
    t_float x_f;

    int oid;                    // object id
    int srate;                  // sample rate
    int loglevel;               // loglevel

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
    float* tap_buffer;          // buffer for tapped samples

    // dynamic inlets/outlets
    t_inlet** extra_inlets;     // extra signal inlets beyond the default one
    t_outlet** signal_outlets;  // all signal outlets (main + tap)
} t_ck;


// static global variables
static int CK_INSTANCE_COUNT = 0;

// core
static void *ck_new(t_symbol *s, int argc, t_atom *argv);
static void ck_free(t_ck *x);
extern "C" void chuck_tilde_setup(void);

// helpers
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

// callbacks (events) -> map_cb_event
static void cb_event(const char* name);

// callbacks (variables)
static void cb_get_int(const char* name, t_CKINT val);
static void cb_get_float(const char* name, t_CKFLOAT val);
static void cb_get_string(const char* name, const char* val);
static void cb_get_int_array(const char* name, t_CKINT array[], t_CKUINT n);
static void cb_get_int_array_value(const char* name, t_CKINT value);
static void cb_get_float_array(const char* name, t_CKFLOAT array[], t_CKUINT n);
static void cb_get_float_array_value(const char* name, t_CKFLOAT value);
static void cb_get_assoc_int_array_value(const char* name, t_CKINT val);
static void cb_get_assoc_float_array_value(const char* name, t_CKFLOAT val);

// dump all global variables
static void cb_get_all_global_vars(const std::vector<Chuck_Globals_TypeValue> & list, void * data);


// global class instance
static t_class *ck_class;

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
        }
        x->tap_buffer = NULL;

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
            post("editor from env: %s", editor);
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
        x->chuck->setParam(CHUCK_PARAM_WORKING_DIRECTORY, x->examples_dir->s_name);
        x->chuck->setParam(CHUCK_PARAM_VM_HALT, (t_CKINT) 0);
        x->chuck->setParam(CHUCK_PARAM_DUMP_INSTRUCTIONS, (t_CKINT) 0);
        // enable chugins
        x->chuck->setParam(CHUCK_PARAM_CHUGIN_ENABLE, (t_CKINT)1);
        // directory for compiled code
        x->chuck->setParam( CHUCK_PARAM_WORKING_DIRECTORY, examples_dir_str);
        std::list<std::string> chugin_search;
        std::string chugins_dir = examples_dir_str + "/chugins";
        x->chugins_dir = gensym(chugins_dir.c_str());
        chugin_search.push_back(chugins_dir);
        x->chuck->setParam( CHUCK_PARAM_IMPORT_PATH_SYSTEM, chugin_search);
        x->chuck->setStdoutCallback(ck_stdout_print);
        x->chuck->setStderrCallback(ck_stderr_print);
        x->oid = ++CK_INSTANCE_COUNT;
        x->loglevel = LOG_LEVEL;

        // initialize our chuck
        x->chuck->init();
        x->chuck->start();

        // set chuck log level
        ChucK::setLogLevel(x->loglevel);

        /* Print message to Max window */
        post("ChucK %s", x->chuck->version());
        post("chuck~ - Object was created (%d channels, %d tap outlets)",
             x->channels, x->tap_channels);
        post("patcher_dir: %s", x->patcher_dir->s_name);
        post("external_dir: %s", x->external_dir->s_name);
        post("examples_dir: %s", x->examples_dir->s_name);
        post("chugins_dir: %s", x->chugins_dir->s_name);
    }
    return (void *)x;
}


static void ck_free(t_ck *x)
{
    delete[] x->in_chuck_buffer;
    delete[] x->out_chuck_buffer;
    delete x->chuck;

    // free tap buffer
    if (x->tap_buffer) {
        delete[] x->tap_buffer;
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

    post("chuck~ - Memory was freed");
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

    class_addbang(ck_class,   (t_method)ck_bang);
    class_addanything(ck_class, (t_method)ck_anything);

    // set name of default help file
    class_sethelpsymbol(ck_class, gensym("help-chuck"));
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
        pd_error(x, "compilation error!: %s", filename);
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
        pd_error(x, (char*)"could not set file as %s", s->s_name);
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
        pd_error(
            x,
            (char*)"ck_edit: editor attribute not set to full path of editor");
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
                pd_error(x, "ck_edit: fork failed");
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
    pd_error(x, (char*)"ck_edit: reguires a valid filename");
    return;
}


static void ck_loglevel(t_ck* x, t_symbol* s, long argc, t_atom* argv)
{
    t_symbol* name = NULL;

    if (argc == 0) {
        x->loglevel = ChucK::getLogLevel();
        name = ck_get_loglevel_name(x->loglevel);
        post("loglevel is %d (%s)", x->loglevel, name->s_name);
        return;
    }
    if (argc == 1) {
        long level = 2;
        if (argv->a_type == A_FLOAT) {
            level = (long)atom_getfloat(argv);
            if ((level >= 0) && (level <= 10)) {
                name = ck_get_loglevel_name(level);
                x->loglevel = level;
                post("setting loglevel to %d (%s)", x->loglevel, name->s_name);
                ChucK::setLogLevel(x->loglevel);
                return;
            } else {
                pd_error(x,
                         "out-of-range: defaulting to level 2. loglevel must "
                         "be between 0-10 inclusive");
                ChucK::setLogLevel(CK_LOG_SYSTEM);
                return;
            }
        }
    }
    pd_error(x, "could not get or set loglevel");
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
            double* float_array = (double*)getbytes(sizeof(double*) * argc);
            for (int i = 0; i < argc; i++) {
                float_array[i] = atom_getfloat(argv + i);
            }
            x->chuck->vm()->globals_manager()->setGlobalFloatArray(
                s->s_name, float_array, argc);
            freebytes(float_array, sizeof(double*) * argc);
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
    pd_error(x, "[ck_anything] cannot set chuck global param");
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
            cb_get_all_global_vars, NULL)) {
        return;
    }
    pd_error(x, (char*)"could not dump global variable to console");
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
            pd_error(x, (char*)"can only run/add shred when audio is on");
            return;
        }
        t_symbol* checked_file = ck_check_file(x, s);

        if (checked_file == gensym("")) {
            pd_error(x, (char*)"could not add file");
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
        pd_error(x,
                 (char*)"add message needs at least one <filename> argument");
        return;
    }

    if ((argv)->a_type != A_SYMBOL) {
        pd_error(x, (char*)"first argument must be a symbol");
        return;
    }

    if (x->run_needs_audio && !pd_getdspstate()) {
        pd_error(x, (char*)"can only run/add shred when audio is on");
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
                    pd_error(x,
                             (char*)"cannot use colon-separated args, use "
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
        pd_error(x, (char*)"could not add file");
        return;
    }

    std::string full_path = std::string(checked_file->s_name);

    // compile but don't run yet (instance == 0)
    if (!x->chuck->compileFile(full_path, args, 0)) {
        pd_error(x, (char*)"could not compile file");
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
    pd_error(x, (char*)"compiled: failed");
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
        pd_error(x, "ck_replace: two arguments are required");
        return;
    }
    if (argv->a_type != A_FLOAT) {
        pd_error(x, "first argument must a shred id number");
        return;
    }
    shred_id = (long)atom_getfloat(argv);

    if ((argv + 1)->a_type != A_SYMBOL) {
        pd_error(x, "second argument must be a symbol");
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
        pd_error(x, "could not compile file");
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
    pd_error(x, "must be 'clear globals' or 'clear vm'");
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
    pd_error(x, "must be 'reset id' or just 'reset' for clearvm");
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
    Chuck_Msg* msg = new Chuck_Msg;
    msg->type = CK_MSG_REMOVEALL;
    msg->reply_cb = (ck_msg_func)0;
    x->chuck->vm()->queue_msg(msg, 1);
    post("removeall: removed all shreds");
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

    pd_error(x, "adaptive: expected no args (get) or integer (set)");
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
        pd_error(x, "param: first argument must be parameter name");
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
            pd_error(x, "param: unknown parameter '%s'", name.c_str());
        }
        return;
    }

    // two+ args: set param value
    if (argc >= 2) {
        if (is_int_param(name)) {
            if ((argv + 1)->a_type != A_FLOAT) {
                pd_error(x, "param: %s requires an integer value", name.c_str());
                return;
            }
            t_CKINT val = (t_CKINT)atom_getint(argv + 1);
            x->chuck->setParam(name, val);
            post("param %s set to %lld", name.c_str(), (long long)val);
        } else if (is_string_param(name)) {
            if ((argv + 1)->a_type != A_SYMBOL) {
                pd_error(x, "param: %s requires a string value", name.c_str());
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
            pd_error(x, "param: unknown parameter '%s'", name.c_str());
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

        pd_error(x, "shreds: unknown subcommand '%s'", cmd->s_name);
        return;
    }

    // numeric arg: get info about specific shred
    if (argv->a_type == A_FLOAT) {
        t_CKUINT id = (t_CKUINT)atom_getint(argv);
        Chuck_VM_Shred* shred = shreduler->lookup(id);

        if (!shred) {
            pd_error(x, "shreds: shred %d not found", (int)id);
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

    pd_error(x, "shreds: invalid argument");
}


static void ck_tap(t_ck* x, t_symbol* s, long argc, t_atom* argv)
{
    if (x->tap_channels == 0) {
        pd_error(x, "tap: no tap outlets configured (use [chuck~ channels tap_outlets])");
        return;
    }

    if (argc == 0) {
        // tap (no args): clear all taps
        for (int i = 0; i < x->tap_channels; i++) {
            x->tap_ugens[i] = gensym("");
        }
        post("tap: cleared all");
    }
    else if (argc == 1) {
        if (argv[0].a_type == A_SYMBOL) {
            // tap ugen_name: set all outlets to tap the same UGen
            t_symbol* ugen_name = atom_getsymbol(&argv[0]);
            for (int i = 0; i < x->tap_channels; i++) {
                x->tap_ugens[i] = ugen_name;
            }
            post("tap: all outlets set to '%s'", ugen_name->s_name);
        }
        else if (argv[0].a_type == A_FLOAT) {
            // tap outlet_index: clear specific outlet
            long outlet = (long)atom_getfloat(&argv[0]);
            if (outlet < 1 || outlet > x->tap_channels) {
                pd_error(x, "tap: outlet %ld out of range (1-%d)", outlet, x->tap_channels);
                return;
            }
            x->tap_ugens[outlet - 1] = gensym("");
            post("tap: outlet %ld cleared", outlet);
        }
        else {
            pd_error(x, "tap: invalid argument type");
        }
    }
    else if (argc == 2) {
        // tap outlet_index ugen_name: set specific outlet
        if (argv[0].a_type != A_FLOAT) {
            pd_error(x, "tap: first argument must be outlet number (1-%d)", x->tap_channels);
            return;
        }
        long outlet = (long)atom_getfloat(&argv[0]);
        if (outlet < 1 || outlet > x->tap_channels) {
            pd_error(x, "tap: outlet %ld out of range (1-%d)", outlet, x->tap_channels);
            return;
        }
        if (argv[1].a_type != A_SYMBOL) {
            pd_error(x, "tap: second argument must be UGen name");
            return;
        }
        t_symbol* ugen_name = atom_getsymbol(&argv[1]);
        x->tap_ugens[outlet - 1] = ugen_name;
        post("tap: outlet %ld set to '%s'", outlet, ugen_name->s_name);
    }
    else {
        pd_error(x, "tap: too many arguments");
    }
}


static void ck_signal(t_ck* x, t_symbol* s)
{
    if (!x->chuck->vm()->globals_manager()->signalGlobalEvent(s->s_name))
        pd_error(x, "[ck_signal] signal global event '%s' failed", s->s_name);
}

static void ck_broadcast(t_ck* x, t_symbol* s)
{
    if (!x->chuck->vm()->globals_manager()->broadcastGlobalEvent(s->s_name))
        pd_error(x, "[ck_broadcast] broadcast global event '%s' failed",
                 s->s_name);
}


//-----------------------------------------------------------------------------------------------
// global event callback

void cb_event(const char* name) { post("cb_event: %s", name); }

//-----------------------------------------------------------------------------------------------
// global variable callbacks

void cb_get_all_global_vars(const std::vector<Chuck_Globals_TypeValue>& list,
                            void* data)
{
    post("cb_get_all_global_vars:");
    for (auto v : list) {
        post("type: %s name: %s", v.type.c_str(), v.name.c_str());
    }
}

void cb_get_int(const char* name, t_CKINT val)
{
    post("cb_get_int: name: %s value: %d", name, val);
}

void cb_get_float(const char* name, t_CKFLOAT val)
{
    post("cb_get_float: name: %s value: %f", name, val);
}

void cb_get_string(const char* name, const char* val)
{
    post("cb_get_string: name: %s value: %s", name, val);
}

void cb_get_int_array(const char* name, t_CKINT array[], t_CKUINT n)
{
    post("cb_get_int_array: name: %s size: %d", name, n);
    for (int i = 0; i < n; i++) {
        post("array[%d] = %d", i, array[i]);
    }
}

void cb_get_float_array(const char* name, t_CKFLOAT array[], t_CKUINT n)
{
    post("cb_get_float_array: name: %s size: %d", name, n);
    for (int i = 0; i < n; i++) {
        post("array[%d] = %d", i, array[i]);
    }
}

void cb_get_int_array_value(const char* name, t_CKINT value)
{
    post("cb_get_int_array_value: name: %s value: %d", name, value);
}

void cb_get_float_array_value(const char* name, t_CKFLOAT value)
{
    post("cb_get_float_array_value: name: %s value: %d", name, value);
}

void cb_get_assoc_int_array_value(const char* name, t_CKINT val)
{
    post("cb_get_assoc_int_array_value: name: %s value: %d", name, val);
}

void cb_get_assoc_float_array_value(const char* name, t_CKFLOAT val)
{
    post("cb_get_assoc_float_array_value: name: %s value: %f", name, val);
}


//-----------------------------------------------------------------------------------------------
// set/get chuck global variables


void ck_set(t_ck* x, t_symbol* s, long argc, t_atom* argv)
{
    if (argc < 3) {
        pd_error(x, (char*)"ck_set: too few # of arguments");
        return;
    }

    if (!(argv->a_type == A_SYMBOL && (argv + 1)->a_type == A_SYMBOL)) {
        pd_error(x, (char*)"ck_get: first two args must be symbols");
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
            long* long_array = (long*)getbytes(sizeof(long*) * length);
            for (int i = 0; i < length; i++) {
                post("set %s[%d] -> %d ", name->s_name, i,
                     atom_getint((argv + offset) + i));
                long_array[i] = atom_getint((argv + offset) + i);
            }
            if (x->chuck->vm()->globals_manager()->setGlobalIntArray(
                    name->s_name, long_array, length)) {
                freebytes(long_array, sizeof(long*) * length);
                return;
            }
        } else if (type == gensym("float[]")) { // list of doubles
            double* float_array = (double*)getbytes(sizeof(double*) * length);
            for (int i = 0; i < length; i++) {
                post("set %s[%d] -> %f ", name->s_name, i,
                     atom_getfloat((argv + offset) + i));
                float_array[i] = atom_getfloat((argv + offset) + i);
            }
            if (x->chuck->vm()->globals_manager()->setGlobalFloatArray(
                    name->s_name, float_array, length)) {
                freebytes(float_array, sizeof(double*) * length);
                return;
            }
        } else if (type == gensym("int[i]")) {
            long index = atom_getint((argv + 2));
            long value = atom_getint((argv + 3));
            if (x->chuck->vm()->globals_manager()->setGlobalIntArrayValue(
                    name->s_name, (t_CKUINT)index, (t_CKINT)value)) {
                post("set %s %d -> %d", name->s_name, index, value);
                return;
            }
        } else if (type == gensym("float[i]")) {
            long index = atom_getint((argv + 2));
            long value = atom_getfloat((argv + 3));
            if (x->chuck->vm()->globals_manager()->setGlobalFloatArrayValue(
                    name->s_name, (t_CKUINT)index, (t_CKFLOAT)value)) {
                post("set %s %d -> %f", name->s_name, index, value);
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
            long value = atom_getfloat((argv + 3));
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
        pd_error(x, (char*)"ck_get: invalid # of arguments");
        return;
    }

    if (!(argv->a_type == A_SYMBOL && (argv + 1)->a_type == A_SYMBOL)) {
        pd_error(x, (char*)"ck_get: first two args must be symbols");
        return;
    }

    t_symbol* type = atom_getsymbol(argv);
    t_symbol* name = atom_getsymbol(argv + 1);

    if (argc == 2) {
        if (type == gensym("int")) {
            if (x->chuck->vm()->globals_manager()->getGlobalInt(name->s_name,
                                                                cb_get_int))
                return;
        } else if (type == gensym("float")) {
            if (x->chuck->vm()->globals_manager()->getGlobalFloat(
                    name->s_name, cb_get_float))
                return;
        } else if (type == gensym("string")) {
            if (x->chuck->vm()->globals_manager()->getGlobalString(
                    name->s_name, cb_get_string))
                return;
        } else if (type == gensym("int[]")) {
            if (x->chuck->vm()->globals_manager()->getGlobalIntArray(
                    name->s_name, cb_get_int_array))
                return;
        } else if (type == gensym("float[]")) {
            if (x->chuck->vm()->globals_manager()->getGlobalFloatArray(
                    name->s_name, cb_get_float_array))
                return;
        }
        return;
    } else if (argc == 3) {
        if ((argv + 2)->a_type == A_FLOAT) {
            t_int index = atom_getint(argv + 2);
            if (type == gensym("int[]") || type == gensym("int[i]")) {
                if (x->chuck->vm()->globals_manager()->getGlobalIntArrayValue(
                        name->s_name, (t_CKUINT)index, cb_get_int_array_value))
                    return;
            } else if (type == gensym("float[]")
                       || type == gensym("float[i]")) {
                if (x->chuck->vm()
                        ->globals_manager()
                        ->getGlobalFloatArrayValue(name->s_name,
                                                   (t_CKUINT)index,
                                                   cb_get_float_array_value))
                    return;
            }
            return;
        } else if ((argv + 2)->a_type == A_SYMBOL) {
            t_symbol* key = atom_getsymbol(argv + 2);
            if (type == gensym("int[]") || type == gensym("int[k]")) {
                if (x->chuck->vm()
                        ->globals_manager()
                        ->getGlobalAssociativeIntArrayValue(
                            name->s_name, key->s_name,
                            cb_get_assoc_int_array_value))
                    return;
                ;
            } else if (type == gensym("float[]")
                       || type == gensym("float[k]")) {
                if (x->chuck->vm()
                        ->globals_manager()
                        ->getGlobalAssociativeFloatArrayValue(
                            name->s_name, key->s_name,
                            cb_get_assoc_float_array_value))
                    return;
                ;
            }
        }
    }
}

static void ck_listen(t_ck* x, t_symbol* s, t_float listen_forever)
{
    if (x->chuck->vm()->globals_manager()->listenForGlobalEvent(
            s->s_name, cb_event, (bool)listen_forever)) {
        post("listening to event %s", s->s_name);
    };
}

static void ck_unlisten(t_ck* x, t_symbol* s)
{
    if (x->chuck->vm()->globals_manager()->stopListeningForGlobalEvent(
            s->s_name, cb_event)) {
        post("stop listening to event %s", s->s_name);
    };
}


//-----------------------------------------------------------------------------------------------
// audio processing

static void ck_dsp(t_ck* x, t_signal** sp)
{
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
        x->tap_buffer = new float[x->buffer_size];
        memset(x->tap_buffer, 0.f, sizeof(float) * x->buffer_size);
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
    post("chuck~ - Executing perform routine");
    post("sample rate: %d", x->srate);
    post("buffer size: %i", x->buffer_size);
    post("channels: %d (+ %d tap outlets)", x->channels, x->tap_channels);
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
                // tap this outlet's UGen (mono)
                t_CKBOOL success = x->chuck->vm()->globals_manager()->getGlobalUGenSamples(
                    ugen_name->s_name, x->tap_buffer, n);

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
