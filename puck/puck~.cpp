
#include "m_pd.h"

#include "chuck.h"

#define GAIN 0.5
#define SAMPLE float
#define MY_SRATE 44100
#define MY_CHANNELS 2
#define MAX_FILENAME 128



typedef struct _puck {
	t_object obj;
	t_float x_f;

	// chuck
	float srate;
	int n_channels;

	int outbuf_size;  // chuck uses the same buffer for both input and output
	// space for these malloc'd in chuck_dsp()
	float *pd_outbuf;

	char *filepath[MAX_FILENAME];
	ChucK *the_chuck;
} t_puck;


static t_class *puck_class;



enum DSP {
	PERFORM, 
	OBJECT, 
	INPUT_VECTOR, 
	OUTPUT_VECTOR, 
	VECTOR_SIZE, 
	NEXT
};


//-----------------------------------------------------------------------------
// name: callme()
// desc: audio callback
//-----------------------------------------------------------------------------
// int callme( void * outputBuffer, void * inputBuffer, unsigned int numFrames,
//             double streamTime, RtAudioStreamStatus status, void * data )
// {
//     // cast!
//     SAMPLE * input = (SAMPLE *)inputBuffer;
//     SAMPLE * output = (SAMPLE *)outputBuffer;
    
//     // --------------------------- ChucK -------------------------------- //
//     // compute chuck!
//     the_chuck->run( input, output, numFrames );
//     // ------------------------------------------------------------------- //
    
//     return 0;
// }


t_int *puck_perform(t_int *w)
{
	/* Copy the object pointer */
	t_puck *x = (t_puck *)w[OBJECT];
	
	/* Copy signal pointers */
	t_float *in = (t_float *)w[INPUT_VECTOR];
	t_float *out = (t_float *)w[OUTPUT_VECTOR];
	
	/* Copy the signal vector size */
	t_int n = w[VECTOR_SIZE];
	
	/* Perform the DSP loop */
	while (n--) {
		// x->the_chuck->run(in, out, n);
		*out++ = GAIN * *in++;
	}
	
	/* Return the next address in the DSP chain */
	return w + NEXT;
}


void puck_dsp(t_puck *x, t_signal **sp, short *count)
{
	/* Attach the object to the DSP chain */
	dsp_add(puck_perform, NEXT-1, x, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
	
	/* Print message to Max window */
	post("chuck~ • Executing perform routine");
}


void *puck_new(void)
{
	/* Instantiate a new object */
	t_puck *x = (t_puck *) pd_new(puck_class);


    x->the_chuck = NULL;

    x->the_chuck = new ChucK();
    // set sample rate and number of in/out channels on our chuck
    x->the_chuck->setParam( CHUCK_PARAM_SAMPLE_RATE, MY_SRATE );
    x->the_chuck->setParam( CHUCK_PARAM_INPUT_CHANNELS, MY_CHANNELS );
    x->the_chuck->setParam( CHUCK_PARAM_OUTPUT_CHANNELS, MY_CHANNELS );
    x->the_chuck->setParam( CHUCK_PARAM_WORKING_DIRECTORY, "." );

    // initialize our chuck
    x->the_chuck->init();

    // print message
    post("chuck-embed example running 'sine.ck'...");
    // run a chuck program from file
    x->the_chuck->compileFile( "sine.ck", "", 1 );

	/* Create signal inlets */
	// Pd creates one by default
	
	/* Create signal outlets */
	outlet_new(&x->obj, gensym("signal"));
	
	/* Print message to Max window */
	post("chuck~ • Object was created");
	
	/* Return a pointer to the new object */
	return x;
}


void puck_free(t_puck *x)
{
	// Nothing to free
	// global_cleanup();
	/* Print message to Max window */
	post("chuck~ • Memory was freed");
}


extern "C" {

void puck_tilde_setup(void)
{
	/* Initialize the class */
	puck_class = class_new(gensym("puck~"),
							 (t_newmethod)puck_new,
							 (t_method)puck_free,
							 sizeof(t_puck), 0, A_NULL);
	
	/* Specify signal input, with automatic float to signal conversion */
	CLASS_MAINSIGNALIN(puck_class, t_puck, x_f);
	
	/* Bind the DSP method, which is called when the DACs are turned on */
	class_addmethod(puck_class, (t_method)puck_dsp, gensym("dsp"), A_NULL);
	
	/* Print message to Max window */
	post("chuck~ • External was loaded");
}

}
