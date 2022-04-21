
#include "m_pd.h"

#include "chuck.h"

#define GAIN 0.5
#define SAMPLE float
#define MY_SRATE 44100
#define MY_CHANNELS 2


static t_class *puck_tilde_class;

enum DSP {
  PERFORM, 
  OBJECT, 
  INPUT_VECTOR_1,
  INPUT_VECTOR_2,
  OUTPUT_VECTOR, 
  VECTOR_SIZE, 
  NEXT
};

typedef struct _puck_tilde {
  t_object x_obj;
  t_float x_pan;
  t_float f;

  const char *currentdir;
  ChucK *the_chuck;

  t_inlet *x_in2;
  t_inlet *x_in3;
  t_outlet*x_out;
} t_puck_tilde;


/**
 * this is the core of the object
 * this perform-routine is called for each signal block
 * the name of this function is arbitrary and is registered to Pd in the
 * puck_tilde_dsp() function, each time the DSP is turned on
 *
 * the argument to this function is just a pointer within an array
 * we have to know for ourselves how many elements inthis array are
 * reserved for us (hint: we declare the number of used elements in the
 * puck_tilde_dsp() at registration
 *
 * since all elements are of type "t_int" we have to cast them to whatever
 * we think is apropriate; "apropriate" is how we registered this function
 * in puck_tilde_dsp()
 */
t_int *puck_tilde_perform(t_int *w)
{
  /* the first element is a pointer to the dataspace of this object */
  t_puck_tilde *x = (t_puck_tilde *)(w[OBJECT]);
  /* here is a pointer to the t_sample arrays that hold the 2 input signals */
  t_sample    *in1 =      (t_sample *)(w[INPUT_VECTOR_1]);
  t_sample    *in2 =      (t_sample *)(w[INPUT_VECTOR_2]);
  /* here comes the signalblock that will hold the output signal */
  t_sample    *out =      (t_sample *)(w[OUTPUT_VECTOR]);
  /* all signalblocks are of the same length */
  int            n =             (int)(w[VECTOR_SIZE]);
  /* get (and clip) the mixing-factor */
  t_sample pan = (x->x_pan<0)?0.0:(x->x_pan>1)?1.0:x->x_pan;
  /* just a counter */
  int i;

  /* this is the main routine:
   * mix the 2 input signals into the output signal
   */
  for(i=0; i<n; i++)
    {
      out[i]=in1[i]*(1-pan)+in2[i]*pan;
    }

  // while (n--) {
  //   x->the_chuck->run(in1, out, n); // <= FIXME: NO AUDIO + CRASHES PD!!
  //   // *out++ = GAIN * *in++;
  // }


  /* return a pointer to the dataspace for the next dsp-object */
  return (w+NEXT);
}


/**
 * register a special perform-routine at the dsp-engine
 * this function gets called whenever the DSP is turned ON
 * the name of this function is registered in puck_tilde_setup()
 */
void puck_tilde_dsp(t_puck_tilde *x, t_signal **sp)
{
  /* add puck_tilde_perform() to the DSP-tree;
   * the puck_tilde_perform() will expect "5" arguments (packed into an
   * t_int-array), which are:
   * the objects data-space, 3 signal vectors (which happen to be
   * 2 input signals and 1 output signal) and the length of the
   * signal vectors (all vectors are of the same length)
   */
  dsp_add(puck_tilde_perform, NEXT-1, x,
          sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[0]->s_n);
}

/**
 * this is the "destructor" of the class;
 * it allows us to free dynamically allocated ressources
 */
void puck_tilde_free(t_puck_tilde *x)
{
  /* free any ressources associated with the given inlet */
  inlet_free(x->x_in2);
  inlet_free(x->x_in3);

  /* free any ressources associated with the given outlet */
  outlet_free(x->x_out);
  delete x->the_chuck;
}

/**
 * this is the "constructor" of the class
 * the argument is the initial mixing-factor
 */
void *puck_tilde_new(t_floatarg f)
{
  t_puck_tilde *x = (t_puck_tilde *)pd_new(puck_tilde_class);

  /* save the mixing factor in our dataspace */
  x->x_pan = f;

  /* create a new signal-inlet */
  x->x_in2 = inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);

  /* create a new passive inlet for the mixing-factor */
  x->x_in3 = floatinlet_new (&x->x_obj, &x->x_pan);

  /* create a new signal-outlet */
  x->x_out = outlet_new(&x->x_obj, &s_signal);


  // initial inits
  x->currentdir = canvas_getcurrentdir()->s_name;

  // chuck-related inits
  x->the_chuck = NULL;

  x->the_chuck = new ChucK();
  // set sample rate and number of in/out channels on our chuck
  x->the_chuck->setParam( CHUCK_PARAM_SAMPLE_RATE, MY_SRATE );
  x->the_chuck->setParam( CHUCK_PARAM_INPUT_CHANNELS, MY_CHANNELS );
  x->the_chuck->setParam( CHUCK_PARAM_OUTPUT_CHANNELS, MY_CHANNELS );
  x->the_chuck->setParam( CHUCK_PARAM_WORKING_DIRECTORY, x->currentdir );

  // initialize our chuck
  x->the_chuck->init();

  // print message
  post("chuck-embed example running 'sine.ck'...");
  // run a chuck program from file
  x->the_chuck->compileFile( "sine.ck", "", 1 );

  return (void *)x;
}

extern "C" {
/**
 * define the function-space of the class
 * within a single-object external the name of this function is very special
 */
void puck_tilde_setup(void) {
  puck_tilde_class = class_new(gensym("puck~"),
        (t_newmethod)puck_tilde_new,
        (t_method)puck_tilde_free,
        sizeof(t_puck_tilde),
        CLASS_DEFAULT,
        A_DEFFLOAT, A_NULL);

  /* whenever the audio-engine is turned on, the "puck_tilde_dsp()"
   * function will get called
   */
  class_addmethod(puck_tilde_class,
      (t_method)puck_tilde_dsp, gensym("dsp"), A_CANT, A_NULL);
  /* if no signal is connected to the first inlet, we can as well
   * connect a number box to it and use it as "signal"
   */
  CLASS_MAINSIGNALIN(puck_tilde_class, t_puck_tilde, f);
}

}