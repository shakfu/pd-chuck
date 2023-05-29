#include "m_pd.h"

#include "chuck.h"
#include "chuck_globals.h"

#define N_IN_CHANNELS 2
#define N_OUT_CHANNELS 2
#define MAX_FILENAME 128


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


typedef struct _ck {
	t_object obj;
	t_float x_f;

	// general
	const char *currentdir;

	// chuck
	int buffer_size;  			// buffer size for for both input and output
    float *in_chuck_buffer;     // intermediate chuck input buffer
    float *out_chuck_buffer;    // intermediate chuck output buffer
	char *filepath[MAX_FILENAME];
	ChucK *chuck;
} t_ck;


static t_class *ck_class;



t_int *ck_perform(t_int *w)
{
    int i;
    t_ck        *x  = (t_ck *)(w[OBJECT]);
    float  *in1  = (float *)(w[INPUT_VECTOR_L]);
    float  *in2  = (float *)(w[INPUT_VECTOR_R]);
    float  *out1 = (float *)(w[OUTPUT_VECTOR_L]);
    float  *out2 = (float *)(w[OUTPUT_VECTOR_R]);
    int           n = (int)(w[VECTOR_SIZE]);
	
    float * in_ptr = x->in_chuck_buffer;
    float * out_ptr = x->out_chuck_buffer;

    for (i = 0; i < n; i++) {
        *(in_ptr++) = in1[i];
    }

    for (i = 0; i < n; i++) {
        *(in_ptr++) = in2[i];
    }

	x->chuck->run(x->in_chuck_buffer, x->out_chuck_buffer, n);

    for (i = 0; i < n; i++) {
            out1[i] = *(out_ptr++);
    }
	
    for (i = 0; i < n; i++) {
            out2[i] = *(out_ptr++);
    }
	
	/* Return the next address in the DSP chain */
	return w + NEXT;
}



void ck_dsp(t_ck *x, t_signal **sp, short *count)
{
    // post("ins : %lx %lx",  sp[0]->s_vec, sp[1]->s_vec);
    // post("outs : %lx %lx", sp[2]->s_vec, sp[3]->s_vec);

    delete[] x->in_chuck_buffer;
    delete[] x->out_chuck_buffer;

    x->buffer_size = sp[0]->s_n;

    x->in_chuck_buffer = new float[x->buffer_size * N_IN_CHANNELS];
    x->out_chuck_buffer = new float[x->buffer_size * N_OUT_CHANNELS];

    memset(x->in_chuck_buffer, 0.f, sizeof(float) * x->buffer_size * N_IN_CHANNELS);
    memset(x->out_chuck_buffer, 0.f, sizeof(float) * x->buffer_size * N_OUT_CHANNELS);

	/* Attach the object to the DSP chain */
	dsp_add(ck_perform, NEXT-1, x, sp[0]->s_vec, sp[1]->s_vec, 
								   sp[2]->s_vec, sp[3]->s_vec, sp[0]->s_n);
	
	/* Print message to Max window */
	post("chuck~ • Executing perform routine");
	post("buffer_size: %i", x->buffer_size);
}


void *ck_new(void)
{
	/* Instantiate a new object */
	t_ck *x = (t_ck *) pd_new(ck_class);

	if (x) {

		/* Create signal inlets */		
		// Pd creates one by default			// Input Left
		signalinlet_new((t_object *)x, 0); 		// Input Right
		
		/* Create signal outlets */
        outlet_new((t_object *)x, &s_signal); 	// Output Left
        outlet_new((t_object *)x, &s_signal); 	// Output Right

		// initial inits
		x->currentdir = canvas_getcurrentdir()->s_name;

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

		/* Print message to Max window */
		post("ChucK %s", x->chuck->version());
		post("chuck~ • Object was created");
	}
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
