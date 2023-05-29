


#include "m_pd.h"

#include "chuck.h"
#include "chuck_globals.h"

#define GAIN 0.5
#define SAMPLE float
#define MY_SRATE 44100
#define MAX_FILENAME 128
#define N_IN_CHANNELS 1
#define N_OUT_CHANNELS 1


typedef struct _ck {
	t_object obj;
	t_float x_f;

	// general
	const char *currentdir;


	// chuck
	float srate;
	int n_channels;
	int buffer_size;  // chuck uses the same buffer for both input and output
    float *in_chuck_buffer;     // intermediate chuck input buffer
    float *out_chuck_buffer;    // intermediate chuck output buffer
	char *filepath[MAX_FILENAME];
	ChucK *chuck;
} t_ck;


static t_class *ck_class;



enum DSP {
	PERFORM, 
	OBJECT, 
	INPUT_VECTOR, 
	OUTPUT_VECTOR, 
	VECTOR_SIZE, 
	NEXT
};



void ck_perform64(t_ck *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, 
                           long sampleframes, long flags, void *userparam)
{
    float * in_ptr = x->in_chuck_buffer;
    float * out_ptr = x->out_chuck_buffer;
    int n = sampleframes; // n = 64

    if (ins) {
        for (int i = 0; i < n; i++) {
            for (int chan = 0; chan < numins; chan++) {
                *(in_ptr++) = ins[chan][i];
            }
        }
    }

    x->chuck->run(x->in_chuck_buffer, x->out_chuck_buffer, n);

    for (int i = 0; i < n; i++) {
        for (int chan = 0; chan < numouts; chan++) {
            outs[chan][i] = *out_ptr++;
        }
    }
}



t_int *ck_perform(t_int *w)
{
	/* Copy the object pointer */
	t_ck *x = (t_ck *)w[OBJECT];

	/* Copy the signal vector size */
	t_int n = w[VECTOR_SIZE];
	
    float * in_ptr = x->in_chuck_buffer;
    float * out_ptr = x->out_chuck_buffer;

	/* Copy signal pointers */
	t_float *ins = (t_float *)w[INPUT_VECTOR];
	t_float *outs = (t_float *)w[OUTPUT_VECTOR];
	

    // int n = sampleframes; // n = 64

    if (ins) {
        for (int i = 0; i < n; i++) {
            *(in_ptr++) = ins[i];
        }
    }

    x->chuck->run(x->in_chuck_buffer, x->out_chuck_buffer, n);

    for (int i = 0; i < n; i++) {
            outs[i] = *out_ptr++;
    }
	
	/* Return the next address in the DSP chain */
	return w + NEXT;
}


void ck_dsp(t_ck *x, t_signal **sp, short *count)
{
    delete[] x->in_chuck_buffer;
    delete[] x->out_chuck_buffer;

    x->buffer_size = sp[0]->s_n;

    x->in_chuck_buffer = new float[x->buffer_size * N_IN_CHANNELS];
    x->out_chuck_buffer = new float[x->buffer_size * N_OUT_CHANNELS];

    memset(x->in_chuck_buffer, 0.f, sizeof(float) * x->buffer_size * N_IN_CHANNELS);
    memset(x->out_chuck_buffer, 0.f, sizeof(float) * x->buffer_size * N_OUT_CHANNELS);

	/* Attach the object to the DSP chain */
	dsp_add(ck_perform, NEXT-1, x, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
	
	/* Print message to Max window */
	post("chuck~ • Executing perform routine");
	post("buffer_size: %i", x->buffer_size);
}


void *ck_new(void)
{
	/* Instantiate a new object */
	t_ck *x = (t_ck *) pd_new(ck_class);


	// initial inits
	x->currentdir = canvas_getcurrentdir()->s_name;
	// x->num_outputs = MY_CHANNELS;

	// chuck-related inits
    x->in_chuck_buffer = NULL;
    x->out_chuck_buffer = NULL;

    x->chuck = new ChucK();
    // set sample rate and number of in/out channels on our chuck
    x->chuck->setParam( CHUCK_PARAM_SAMPLE_RATE, (t_CKINT) sys_getsr() );
    x->chuck->setParam( CHUCK_PARAM_INPUT_CHANNELS, (t_CKINT) N_IN_CHANNELS );
    x->chuck->setParam( CHUCK_PARAM_OUTPUT_CHANNELS, (t_CKINT) N_OUT_CHANNELS );
    x->chuck->setParam( CHUCK_PARAM_WORKING_DIRECTORY, x->currentdir );

    // initialize our chuck
    x->chuck->init();
    x->chuck->start();

	/* Create signal inlets */
	// Pd creates one by default
	
	/* Create signal outlets */
	outlet_new(&x->obj, gensym("signal"));
	
	/* Print message to Max window */
	post("ChucK %s", x->chuck->version());
	post("chuck~ • Object was created");
	
	/* Return a pointer to the new object */
	return x;
}


void ck_free(t_ck *x)
{
    delete[] x->in_chuck_buffer;
    delete[] x->out_chuck_buffer;
	delete x->chuck;
	post("chuck~ • Memory was freed");
}


void ck_bang(t_ck *x)
{
	std::string filename ("sine.ck");
	std::string filepath = std::string(x->currentdir) + "/" + filename;

	post("filename: %s", filename.c_str());

    if (!x->chuck->compileFile(filepath, "", 1)) {
        pd_error(x, "compilation error! : %s", filepath.c_str());
    }
}


void ck_send_chuck_vm_msg(t_ck* x, Chuck_Msg_Type msg_type)
{    
    Chuck_Msg * msg = new Chuck_Msg;
    msg->type = msg_type;
    
    // null reply so that VM will delete for us when it's done
    msg->reply = ( ck_msg_func )NULL;
    
    x->chuck->vm()->globals_manager()->execute_chuck_msg_with_globals(msg);

}

void ck_reset(t_ck *x)
{
    post("reset vm");
	ck_send_chuck_vm_msg(x, MSG_CLEARGLOBALS);
    ck_send_chuck_vm_msg(x, MSG_CLEARVM);
}



extern "C" {

void chuck_tilde_setup(void)
{
	/* Initialize the class */
	ck_class = class_new(gensym("chuck~"),
							 (t_newmethod)ck_new,
							 (t_method)ck_free,
							 sizeof(t_ck), 0, A_NULL);
	
	/* Specify signal input, with automatic float to signal conversion */
	CLASS_MAINSIGNALIN(ck_class, t_ck, x_f);
	
	/* Bind the DSP method, which is called when the DACs are turned on */
	class_addmethod(ck_class, (t_method)ck_dsp, gensym("dsp"), A_CANT, A_NULL);

	class_addmethod(ck_class, (t_method)ck_reset, gensym("reset"), A_NULL);

	class_addbang(ck_class, ck_bang);

	// set the alias to external
    class_addcreator((t_newmethod)ck_new, gensym("chuck~"), A_NULL);

    // set name of default help file
    class_sethelpsymbol(ck_class, gensym("help-ck"));

	/* Print message to Max window */
	// post("chuck~ • External was loaded");
}

}
