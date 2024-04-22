#include "m_pd.h"

#include "chuck.h"
#include "chuck_globals.h"

#include <string>
#include <unordered_map>

#include <libgen.h>
#include <unistd.h>

// globals defs
#define LOG_LEVEL CK_LOG_SYSTEM // chuck log levels 0-10 (default: 2)
#define N_IN_CHANNELS 2
#define N_OUT_CHANNELS 2

static int PD_CK_COUNT = 0;

enum DSP {
    PERFORM,
    OBJECT,
    INPUT_VECTOR_L,
    INPUT_VECTOR_R,
    OUTPUT_VECTOR_L,
    OUTPUT_VECTOR_R,
    VECTOR_SIZE,
    NEXT
};

// typedefs
typedef void (*ck_callback)(void);
typedef std::unordered_map<std::string, ck_callback> callback_map;

// object struct
typedef struct _ck {
	t_object obj;
	t_float x_f;

    int oid;                    // object id
    int srate;                  // sample rate
    int loglevel;               // loglevel
    callback_map cb_map;        // callback map<string,callback>

	// chuck
	int buffer_size;  			// buffer size for for both input and output
    float *in_chuck_buffer;     // intermediate chuck input buffer
    float *out_chuck_buffer;    // intermediate chuck output buffer
    t_symbol *current_dir;
    char examples_dir[MAXPDSTRING];
	t_symbol *filename;
	ChucK *chuck;

	// inlets & outlets (default inlet is inL)
	t_inlet *x_inR;
	t_outlet*x_outL;
	t_outlet*x_outR;
} t_ck;


// core
void *ck_new(t_symbol *s);
void ck_free(t_ck *x);
extern "C" void chuck_tilde_setup(void);

// helpers
void ck_stdout_print(const char* msg);
void ck_stderr_print(const char* msg);
void ck_run_file(t_ck *x);
void ck_compile_file(t_ck *x, const char *filename);
void ck_send_chuck_vm_msg(t_ck* x, Chuck_Msg_Type msg_type);

// general message handlers
void ck_bang(t_ck *x);                     // (re)load chuck file
void ck_anything(t_ck *x, t_symbol *s, int argc, t_atom *argv); // set global params by name, value

// chuck vm message handlers
void ck_add(t_ck* x, t_symbol* s); // add shred from file
void ck_run(t_ck* x, t_symbol* s); // alias of add, run chuck file
void ck_remove(t_ck* x, t_symbol* s, long argc, t_atom* argv);  // remove shreds (all, last, by #)
void ck_replace(t_ck* x, t_symbol* s, long argc, t_atom* argv); // replace shreds 
void ck_clear(t_ck* x, t_symbol* s, long argc, t_atom* argv);   // clear_vm, clear_globals
void ck_reset(t_ck* x, t_symbol* s, long argc, t_atom* argv);   // clear_vm, reset_id
void ck_status(t_ck* x); // get info about running shreds
void ck_time(t_ck* x);

// special message handlers
void ck_info(t_ck* x);                     // get info about running shreds
// void ck_reset(t_ck* x);                    // remove all shreds and clean vm
void ck_signal(t_ck* x, t_symbol* s);      // signal global event
void ck_broadcast(t_ck* x, t_symbol* s);   // broadcast global event
void ck_loglevel(t_ck* x, t_symbol* s, long argc, t_atom* argv); 

// audio processing
void ck_dsp(t_ck *x, t_signal **sp);
t_int *ck_perform(t_int *w);

// callback registeration
void ck_register(t_ck* x, t_symbol* s);
void ck_unregister(t_ck* x, t_symbol* s);


// global class instance
static t_class *ck_class;

//-----------------------------------------------------------------------------------------------
// callbacks

/* nothing useful here yet */

void cb_demo(void)
{
    post("==> demo callback is called!");
}

//-----------------------------------------------------------------------------------------------
// initialization / destruction

void *ck_new(t_symbol *s)
{
	/* Instantiate a new object */
	t_ck *x = (t_ck *) pd_new(ck_class);

	if (x) {

		/* Create signal inlets */		
		// Pd creates one by default				   		// Input Left
		x->x_inR = signalinlet_new((t_object *)x, 0); 		// Input Right
		
		/* Create signal outlets */
        x->x_outL = outlet_new((t_object *)x, &s_signal); 	// Output Left
        x->x_outR = outlet_new((t_object *)x, &s_signal); 	// Output Right

		// initial inits
        x->current_dir = canvas_getcurrentdir();
        snprintf(x->examples_dir, MAXPDSTRING, "%s/examples", x->current_dir->s_name);
        std:string examples_dir = std::string(x->examples_dir);

		x->filename = s;
		x->x_f = 0.0;

		// chuck-related inits
	    x->in_chuck_buffer = NULL;
	    x->out_chuck_buffer = NULL;

	    x->chuck = new ChucK();
	    // set sample rate and number of in/out channels on our chuck
        x->srate = sys_getsr();
        
	    x->chuck->setParam(CHUCK_PARAM_SAMPLE_RATE, (t_CKINT) x->srate);
        x->chuck->setParam(CHUCK_PARAM_INPUT_CHANNELS, (t_CKINT) N_IN_CHANNELS);
	    x->chuck->setParam(CHUCK_PARAM_OUTPUT_CHANNELS, (t_CKINT) N_OUT_CHANNELS);
	    x->chuck->setParam(CHUCK_PARAM_WORKING_DIRECTORY, x->examples_dir);
        x->chuck->setParam(CHUCK_PARAM_VM_HALT, (t_CKINT) 0);
        x->chuck->setParam(CHUCK_PARAM_DUMP_INSTRUCTIONS, (t_CKINT) 0);
        // directory for compiled code
        // std::string global_dir = std::string(x->examples_dir);
        x->chuck->setParam( CHUCK_PARAM_WORKING_DIRECTORY, examples_dir);
        std::list<std::string> chugin_search;
        chugin_search.push_back(examples_dir + "/chugins");
        x->chuck->setParam( CHUCK_PARAM_USER_CHUGIN_DIRECTORIES, chugin_search);
        x->chuck->setStdoutCallback(ck_stdout_print);
        x->chuck->setStderrCallback(ck_stderr_print);
        x->oid = ++PD_CK_COUNT;
        x->loglevel = LOG_LEVEL;

        // init cb_map and register callbacks
        x->cb_map = callback_map();
        x->cb_map.emplace("demo", &cb_demo);

	    // initialize our chuck
	    x->chuck->init();
	    x->chuck->start();

        // set chuck log level
        ChucK::setLogLevel(x->loglevel);

		/* Print message to Max window */
		post("ChucK %s", x->chuck->version());
		post("chuck~ • Object was created");
	}
	return (void *)x;
}


void ck_free(t_ck *x)
{
    delete[] x->in_chuck_buffer;
    delete[] x->out_chuck_buffer;
	delete x->chuck;
	/* free inlet resources */
	inlet_free(x->x_inR);

	/* free any resources associated with the given outlet */
 	outlet_free(x->x_outL);
 	outlet_free(x->x_outR);

	post("chuck~ • Memory was freed");
}


extern "C" void chuck_tilde_setup(void)
{
	/* Initialize the class */
	ck_class = class_new(gensym("chuck~"),
		(t_newmethod)ck_new,
		(t_method)ck_free,
		sizeof(t_ck),
		CLASS_DEFAULT,
		A_DEFSYMBOL,
		A_NULL);

	/* Specify signal input, with automatic float to signal conversion */
	CLASS_MAINSIGNALIN(ck_class, t_ck, x_f);
	
	/* Bind the DSP method, which is called when the DACs are turned on */
	class_addmethod(ck_class, (t_method)ck_dsp, 	   gensym("dsp"),        A_CANT, A_NULL);

    class_addmethod(ck_class, (t_method)ck_add,        gensym("add"),        A_SYMBOL,    A_NULL);
    class_addmethod(ck_class, (t_method)ck_run,        gensym("run"),        A_SYMBOL,    A_NULL);
    class_addmethod(ck_class, (t_method)ck_signal,     gensym("sig"),        A_SYMBOL,    A_NULL);
    class_addmethod(ck_class, (t_method)ck_broadcast,  gensym("broadcast"),  A_SYMBOL,    A_NULL);
    class_addmethod(ck_class, (t_method)ck_register,   gensym("register"),   A_SYMBOL,    A_NULL);
    class_addmethod(ck_class, (t_method)ck_unregister, gensym("unregister"), A_SYMBOL,    A_NULL);

    class_addmethod(ck_class, (t_method)ck_remove,     gensym("remove"),     A_GIMME,     A_NULL);
    class_addmethod(ck_class, (t_method)ck_replace,    gensym("replace"),    A_GIMME,     A_NULL);
    class_addmethod(ck_class, (t_method)ck_clear,      gensym("clear"),      A_GIMME,     A_NULL);
    class_addmethod(ck_class, (t_method)ck_reset,      gensym("reset"),      A_GIMME,     A_NULL);
    class_addmethod(ck_class, (t_method)ck_loglevel,   gensym("loglevel"),   A_GIMME,     A_NULL);
    class_addmethod(ck_class, (t_method)ck_info,       gensym("info"),                    A_NULL);
    class_addmethod(ck_class, (t_method)ck_status,     gensym("status"),                  A_NULL);
    class_addmethod(ck_class, (t_method)ck_time,       gensym("time"),                    A_NULL);

    class_addbang(ck_class,   (t_method)ck_bang);
    class_addanything(ck_class, (t_method)ck_anything);

    // set name of default help file
    class_sethelpsymbol(ck_class, gensym("help-chuck"));
}


//-----------------------------------------------------------------------------------------------
// helpers

void ck_stdout_print(const char* msg)
{
    post("-> %s", msg);
}


void ck_stderr_print(const char* msg)
{
    post("=> %s", msg);
}


void ck_send_chuck_vm_msg(t_ck *x, Chuck_Msg_Type msg_type)
{
    Chuck_Msg *msg = new Chuck_Msg;
    msg->type = msg_type;

    // null reply so that VM will delete for us when it's done
    msg->reply_cb = (ck_msg_func)NULL;

    x->chuck->vm()->globals_manager()->execute_chuck_msg_with_globals(msg);
}

void ck_compile_file(t_ck *x, const char *filename)
{
    post("compile: %s", filename);
    if (!x->chuck->compileFile(std::string(filename), "", 1)) {
        pd_error(x, "compilation error!: %s", filename);
    }
}

void ck_run_file(t_ck *x)
{
    char filepath[MAXPDSTRING];

    if (x->filename != gensym("")) {
        if (access(x->filename->s_name, F_OK) == 0) { // file exists in path
            ck_compile_file(x, x->filename->s_name);
        } else { // try in the examples folder
            snprintf(filepath, MAXPDSTRING, "%s/%s", x->examples_dir, x->filename->s_name);
            // post("filepath: %s", filepath);
            ck_compile_file(x, filepath);
        }
    }
}


t_symbol* ck_get_loglevel_name(long level)
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

void ck_loglevel(t_ck* x, t_symbol* s, long argc, t_atom* argv)
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
                pd_error(x, "out-of-range: defaulting to level 2. loglevel must be between 0-10 inclusive");
                ChucK::setLogLevel(CK_LOG_SYSTEM);
                return;
            }
        }
    }
    pd_error(x, "could not get or set loglevel");
}

void ck_bang(t_ck *x) { ck_run_file(x); }

void ck_anything(t_ck *x, t_symbol *s, int argc, t_atom *argv)
{
    // TODO:
    //  - should check set op (true if succeed)
    //  - handle case of 2 length array (maybe)

    if (s == gensym("") || argc == 0) {
        goto error;
    }

    // set '+' as shorthand for ck_add method
    if (s == gensym("+")) {
        ck_add(x, atom_getsymbol(argv));
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
            t_symbol *p_sym = atom_getsymbol(argv);
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
            double *float_array = (double *)getbytes(sizeof(double *) * argc);
            for (int i = 0; i < argc; i++) {
                float_array[i] = atom_getfloat(argv + i);
            }
            x->chuck->vm()->globals_manager()->setGlobalFloatArray(
                s->s_name, float_array, argc);
            freebytes(float_array, sizeof(double *) * argc);
        }
    }

    if (argc == 2) {                   // <param-name> <index|key> <value
        if (argv->a_type == A_FLOAT) { // int index
            int index = atom_getint(argv);
            float p_float = atom_getfloat(argv + 1);
            x->chuck->vm()->globals_manager()->setGlobalFloatArrayValue(
                s->s_name, index, p_float);

        } else if (argv->a_type == A_SYMBOL) { // key/value
            t_symbol *key = atom_getsymbol(argv);
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
// special message handlers

void ck_info(t_ck *x)
{
    Chuck_VM_Shreduler * shreduler = x->chuck->vm()->shreduler();
    std::vector<Chuck_VM_Shred *> shreds;
    shreduler->get_all_shreds(shreds);
    post("\nRUNNING SHREDS:");
    for(const Chuck_VM_Shred* i : shreds) {
        post("%d-%d: %s", x->oid, i->get_id(), i->name.c_str());
    }
    post("");
}

// void ck_reset(t_ck *x)
// {
//     post("reset vm");
//     ck_send_chuck_vm_msg(x, CK_MSG_CLEARGLOBALS);
//     ck_send_chuck_vm_msg(x, CK_MSG_CLEARVM);
// }

void ck_add(t_ck *x, t_symbol *s)
{
    if (s != gensym("")) {
        post("filename: %s", s->s_name);
        x->filename = s;
        ck_run_file(x);
    }
}

void ck_run(t_ck *x, t_symbol *s)
{
    ck_add(x, s);
}

void ck_remove(t_ck *x, t_symbol *s, long argc, t_atom *argv)
{
    post("%s: %d", s->s_name, argc);

    Chuck_Msg *msg = new Chuck_Msg;

    if (argc == 1) {

        if (argv->a_type == A_FLOAT) {
            int shred_id = atom_getint(argv);
            msg->type = CK_MSG_REMOVE;
            msg->param = shred_id;

        } else if (argv->a_type == A_SYMBOL) {

            t_symbol *cmd = atom_getsymbol(argv);

            if (cmd == gensym("all")) {
                msg->type = CK_MSG_REMOVEALL;

            } else if (cmd == gensym("last")) {
                msg->type = CK_MSG_REMOVE;
                msg->param = 0xffffffff;
            }
        }

    } else {
        // default to last
        msg->type = CK_MSG_REMOVE;
        msg->param = 0xffffffff;
    }

    msg->reply_cb = (ck_msg_func)0;
    x->chuck->vm()->queue_msg(msg, 1);
}


void ck_replace(t_ck* x, t_symbol* s, long argc, t_atom* argv)
{
    long shred_id;
    t_symbol *filename_sym = NULL;

    if (argc < 2) {
        pd_error(x, "ck_replace: two arguments are required");
        return;
    }
    if (argv->a_type != A_FLOAT) {
        pd_error(x, "first argument must a shred id number");
        return;
    }
    shred_id = (long)atom_getfloat(argv);

    if ((argv+1)->a_type != A_SYMBOL) {
        pd_error(x, "second argument must be a symbol");
        return;
    }
    filename_sym = atom_getsymbol(argv+1);

    // get string
    std::string path = std::string(filename_sym->s_name);
    // filename
    std::string filename;
    // arguments
    std::string args;
    // extract args FILE:arg1:arg2:arg3
    extract_args( path, filename, args );

    std::string full_path = std::string(x->examples_dir) + "/" + filename; // not portable

    // compile but don't run yet (instance == 0)
    if( !x->chuck->compileFile( full_path, args, 0 ) ) {
        pd_error(x, "could not compile file");
        return;
    }

    // construct chuck msg (must allocate on heap, as VM will clean up)
    Chuck_Msg * msg = new Chuck_Msg();
    // set type
    msg->type = CK_MSG_REPLACE;
    // set shred id to replace
    msg->param = shred_id;
    // set code for incoming shred
    msg->code = x->chuck->vm()->carrier()->compiler->output();
    // create args array
    msg->args = new vector<string>;
    // extract args again but this time into vector
    extract_args( path, filename, *(msg->args) );
    // process REPLACE message, return new shred ID
    x->chuck->vm()->process_msg( msg );
}

void ck_clear(t_ck* x, t_symbol* s, long argc, t_atom* argv)
{
    if (argc == 0) {
        return ck_send_chuck_vm_msg(x, CK_MSG_CLEARVM);
    }

    if (argc == 1) {
        if (argv->a_type == A_SYMBOL) {
            t_symbol* target = atom_getsymbol(argv);
            if (target == gensym("globals")) {
                post("=> [chuck]: clean up global variables without clearing the whole VM");
                return ck_send_chuck_vm_msg(x, CK_MSG_CLEARGLOBALS);
            } 
            if (target == gensym("vm")) {
                return ck_send_chuck_vm_msg(x, CK_MSG_CLEARVM); 
            }
        }
    }
    pd_error(x, "must be 'clear globals' or 'clear vm'");
}

void ck_reset(t_ck* x, t_symbol* s, long argc, t_atom* argv)
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


void ck_status(t_ck* x)
{
    Chuck_VM_Shreduler* shreduler = x->chuck->vm()->shreduler();
    shreduler->status();
}

void ck_time(t_ck* x)
{
    return ck_send_chuck_vm_msg(x, CK_MSG_TIME);
}


void ck_signal(t_ck *x, t_symbol *s)
{
    if (!x->chuck->vm()->globals_manager()->signalGlobalEvent(s->s_name))
        pd_error(x, "[ck_signal] signal global event '%s' failed", s->s_name);
}

void ck_broadcast(t_ck *x, t_symbol *s)
{
    if (!x->chuck->vm()->globals_manager()->broadcastGlobalEvent(s->s_name))
        pd_error(x, "[ck_broadcast] broadcast global event '%s' failed",
                 s->s_name);
}


void ck_register(t_ck* x, t_symbol* s)
{
    if (!x->cb_map.count(s->s_name)) {
        pd_error(x, "event/callback not found: %s", s->s_name);
        return;
    }
    std::string key = std::string(s->s_name);
    ck_callback cb = x->cb_map[key];
    // false: for a one off call, true: called everytime it is called
    if (x->chuck->vm()->globals_manager()->listenForGlobalEvent(s->s_name, cb, false)) {
        post("%s event/callback registered", s->s_name);
    };
}


void ck_unregister(t_ck* x, t_symbol* s)
{
    if (!x->cb_map.count(s->s_name)) {
        pd_error(x, "event/callback not found: %s", s->s_name);
        return;
    }
    std::string key = std::string(s->s_name);
    ck_callback cb = x->cb_map[key];
    if (x->chuck->vm()->globals_manager()->stopListeningForGlobalEvent(s->s_name, cb)) {
        post("%s event/callback unregistered", s->s_name);
    };
}

//-----------------------------------------------------------------------------------------------
// audio processing

void ck_dsp(t_ck *x, t_signal **sp)
{
    delete[] x->in_chuck_buffer;
    delete[] x->out_chuck_buffer;

    x->buffer_size = sp[0]->s_n;

    x->in_chuck_buffer = new float[x->buffer_size * N_IN_CHANNELS];
    x->out_chuck_buffer = new float[x->buffer_size * N_OUT_CHANNELS];

    memset(x->in_chuck_buffer, 0.f,
           sizeof(float) * x->buffer_size * N_IN_CHANNELS);
    memset(x->out_chuck_buffer, 0.f,
           sizeof(float) * x->buffer_size * N_OUT_CHANNELS);

    /* Attach the object to the DSP chain */
    dsp_add(ck_perform, NEXT - 1, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec,
            sp[3]->s_vec, sp[0]->s_n);

    /* Print message to Max window */
    post("chuck~ • Executing perform routine");
    post("sample rate: %d", x->srate);
    post("buffer size: %i", x->buffer_size);
}

t_int *ck_perform(t_int *w)
{
    // Thanks to Professor GE Wang for the fix!
    int i;
    t_ck *x = (t_ck *)(w[OBJECT]);
    float *in1 = (float *)(w[INPUT_VECTOR_L]);
    float *in2 = (float *)(w[INPUT_VECTOR_R]);
    float *out1 = (float *)(w[OUTPUT_VECTOR_L]);
    float *out2 = (float *)(w[OUTPUT_VECTOR_R]);
    int n = (int)(w[VECTOR_SIZE]);

    float *in_ptr = x->in_chuck_buffer;
    float *out_ptr = x->out_chuck_buffer;

    // interleave
    for (i = 0; i < n; i++) {
        *in_ptr = in1[i];
        in_ptr += N_IN_CHANNELS;
    }
    // set in_ptr to input with offset
    in_ptr = x->in_chuck_buffer+1;
    for (i = 0; i < n; i++) {
        *in_ptr = in2[i];
        in_ptr += N_IN_CHANNELS;
    }

    // NB pd non-interleaved; chuck interleaved by default
    x->chuck->run(x->in_chuck_buffer, x->out_chuck_buffer, n);

    // de-interleave
    out_ptr = x->out_chuck_buffer;
    for (i = 0; i < n; i++) {
        out1[i] = *out_ptr;
        out_ptr += N_OUT_CHANNELS;
    }
    // set out_ptr to output with offset
    out_ptr = x->out_chuck_buffer+1;
    for (i = 0; i < n; i++) {
        out2[i] = *out_ptr;
        out_ptr += N_OUT_CHANNELS;
    }

    /* Return the next address in the DSP chain */
    return w + NEXT;
}
