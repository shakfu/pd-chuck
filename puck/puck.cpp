// #include "RtAudio/RtAudio.h"
#include "chuck.h"

#include "m_pd.h"

// our datetype
#define SAMPLE float
// corresponding format for RtAudio
// #define MY_FORMAT RTAUDIO_FLOAT32
// sample rate
#define MY_SRATE 44100
// number of channels
#define MY_CHANNELS 2



static t_class *puck_class;

typedef struct _puck {
    t_object x_obj;
    ChucK *the_chuck;
} t_puck;

void puck_bang(t_puck *x) { 
    post("Hello world !!");
}

void *puck_new(void) {
    t_puck *x = (t_puck *)pd_new(puck_class);

    x->the_chuck = NULL;

    x->the_chuck = new ChucK();
    // set sample rate and number of in/out channels on our chuck
    x->the_chuck->setParam( CHUCK_PARAM_SAMPLE_RATE, MY_SRATE );
    x->the_chuck->setParam( CHUCK_PARAM_INPUT_CHANNELS, MY_CHANNELS );
    x->the_chuck->setParam( CHUCK_PARAM_OUTPUT_CHANNELS, MY_CHANNELS );
    x->the_chuck->setParam( CHUCK_PARAM_WORKING_DIRECTORY, "." );

    // initialize our chuck
    x->the_chuck->init();


    return (void *)x;
}

extern "C" {

void puck_setup(void) {
    puck_class =
        class_new(gensym("puck"), 
            (t_newmethod)puck_new, 
            0,
            sizeof(t_puck), 
            CLASS_DEFAULT, 
            A_NULL);

    class_addbang(puck_class, puck_bang);

    // create alias
    class_addcreator((t_newmethod)puck_new, gensym("chuck"), A_NULL);
}

}
