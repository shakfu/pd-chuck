/*
// Copyright (c) 2018 - GRAME CNCM - CICM - ANR MUSICOLL - Pierre Guillot.
// For information on usage and redistribution, and for a DISCLAIMER OF ALL
// WARRANTIES, see the file, "LICENSE.txt," in this distribution.
*/


#include "faust_tilde_ui.h"
#ifdef DSPC
#include <faust/dsp/llvm-dsp-c.h>
#else
#include <faust/dsp/llvm-c-dsp.h>
#endif
#include <string.h>
#include <ctype.h>
#include <float.h>
#include <math.h>

#define MAXFAUSTSTRING 4096
#define FAUST_UI_TYPE_BUTTON     0
#define FAUST_UI_TYPE_TOGGLE     1
#define FAUST_UI_TYPE_NUMBER     2
#define FAUST_UI_TYPE_BARGRAPH   3

// MIDI support,
// cf. https://faust.grame.fr/doc/manual/#midi-and-polyphony-support
enum {
  MIDI_NONE, MIDI_CTRL, MIDI_KEYON, MIDI_KEYOFF, MIDI_KEY,
  MIDI_KEYPRESS, MIDI_PGM, MIDI_CHANPRESS, MIDI_PITCHWHEEL,
  MIDI_START, MIDI_STOP, MIDI_CLOCK,
  N_MIDI
};

// new-style freq/gain/gate voice meta data (you saw it here first ;-)
enum {
  VOICE_NONE, VOICE_FREQ, VOICE_GAIN, VOICE_GATE, N_VOICE
};

// Special keys used on the Faust side to identify the different message types
// in Faust UI meta data such as "[midi:ctrl 7]".
static const char *midi_key[N_MIDI] = {
  "none", "ctrl", "keyon", "keyoff", "key",
  "keypress", "pgm", "chanpress", "pitchwheel",
  "start", "stop", "clock"
};

#if 0 // currently unused
static const char *voice_key[N_VOICE] = {
  "none", "freq", "gain", "gate"
};
#endif

// Encoding of MIDI messages in SMMF (https://bitbucket.org/agraef/pd-smmf).
// This is used for incoming and outgoing MIDI messages on the Pd side. Hence
// the messages use their Pd names, i.e., notes ("key") are named "note",
// aftertouch (key and channel pressure) are named "polytouch" and "touch",
// and "pitchwheel" (or "pitchbend") is named "bend". NOTE: "noteon",
// "noteoff", and "clock" aren't really in SMMF, but for convenience we
// support them anyway. As these aren't produced by the SMMF abstractions,
// you'll have to handle them manually.
static const char *midi_sym_s[N_MIDI] = {
  NULL, "ctl", "noteon", "noteoff", "note",
  "polytouch", "pgm", "touch", "bend",
  "start", "stop", "clock"
  // currently unsupported: cont, sysex
};

// corresponding Pd symbols
static t_symbol *midi_sym[N_MIDI];

// Argument count of the different SMMF messages (excluding the trailing
// channel argument). Note that there are some idiosyncrasies in the argument
// order of the 2-argument messages to account for the way the Pd MIDI objects
// work.
static int midi_argc[N_MIDI] = {
  // ctl has the controller number as the *2nd* data byte, value in 1st
  0, 2,
  // note messages have the note number as the *1st* data byte, velocity in 2nd
  2, 2, 2,
  // polytouch has the note number as the *2nd* data byte, velocity in 1st
  2, 1, 1, 1,
  // start, stop, clock don't have any arguments, and no channel either
  0, 0, 0
};

typedef struct {
  int msg;  // message type (see MIDI_XYZ enum above)
  int num;  // parameter (note or controller number)
  int chan; // MIDI channel (-1 if none)
  int val;  // last output value (passive controls only)
} t_faust_midi_ui;

typedef struct {
  t_symbol *msg; // message selector
  double a, b;   // target range (both zero if none)
  int val;       // last output value (passive controls only)
} t_faust_osc_ui;

// Temporary storage for ui meta data. The ui meta callback is always invoked
// before the callback which creates the ui element itself, so we need to keep
// the meta data somewhere until it can be processed. This is only used for
// midi and osc data at present, but we might use it for other kinds of
// UI-related meta data in the future, such as the style of UI elements.
#define N_MIDI_UI 256
#define N_OSC_UI 256
static struct {
  FAUSTFLOAT* zone;
  int voice;
  size_t n_midi;
  t_faust_midi_ui midi[N_MIDI_UI];
  size_t n_osc;
  t_faust_osc_ui osc[N_OSC_UI];
} last_meta;

// A simple proxy object to receive parameter updates from the GUI.
typedef struct {
  t_pd pd;
  struct _faust_ui_manager *owner;
  t_symbol *uisym; // the symbol to bind to
  t_symbol *lname; // the actual long name of the symbol
  int type; // 0 is an ordinary gui element, 1 = active, 2 = init, 3 = panic
  // recursive means that we're currently sending a message which might
  // trigger an update, so we don't want to receive messages in that case.
  bool recursive;
} t_faust_ui_proxy;

static t_class *faust_ui_proxy_class;

static t_faust_ui_proxy *faust_ui_receive_new(t_faust_ui_manager *owner, t_symbol *uisym, t_symbol *lname, int type)
{
  t_faust_ui_proxy *r = getbytes(sizeof(t_faust_ui_proxy));
  r->pd = faust_ui_proxy_class;
  r->owner = owner;
  r->uisym = uisym;
  r->lname = lname;
  r->type = type;
  r->recursive = false;
  pd_bind(&r->pd, r->uisym);
  return r;
}

static void faust_ui_receive_free(t_faust_ui_proxy *r)
{
  pd_unbind(&r->pd, r->uisym);
  freebytes(r, sizeof(t_faust_ui_proxy));
}

static void faust_ui_receive(t_faust_ui_proxy *r, t_floatarg v);
static void faust_bang_receive(t_faust_ui_proxy *r);

void faust_ui_receive_setup(void)
{
  faust_ui_proxy_class = class_new(gensym("faustgen2~ proxy receive"), 0, 0, sizeof(t_faust_ui_proxy), 0, 0);
  class_addbang(faust_ui_proxy_class, faust_bang_receive);
  class_addfloat(faust_ui_proxy_class, faust_ui_receive);
}

typedef struct _faust_ui
{
    t_symbol*           p_name;
    t_symbol*           p_longname;
    t_symbol*           p_uisym;
    t_faust_ui_proxy *  p_uirecv;
    FAUSTFLOAT          p_uival;
    int                 p_type;
    FAUSTFLOATX*        p_zone;
    FAUSTFLOAT          p_min;
    FAUSTFLOAT          p_max;
    FAUSTFLOAT          p_step;
    FAUSTFLOAT          p_default;
    FAUSTFLOAT          p_saved;
    char                p_kept;
    size_t              p_index;
    FAUSTFLOAT          p_tempv;
    int                 p_voice;
    size_t              p_nmidi;
    t_faust_midi_ui*    p_midi;
    size_t              p_nosc;
    t_faust_osc_ui*     p_osc;
    struct _faust_ui*   p_next;
}t_faust_ui;

// keep track of voice controls
typedef struct _faust_voice {
  int num; // current note playing, if any
  struct _faust_ui *freq_c, *gain_c, *gate_c;
  struct _faust_voice *next_free, *next_used;
} t_faust_voice;

typedef struct _faust_key {
  int num;
  struct _faust_key *next;
} t_faust_key;

typedef struct _faust_ui_manager
{
    UIGlue      f_glue;
    t_object*   f_owner;
    t_faust_ui* f_uis;
    size_t      f_nuis;
    t_symbol**  f_names;
    size_t      f_nnames;
    MetaGlue    f_meta_glue;
    bool        f_isdouble;
    bool        f_quiet;
    bool        f_midi, f_osc;
    // new-style polyphony
    int         f_nvoices;
    t_faust_voice *f_voices, *f_free, *f_used;
    t_faust_key *f_keys;
    t_faust_ui_proxy *f_panic_recv, *f_init_recv, *f_active_recv;
    t_float *f_tuning;
    // old-style polyphony (nvoices meta data), >0 when set
    int         f_npoly;
    struct _faust_ui *freq_c, *gain_c, *gate_c;
}t_faust_ui_manager;

static void faust_free_voices(t_faust_ui_manager *x)
{
  if (x->f_voices) {
    freebytes(x->f_voices, x->f_nvoices*sizeof(t_faust_voice));
    while (x->f_keys) {
      t_faust_key *next = x->f_keys->next;
      freebytes(x->f_keys, sizeof(t_faust_key));
      x->f_keys = next;
    }
    x->f_voices = x->f_free = x->f_used = NULL;
    x->f_nvoices = 0;
  }
}

static void faust_new_voices(t_faust_ui_manager *x)
{
  // make sure not to leak any memory on these
  if (x->f_voices) faust_free_voices(x);
  // iterate over all voice controls, to make sure that we have a consistent
  // number of freq, gain, gate controls
  int n_freq = 0, n_gain = 0, n_gate = 0;
  t_faust_ui *c = x->f_uis;
  while (c) {
    switch (c->p_voice) {
    case VOICE_FREQ:
      n_freq++;
      break;
    case VOICE_GAIN:
      n_gain++;
      break;
    case VOICE_GATE:
      n_gate++;
      break;
    default:
      break;
    }
    c = c->p_next;
  }
  int n_voices = n_freq?n_freq:n_gain?n_gain:n_gate;
  if (n_voices) {
    if ((n_freq && n_freq != n_voices) ||
        (n_gain && n_gain != n_voices) ||
        (n_gate && n_gate != n_voices)) {
      if (!x->f_quiet) pd_error(x->f_owner, "faustgen2~: inconsistent number of voice controls");
      return;
    }
    x->f_keys = NULL;
    x->f_voices = getzbytes(n_voices*sizeof(t_faust_voice));
    if (!x->f_voices) {
      if (!x->f_quiet) pd_error(x->f_owner, "faustgen2~: memory allocation failed - voice controls");
      return;
    }
    if (!x->f_quiet) logpost(x->f_owner, 3, "             [%d voice polyphony]", n_voices);
    // Run through the voice controls again and populate the f_voices table.
    n_freq = 0; n_gain = 0; n_gate = 0;
    c = x->f_uis;
    while (c) {
      switch (c->p_voice) {
      case VOICE_FREQ:
        x->f_voices[n_freq++].freq_c = c;
        break;
      case VOICE_GAIN:
        x->f_voices[n_gain++].gain_c = c;
        break;
      case VOICE_GATE:
        x->f_voices[n_gate++].gate_c = c;
        break;
      default:
        break;
      }
      c = c->p_next;
    }
    x->f_nvoices = n_voices;
    // Initialize the free and used lists.
    x->f_free = x->f_voices;
    x->f_used = NULL;
    for (int i = 0; i < n_voices; i++) {
      if (i+1 < n_voices)
        x->f_voices[i].next_free = x->f_voices+i+1;
      else
        x->f_voices[i].next_free = NULL;
      x->f_voices[i].next_used = NULL;
    }
    x->f_used = NULL;
  }
}

static void faust_ui_free(t_faust_ui *c)
{
  if (c->p_midi)
    freebytes(c->p_midi, c->p_nmidi*sizeof(t_faust_midi_ui));
  if (c->p_osc)
    freebytes(c->p_osc, c->p_nosc*sizeof(t_faust_osc_ui));
  if (c->p_uirecv)
    faust_ui_receive_free(c->p_uirecv);
}

static void faust_ui_manager_free_uis(t_faust_ui_manager *x)
{
    t_faust_ui *c = x->f_uis;
    while(c)
    {
        x->f_uis = c->p_next;
        faust_ui_free(c);
        freebytes(c, sizeof(*c));
        c = x->f_uis;
    }
    faust_free_voices(x);
}

static t_faust_ui* faust_ui_manager_get(t_faust_ui_manager const *x, t_symbol const *name)
{
    t_faust_ui *c = x->f_uis;
    while(c)
    {
        if(c->p_name == name || c->p_longname == name)
        {
            return c;
        }
        c = c->p_next;
    }
    return NULL;
}

// Generic zone value accesses. Note that the actual floating point type of
// the zone values depends on the f_isdouble flag (-double compilation option)
// which may change with each compilation. Normally this is float, but changes
// to double if the dsp is compiled with -double. Thus we need to cast the
// zone pointers accordingly at runtime, which is handled by the following
// routines. Note that the FAUSTFLOAT type doesn't help with this because it
// is determined at compile time. By default, FAUSTFLOAT is just float, but
// you can set it to double *before* including llvm-c-dsp.h if you want to
// internally represent all control data using double precision.

static FAUSTFLOAT faustflt(const t_faust_ui_manager *x, FAUSTFLOATX *z)
{
  if (x->f_isdouble)
    return *(double*)z;
  else
    return *(float*)z;
}

static FAUSTFLOAT setfaustflt(t_faust_ui_manager *x, FAUSTFLOATX *z, FAUSTFLOAT v)
{
  if (x->f_isdouble)
    return (*(double*)z = v);
  else
    return (*(float*)z = v);
}

static void faust_ui_manager_prepare_changes(t_faust_ui_manager *x, int isdbl)
{
    t_faust_ui *c = x->f_uis;
    faust_ui_manager_all_notes_off(x);
    // Note that here we're still accessing the *old* zone values, so we only
    // update the f_isdouble flag to its new value *after* this has been done.
    while(c)
    {
        c->p_kept  = 0;
        c->p_tempv = faustflt(x, c->p_zone);
        c = c->p_next;
    }
    x->f_nuis = 0;
    // Update the f_isdouble flag now, so that subsequent zone value accesses
    // use the new status. Note that this flag may change with each
    // compilation, depending on the compilation options (specifically,
    // whether -double is used or not).
    x->f_isdouble = isdbl;
    x->f_midi = x->f_osc = true;
    faust_free_voices(x);
    last_meta.n_midi = 0;
    last_meta.voice = VOICE_NONE;
    last_meta.n_osc = 0;
}

static int cmpui(const void *p1, const void *p2)
{
  t_faust_ui *c1 = *(t_faust_ui*const*)p1;
  t_faust_ui *c2 = *(t_faust_ui*const*)p2;
  return (int)c1->p_index - (int)c2->p_index;
}

static void faust_ui_manager_sort(t_faust_ui_manager *x)
{
  t_faust_ui *c = x->f_uis;
  if (c) {
    t_faust_ui **cv = (t_faust_ui**)getbytes(x->f_nuis*sizeof(t_faust_ui*));
    size_t i, n = 0;
    if (!cv) {
      if (!x->f_quiet) pd_error(x->f_owner, "faustgen2~: memory allocation failed - ui sort");
      return;
    }
    while (c && n < x->f_nuis) {
      cv[n++] = c;
      c = c->p_next;
    }
    if (n <= x->f_nuis && n > 0) {
      qsort(cv, n, sizeof(t_faust_ui*), cmpui);
      for (i = 1; i < n; i++) {
        cv[i-1]->p_next = cv[i];
      }
      cv[i-1]->p_next = NULL;
      x->f_uis = cv[0];
    } else {
      if (!x->f_quiet) pd_error(x->f_owner, "faustgen2~: internal error - ui sort");
    }
    freebytes(cv, x->f_nuis*sizeof(t_faust_ui*));
  }
}

static void faust_ui_manager_finish_changes(t_faust_ui_manager *x)
{
    t_faust_ui *c = x->f_uis;
    if(c)
    {
        t_faust_ui *n = c->p_next;
        while(n)
        {
            if(!n->p_kept)
            {
                c->p_next = n->p_next;
                faust_ui_free(n);
                freebytes(n, sizeof(*n));
                n = c->p_next;
            }
            else
            {
                c = n;
                n = c->p_next;
            }
        }
        c = x->f_uis;
        if(!c->p_kept)
        {
            x->f_uis = c->p_next;
            faust_ui_free(c);
            freebytes(c, sizeof(*c));
        }
        faust_ui_manager_sort(x);
        faust_new_voices(x);
    }
}

static void faust_ui_manager_free_names(t_faust_ui_manager *x)
{
    if(x->f_names && x->f_nnames)
    {
        freebytes(x->f_names, x->f_nnames * sizeof(t_symbol *));
    }
    x->f_names  = NULL;
    x->f_nnames = 0;
}

/* ag: Pd’s input syntax for symbols is rather restrictive. Whitespace is not
   allowed, and many punctuation characters have a special meaning in Pd.
   These can be escaped in some Pd versions, but not in others, and in any
   case the rules may not be obvious. Because any of these special characters
   are allowed in Faust labels, group and control labels in the Faust source
   are mangled into a form which only contains alphanumeric characters and
   hyphens, so that the control names are always legal Pd symbols, no matter
   which Pd version you have. For instance, a Faust control name like "meter
   #1 (dB)" will become "meter-1-dB" which can be input directly as a symbol
   in Pd without any problems. */

static t_symbol* mangle(const char* label)
{
  // ASCII-only version for now. To be on the safe side, this should be
  // rewritten so that it works with UTF-8 some time.
  size_t i = 0;
  char name[MAXFAUSTSTRING];
  int state = 0;
  memset(name, 0, MAXFAUSTSTRING);
  while (*label && i < MAXFAUSTSTRING-1) {
    if (isalnum(*label)) {
      if (state) {
        name[i++] = '-';
        if (i >= MAXFAUSTSTRING-1) break;
      }
      name[i++] = *label++;
      state = 0;
    } else {
      label++;
      state = 1;
    }
  }
  // If name is still empty now then label consists of just non-alphanumeric
  // symbols; in that case, the best that we can do is pretend that it's an
  // empty label ("0x00" in Faust; we'll later get rid of those as well).
  return gensym(*name?name:"0x00");
}

static t_symbol* faust_ui_manager_get_long_name(t_faust_ui_manager *x, const char* label)
{
    size_t i;
    char name[MAXFAUSTSTRING];
    memset(name, 0, MAXFAUSTSTRING);
    for(i = 0; i < x->f_nnames; ++i)
    {
        // remove dummy "0x00" labels for anonymous groups
        if (strcmp(x->f_names[i]->s_name, "0x00") == 0) continue;
        strncat(name, x->f_names[i]->s_name, MAXFAUSTSTRING - strnlen(name, MAXFAUSTSTRING) - 1);
        strncat(name, "/", MAXFAUSTSTRING - strnlen(name, MAXFAUSTSTRING) - 1);
    }
    // remove dummy "0x00" labels for anonymous controls
    t_symbol *mangled;
    if (strcmp(label, "0x00") &&
        strcmp((mangled = mangle(label))->s_name, "0x00"))
      strncat(name, mangled->s_name, MAXFAUSTSTRING - strnlen(name, MAXFAUSTSTRING) - 1);
    else if (*name) // remove trailing "/"
      name[strnlen(name, MAXFAUSTSTRING) - 1] = 0;
    // The result is a canonicalized path which has all the "0x00" components
    // removed. This path may be empty if all components, including the
    // control label itself, are "0x00". In that case, return "anon" instead.
    return gensym(*name?name:"anon");
}

static t_symbol* faust_ui_manager_get_name(t_faust_ui_manager *x, const char* label)
{
    size_t i;
    // return the last component in the path which isn't "0x00"
    t_symbol *mangled;
    if (strcmp(label, "0x00") &&
        strcmp((mangled = mangle(label))->s_name, "0x00"))
      return mangled;
    for(i = x->f_nnames; i > 0; --i)
    {
      if (strcmp(x->f_names[i-1]->s_name, "0x00"))
        return x->f_names[i-1];
    }
    // The resulting name may be empty if all components, including the
    // control label itself, are "0x00"; in that case, return "anon" instead.
    return gensym("anon");
}

static int midi_defaultval(FAUSTFLOAT z, FAUSTFLOAT p_min, FAUSTFLOAT p_max,
                           int p_type, int msg);
static int osc_defaultval(FAUSTFLOAT z, FAUSTFLOAT p_min, FAUSTFLOAT p_max,
                          int p_type, double a, double b);

static void faust_ui_manager_add_param(t_faust_ui_manager *x, const char* label, int const type, FAUSTFLOAT* zone,
                                        FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step)
{
    FAUSTFLOAT saved, current;
    t_symbol* name  = faust_ui_manager_get_name(x, label);
    t_symbol* lname = faust_ui_manager_get_long_name(x, label);
    t_faust_ui *c   = faust_ui_manager_get(x, lname);
    if(c && !c->p_kept)
    {
        saved   = c->p_saved;
        current = c->p_tempv;
        faust_ui_free(c);
    }
    else
    {
        c = (t_faust_ui *)getbytes(sizeof(*c));
        if(!c)
        {
            if (!x->f_quiet) pd_error(x->f_owner, "faustgen2~: memory allocation failed - ui glue");
            return;
        }
        c->p_name   = name;
        c->p_next   = x->f_uis;
        x->f_uis    = c;
        saved       = init;
        current     = init;
    }
    c->p_longname  = lname;
    c->p_uisym     = NULL;
    c->p_uirecv    = NULL;
    c->p_type      = type;
    c->p_zone      = zone;
    c->p_min       = min;
    c->p_max       = max;
    c->p_step      = step;
    c->p_default   = init;
    c->p_saved     = saved;
    c->p_kept      = 1;
    c->p_index     = x->f_nuis++;
    c->p_osc       = NULL;
    c->p_nosc      = 0;
    c->p_midi      = NULL;
    c->p_nmidi     = 0;
    c->p_voice     = VOICE_NONE;
    setfaustflt(x, c->p_zone, current);
    if (last_meta.zone == zone) {
      if (last_meta.voice) {
        if (c->p_type != FAUST_UI_TYPE_BARGRAPH) {
          c->p_voice = last_meta.voice;
#if 0
          if (!x->f_quiet)
            logpost(x->f_owner, 3, "             %s: voice:%s", name->s_name,
                    voice_key[last_meta.voice]);
#endif
        } else {
          // voice controls can't be passive
          if (!x->f_quiet) pd_error(x->f_owner, "faustgen2~: '%s' can't be used as voice control", name->s_name);
        }
      }
      if (last_meta.n_midi) {
        c->p_midi = getbytes(last_meta.n_midi*sizeof(t_faust_midi_ui));
        if (c->p_midi) {
          c->p_nmidi = last_meta.n_midi;
          for (size_t i = 0; i < last_meta.n_midi; i++) {
            if (!x->f_quiet) {
              if (last_meta.midi[i].chan >= 0) {
                if (midi_argc[last_meta.midi[i].msg] > 1)
                  logpost(x->f_owner, 3, "             %s: midi:%s %d %d", name->s_name,
                          midi_key[last_meta.midi[i].msg], last_meta.midi[i].num,
                          last_meta.midi[i].chan);
                else
                  logpost(x->f_owner, 3, "             %s: midi:%s %d", name->s_name,
                          midi_key[last_meta.midi[i].msg], last_meta.midi[i].chan);
              } else {
                if (midi_argc[last_meta.midi[i].msg] > 1)
                  logpost(x->f_owner, 3, "             %s: midi:%s %d", name->s_name,
                          midi_key[last_meta.midi[i].msg], last_meta.midi[i].num);
                else
                  logpost(x->f_owner, 3, "             %s: midi:%s", name->s_name,
                          midi_key[last_meta.midi[i].msg]);
              }
            }
            c->p_midi[i].msg  = last_meta.midi[i].msg;
            c->p_midi[i].num  = last_meta.midi[i].num;
            c->p_midi[i].chan = last_meta.midi[i].chan;
            c->p_midi[i].val  = midi_defaultval(init, min, max, type,
                                                c->p_midi[i].msg);
          }
        } else if (!x->f_quiet) {
          pd_error(x->f_owner, "faustgen2~: memory allocation failed - ui midi");
        }
      }
      if (last_meta.n_osc) {
        c->p_osc = getbytes(last_meta.n_osc*sizeof(t_faust_osc_ui));
        if (c->p_osc) {
          c->p_nosc = last_meta.n_osc;
          for (size_t i = 0; i < last_meta.n_osc; i++) {
            if (!x->f_quiet)
              logpost(x->f_owner, 3, "             %s: osc:%s %g %g",
                      name->s_name, last_meta.osc[i].msg->s_name,
                      last_meta.osc[i].a, last_meta.osc[i].b);
            c->p_osc[i].msg = last_meta.osc[i].msg;
            c->p_osc[i].a   = last_meta.osc[i].a;
            c->p_osc[i].b   = last_meta.osc[i].b;
            c->p_osc[i].val = osc_defaultval(init, min, max, type,
                                             c->p_osc[i].a,
                                             c->p_osc[i].b);
          }
        } else if (!x->f_quiet) {
          pd_error(x->f_owner, "faustgen2~: memory allocation failed - ui osc");
        }
      }
    }
    last_meta.n_osc = last_meta.n_midi = 0;
    last_meta.voice = VOICE_NONE;
    // old-style polyphony
    if (strcmp(name->s_name, "freq") == 0) {
      x->freq_c = c;
    } else if (strcmp(name->s_name, "gain") == 0) {
      x->gain_c = c;
    } else if (strcmp(name->s_name, "gate") == 0) {
      x->gate_c = c;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//                                      PRIVATE INTERFACE                                       //
//////////////////////////////////////////////////////////////////////////////////////////////////

// NAME PATH
//////////////////////////////////////////////////////////////////////////////////////////////////

static void faust_ui_manager_ui_open_box(t_faust_ui_manager* x, const char* label)
{
    if(x->f_nnames)
    {
        t_symbol** temp = (t_symbol**)resizebytes(x->f_names, x->f_nnames * sizeof(t_symbol *), (x->f_nnames + 1) * sizeof(t_symbol *));
        if(temp)
        {
            x->f_names  = temp;
            x->f_names[x->f_nnames] = mangle(label);
            x->f_nnames = x->f_nnames + 1;
            return;
        }
        else
        {
            if (!x->f_quiet) pd_error(x->f_owner, "faustgen2~: memory allocation failed - ui box");
            return;
        }
    }
    else
    {
        x->f_names = getbytes(sizeof(t_symbol *));
        if(x->f_names)
        {
            x->f_names[0] = mangle(label);
            x->f_nnames = 1;
            return;
        }
        else
        {
            if (!x->f_quiet) pd_error(x->f_owner, "faustgen2~: memory allocation failed - ui box");
            return;
        }
    }
}

static void faust_ui_manager_ui_close_box(t_faust_ui_manager* x)
{
    if(x->f_nnames > 1)
    {
        t_symbol** temp = (t_symbol**)resizebytes(x->f_names, x->f_nnames * sizeof(t_symbol *), (x->f_nnames - 1) * sizeof(t_symbol *));
        if(temp)
        {
            x->f_names  = temp;
            x->f_nnames = x->f_nnames - 1;
            return;
        }
        else
        {
            if (!x->f_quiet) pd_error(x->f_owner, "faustgen2~: memory de-allocation failed - ui box");
            return;
        }
    }
    else if(x->f_nnames)
    {
        freebytes(x->f_names, sizeof(t_symbol *));
        x->f_names  = NULL;
        x->f_nnames = 0;
    }
}


// ACTIVE UIS
//////////////////////////////////////////////////////////////////////////////////////////////////

static void faust_ui_manager_ui_add_button(t_faust_ui_manager* x, const char* label, FAUSTFLOAT* zone)
{
    faust_ui_manager_add_param(x, label, FAUST_UI_TYPE_BUTTON, zone, 0, 0, 0, 0);
}

static void faust_ui_manager_ui_add_toggle(t_faust_ui_manager* x, const char* label, FAUSTFLOAT* zone)
{
    faust_ui_manager_add_param(x, label, FAUST_UI_TYPE_TOGGLE, zone, 0, 0, 1, 1);
}

static void faust_ui_manager_ui_add_number(t_faust_ui_manager* x, const char* label, FAUSTFLOAT* zone,
                                            FAUSTFLOAT init, FAUSTFLOAT min, FAUSTFLOAT max, FAUSTFLOAT step)
{
    faust_ui_manager_add_param(x, label, FAUST_UI_TYPE_NUMBER, zone, init, min, max, step);
}

// PASSIVE UIS
//////////////////////////////////////////////////////////////////////////////////////////////////

static void faust_ui_manager_ui_add_bargraph(t_faust_ui_manager* x, const char* label,
                                                        FAUSTFLOAT* zone, FAUSTFLOAT min, FAUSTFLOAT max)
{
    faust_ui_manager_add_param(x, label, FAUST_UI_TYPE_BARGRAPH, zone, 0, min, max, 0);
}

static void faust_ui_manager_ui_add_sound_file(t_faust_ui_manager* x, const char* label, const char* filename, struct Soundfile** sf_zone)
{
    if (!x->f_quiet) pd_error(x->f_owner, "faustgen2~: add sound file not supported yet");
}

// DECLARE UIS
//////////////////////////////////////////////////////////////////////////////////////////////////

static void faust_ui_manager_ui_declare(t_faust_ui_manager* x, FAUSTFLOAT* zone, const char* key, const char* value)
{
  if (zone && value && *value) {
    //if (!x->f_quiet) logpost(x->f_owner, 3, "             %s: %s (%p)", key, value, zone);
    if (strcmp(key, "voice") == 0) {
      if (strcmp(value, "freq") == 0) {
        last_meta.zone = zone;
        last_meta.voice = VOICE_FREQ;
      } else if (strcmp(value, "gain") == 0) {
        last_meta.zone = zone;
        last_meta.voice = VOICE_GAIN;
      } else if (strcmp(value, "gate") == 0) {
        last_meta.zone = zone;
        last_meta.voice = VOICE_GATE;
      }
    } else if (strcmp(key, "midi") == 0) {
      unsigned num, chan;
      int count;
      size_t i = last_meta.n_midi;
      // We only support up to N_MIDI_UI different entries per element.
      if (i >= N_MIDI_UI) return;
      // The extra channel argument isn't in the Faust manual, but recognized
      // in faust/gui/MidiUI.h, so we support it here, too.
      if ((count = sscanf(value, "ctrl %u %u", &num, &chan)) > 0) {
        last_meta.zone = zone;
        last_meta.midi[i].msg = MIDI_CTRL;
        last_meta.midi[i].num = num;
        last_meta.midi[i].chan = (count > 1)?chan:-1;
        last_meta.midi[i].val = -1;
        last_meta.n_midi++;
      } else if ((count = sscanf(value, "keyon %u %u", &num, &chan)) > 0) {
        last_meta.zone = zone;
        last_meta.midi[i].msg = MIDI_KEYON;
        last_meta.midi[i].num = num;
        last_meta.midi[i].chan = (count > 1)?chan:-1;
        last_meta.midi[i].val = -1;
        last_meta.n_midi++;
      } else if ((count = sscanf(value, "keyoff %u %u", &num, &chan)) > 0) {
        last_meta.zone = zone;
        last_meta.midi[i].msg = MIDI_KEYOFF;
        last_meta.midi[i].num = num;
        last_meta.midi[i].chan = (count > 1)?chan:-1;
        last_meta.midi[i].val = -1;
        last_meta.n_midi++;
      } else if ((count = sscanf(value, "key %u %u", &num, &chan)) > 0) {
        last_meta.zone = zone;
        last_meta.midi[i].msg = MIDI_KEY;
        last_meta.midi[i].num = num;
        last_meta.midi[i].chan = (count > 1)?chan:-1;
        last_meta.midi[i].val = -1;
        last_meta.n_midi++;
      } else if ((count = sscanf(value, "keypress %u %u", &num, &chan)) > 0) {
        last_meta.zone = zone;
        last_meta.midi[i].msg = MIDI_KEYPRESS;
        last_meta.midi[i].num = num;
        last_meta.midi[i].chan = (count > 1)?chan:-1;
        last_meta.midi[i].val = -1;
        last_meta.n_midi++;
      } else if ((count = sscanf(value, "pgm %u", &chan)) > 0 ||
                 strcmp(value, "pgm") == 0) {
        last_meta.zone = zone;
        last_meta.midi[i].msg = MIDI_PGM;
        last_meta.midi[i].num = 0; // ignored
        last_meta.midi[i].chan = (count > 0)?chan:-1;
        last_meta.midi[i].val = -1;
        last_meta.n_midi++;
      } else if ((count = sscanf(value, "chanpress %u", &chan)) > 0 ||
                 strcmp(value, "chanpress") == 0) {
        // At the time of this writing, this isn't mentioned in the Faust
        // manual, but it is in faust/gui/MidiUI.h. (The implementation in
        // faust/gui/MidiUI.h seems to be broken at present, however, as it
        // adds an extra note number argument which doesn't make any sense
        // with channel pressure. Here we do it correctly.)
        last_meta.zone = zone;
        last_meta.midi[i].msg = MIDI_CHANPRESS;
        last_meta.midi[i].num = 0; // ignored
        last_meta.midi[i].chan = (count > 0)?chan:-1;
        last_meta.midi[i].val = -1;
        last_meta.n_midi++;
      } else if ((count = sscanf(value, "pitchwheel %u", &chan)) > 0 ||
                 strcmp(value, "pitchwheel") == 0) {
        last_meta.zone = zone;
        last_meta.midi[i].msg = MIDI_PITCHWHEEL;
        last_meta.midi[i].num = 0; // ignored
        last_meta.midi[i].chan = (count > 0)?chan:-1;
        last_meta.midi[i].val = -1;
        last_meta.n_midi++;
      } else if ((count = sscanf(value, "pitchbend %u", &chan)) > 0 ||
                 strcmp(value, "pitchbend") == 0) {
        // synonym for "pitchwheel" (again, this isn't in the Faust manual,
        // but it is in faust/gui/MidiUI.h, so we support it)
        last_meta.zone = zone;
        last_meta.midi[i].msg = MIDI_PITCHWHEEL;
        last_meta.midi[i].num = 0; // ignored
        last_meta.midi[i].chan = (count > 0)?chan:-1;
        last_meta.midi[i].val = -1;
        last_meta.n_midi++;
      } else if (strcmp(value, "start") == 0) {
        last_meta.zone = zone;
        last_meta.midi[i].msg = MIDI_START;
        last_meta.midi[i].num = 0; // ignored
        last_meta.midi[i].chan = -1;
        last_meta.midi[i].val = -1;
        last_meta.n_midi++;
      } else if (strcmp(value, "stop") == 0) {
        last_meta.zone = zone;
        last_meta.midi[i].msg = MIDI_STOP;
        last_meta.midi[i].num = 0; // ignored
        last_meta.midi[i].chan = -1;
        last_meta.midi[i].val = -1;
        last_meta.n_midi++;
      } else if (strcmp(value, "clock") == 0) {
        last_meta.zone = zone;
        last_meta.midi[i].msg = MIDI_CLOCK;
        last_meta.midi[i].num = 0; // ignored
        last_meta.midi[i].chan = -1;
        last_meta.midi[i].val = -1;
        last_meta.n_midi++;
      }
    } else if (strcmp(key, "osc") == 0) {
      char s[1024];
      double a, b;
      size_t i = last_meta.n_osc;
      // We only support up to N_OSC_UI different entries per element.
      if (i >= N_OSC_UI) return;
      if (sscanf(value, "%1023s %lg %lg", s, &a, &b) == 3) {
        last_meta.zone = zone;
        last_meta.osc[i].msg = gensym(s);
        last_meta.osc[i].a = a;
        last_meta.osc[i].b = b;
        last_meta.n_osc++;
      } else if (sscanf(value, "%1023s", s) == 1) {
        last_meta.zone = zone;
        last_meta.osc[i].msg = gensym(s);
        // range defaults to 0.0 - 1.0 (the default OSC range)
        last_meta.osc[i].a = 0.0;
        last_meta.osc[i].b = 1.0;
        last_meta.n_osc++;
      }
    }
  }
}

// META UIS
//////////////////////////////////////////////////////////////////////////////////////////////////

static void faust_ui_manager_meta_declare(t_faust_ui_manager* x, const char* key, const char* value)
{
#if 0
    if (!x->f_quiet) logpost(x->f_owner, 3, "             %s: %s", key, value);
#endif
    // The nvoices declaration is *really* old-school, but still used in some
    // older Faust programs (specifically the pd-faust examples), so we
    // support it here. Modern Faust source uses the nvoices option instead,
    // as described in the Faust manual. We also recognize that, see below.
    int npoly = 0;
    if (strcmp(key, "nvoices") == 0 && sscanf(value, "%d", &npoly) == 1 && npoly > 0) {
      x->f_npoly = npoly;
    }
    if (strcmp(key, "options") == 0 && value) {
      // Currently we recognize the standard Faust options 'nvoices', 'midi',
      // and 'osc'. The latter two are both enabled by default, but you can
      // disable them by specifying an 'off' value.
      const char *s = value;
      char k[128], v[128];
      int n;
      while (isspace(*s)) ++s;
      while (sscanf(s, "[%127[^]:]:%127[^]]]%n", k, v, &n) == 2) {
        if (strcmp(k, "midi") == 0) {
          if (strcmp(v, "on") == 0) {
            x->f_midi = true;
            if (!x->f_quiet) logpost(x->f_owner, 3, "             [%s:%s]", k, v);
          } else if (strcmp(v, "off") == 0) {
            x->f_midi = false;
            if (!x->f_quiet) logpost(x->f_owner, 3, "             [%s:%s]", k, v);
          } else if (!x->f_quiet) {
            pd_error(x->f_owner, "faustgen2~: error parsing option %s:%s", k, v);
          }
        } else if (strcmp(k, "osc") == 0) {
          if (strcmp(v, "on") == 0) {
            x->f_osc = true;
            if (!x->f_quiet) logpost(x->f_owner, 3, "             [%s:%s]", k, v);
          } else if (strcmp(v, "off") == 0) {
            x->f_osc = false;
            if (!x->f_quiet) logpost(x->f_owner, 3, "             [%s:%s]", k, v);
          } else if (!x->f_quiet) {
            pd_error(x->f_owner, "faustgen2~: error parsing option %s:%s", k, v);
          }
        } else if (strcmp(k, "nvoices") == 0 && sscanf(v, "%d", &npoly) == 1 && npoly > 0) {
	  x->f_npoly = npoly;
        }
        s += n;
        while (isspace(*s)) ++s;
      }
      while (isspace(*s)) ++s;
      if (*s) {
        if (!x->f_quiet) pd_error(x->f_owner, "faustgen2~: error parsing option %s", s);
      }
    }
}


//////////////////////////////////////////////////////////////////////////////////////////////////
//                                      PUBLIC INTERFACE                                        //
//////////////////////////////////////////////////////////////////////////////////////////////////

t_faust_ui_manager* faust_ui_manager_new(t_object* owner)
{
    t_faust_ui_manager* ui_manager = (t_faust_ui_manager*)getbytes(sizeof(t_faust_ui_manager));
    if(ui_manager)
    {
        ui_manager->f_glue.uiInterface            = ui_manager;
        ui_manager->f_glue.openTabBox             = (openTabBoxFun)faust_ui_manager_ui_open_box;
        ui_manager->f_glue.openHorizontalBox      = (openHorizontalBoxFun)faust_ui_manager_ui_open_box;
        ui_manager->f_glue.openVerticalBox        = (openVerticalBoxFun)faust_ui_manager_ui_open_box;
        ui_manager->f_glue.closeBox               = (closeBoxFun)faust_ui_manager_ui_close_box;
        
        ui_manager->f_glue.addButton              = (addButtonFun)faust_ui_manager_ui_add_button;
        ui_manager->f_glue.addCheckButton         = (addCheckButtonFun)faust_ui_manager_ui_add_toggle;
        ui_manager->f_glue.addVerticalSlider      = (addVerticalSliderFun)faust_ui_manager_ui_add_number;
        ui_manager->f_glue.addHorizontalSlider    = (addHorizontalSliderFun)faust_ui_manager_ui_add_number;
        ui_manager->f_glue.addNumEntry            = (addNumEntryFun)faust_ui_manager_ui_add_number;
        
        ui_manager->f_glue.addHorizontalBargraph  = (addHorizontalBargraphFun)faust_ui_manager_ui_add_bargraph;
        ui_manager->f_glue.addVerticalBargraph    = (addVerticalBargraphFun)faust_ui_manager_ui_add_bargraph;
        ui_manager->f_glue.addSoundfile           = (addSoundfileFun)faust_ui_manager_ui_add_sound_file;
        ui_manager->f_glue.declare                = (declareFun)faust_ui_manager_ui_declare;
        
        ui_manager->f_owner     = owner;
        ui_manager->f_uis       = NULL;
        ui_manager->f_nuis      = 0;
        ui_manager->f_names     = NULL;
        ui_manager->f_nnames    = 0;
        ui_manager->f_isdouble  = false;
        ui_manager->f_midi = ui_manager->f_osc = true;
        ui_manager->f_nvoices   = 0;
        ui_manager->f_npoly     = 0;
        ui_manager->freq_c = ui_manager->gain_c = ui_manager->gate_c = NULL;
        ui_manager->f_keys = NULL;
        ui_manager->f_voices = ui_manager->f_free = ui_manager->f_used = NULL;
        ui_manager->f_panic_recv = NULL;
        ui_manager->f_init_recv = NULL;
        ui_manager->f_active_recv = NULL;
        ui_manager->f_tuning = NULL;
        ui_manager->f_quiet = false;
        
        ui_manager->f_meta_glue.metaInterface = ui_manager;
        ui_manager->f_meta_glue.declare       = (metaDeclareFun)faust_ui_manager_meta_declare;
    }
    return ui_manager;
}

void faust_ui_manager_free(t_faust_ui_manager *x)
{
    faust_ui_manager_clear(x);
    freebytes(x, sizeof(*x));
}

void faust_ui_manager_init(t_faust_ui_manager *x, void* dspinstance, int isdbl, char quiet)
{
    x->f_quiet = quiet;
    faust_ui_manager_prepare_changes(x, isdbl);
    buildUserInterfaceCDSPInstance((llvm_dsp *)dspinstance, (UIGlue *)&(x->f_glue));
    faust_ui_manager_finish_changes(x);
    faust_ui_manager_free_names(x);
    metadataCDSPInstance((llvm_dsp *)dspinstance, &x->f_meta_glue);
    x->f_quiet = false;
}

void faust_ui_manager_clear(t_faust_ui_manager *x)
{
    if (x->f_panic_recv) faust_ui_receive_free(x->f_panic_recv);
    if (x->f_init_recv) faust_ui_receive_free(x->f_init_recv);
    if (x->f_active_recv) faust_ui_receive_free(x->f_active_recv);
    if (x->f_tuning) freebytes(x->f_tuning, 12*sizeof(t_float));
    faust_ui_manager_free_uis(x);
    faust_ui_manager_free_names(x);
}

char faust_ui_manager_get_polyphony(t_faust_ui_manager *x, char *midi, int *npoly,
                                    FAUSTFLOATX** freq, FAUSTFLOATX** gain, FAUSTFLOATX** gate)
{
  *midi = x->f_midi;
  *freq = *gain = *gate = NULL;
  // we prefer new-style polyphony if we have both
  if (x->f_nvoices > 0 || x->f_npoly == 0) {
    *npoly = 0;
  } else {
    *npoly = x->f_npoly;
    if (x->freq_c) {
      // suppress in GUI
      x->freq_c->p_voice = VOICE_FREQ;
      *freq = x->freq_c->p_zone;
    }
    if (x->gain_c) {
      // suppress in GUI
      x->gain_c->p_voice = VOICE_GAIN;
      *gain = x->gain_c->p_zone;
    }
    if (x->gate_c) {
      // suppress in GUI
      x->gate_c->p_voice = VOICE_GATE;
      *gate = x->gate_c->p_zone;
    }
  }
  return *npoly > 0;
}

static void gui_update(FAUSTFLOAT v, t_faust_ui_proxy *r)
{
  if (r) {
    t_symbol *s = r->uisym;
    if (s && s->s_thing) {
      // This suppresses a recursive update while we're sending the message.
      r->recursive = true;
      pd_float(s->s_thing, v);
      r->recursive = false;
    }
  }
}

static void set_zone(t_faust_ui_manager *x, FAUSTFLOATX *z, FAUSTFLOAT v, t_faust_ui_proxy *r)
{
  setfaustflt(x, z, v);
  gui_update(v, r);
}

char faust_ui_manager_set_value(t_faust_ui_manager *x, t_symbol const *name, t_float const f)
{
    t_faust_ui* ui = faust_ui_manager_get(x, name);
    if(ui)
    {
        if(ui->p_type == FAUST_UI_TYPE_BUTTON || ui->p_type == FAUST_UI_TYPE_TOGGLE)
        {
            set_zone(x, ui->p_zone, (FAUSTFLOAT)(f > FLT_EPSILON), ui->p_uirecv);
            return 0;
        }
        else if(ui->p_type == FAUST_UI_TYPE_NUMBER)
        {
            const FAUSTFLOAT v = (FAUSTFLOAT)(f);
            set_zone(x, ui->p_zone, (FAUSTFLOAT)(v < ui->p_min?ui->p_min:v > ui->p_max?ui->p_max:v), ui->p_uirecv);
            return 0;
        }
    }
    return 1;
}

char faust_ui_manager_get_value(t_faust_ui_manager const *x, t_symbol const *name, t_float* f)
{
    t_faust_ui* ui = faust_ui_manager_get(x, name);
    if(ui)
    {
        *f = (t_float)(faustflt(x, ui->p_zone));
        return 0;
    }
    return 1;
}

static double round_near(double x, double x0, double min, double max)
{
  if (fabs(x-x0) < FLT_EPSILON*(max - min)) x = x0;
  return x;
}

static FAUSTFLOAT translate_from_midi(int val, int min, int max, int p_type,
                                      FAUSTFLOAT p_min, FAUSTFLOAT p_max,
                                      FAUSTFLOAT p_step)
{
  // clamp val in the prescribed range
  if (val < min) val = min;
  if (val > max) val = max;
  // We pretend here that the range of val is one larger than it actually is,
  // so that the range becomes symmetrical and 64 (or 8192 for 14 bit values)
  // gets mapped to the center value. To make up for this, we also increase
  // the value at the end of the range by 1 if needed, so that the entire
  // range is covered no matter what the target range and rounding setup is.
  if (max - min > 1 && val == max-1) val = max;
  if (p_type == FAUST_UI_TYPE_BUTTON || p_type == FAUST_UI_TYPE_TOGGLE) {
    return val>min?1.0:0.0;
  } else {
    // We go to some lengths here to avoid rounding issues around the min, max
    // and center values.
    double v = (double)(val-min)/(double)(max-min);
    if (p_min > p_max) {
      FAUSTFLOAT temp = p_min;
      p_min = p_max; p_max = temp; p_step = -p_step;
    }
    v *= (p_max - p_min);
    // round near center value
    v = round_near(v, (p_max-p_min)/2.0, p_min, p_max);
    // round near min and max
    v += p_min;
    v = round_near(v, p_min, p_min, p_max);
    v = round_near(v, p_max, p_min, p_max);
    // Round to the nearest step. This needs to be done last, to avoid
    // rounding issues near the center value.
    if (p_step > FLT_EPSILON) v = p_step*round(v/p_step);
    // clamp the result, to be on the safe side
    if (v < p_min) v = p_min;
    if (v > p_max) v = p_max;
    return v;
  }
}

static void faust_ui_midi_init(void)
{
  if (!midi_sym[MIDI_CTRL]) {
    // populate the midi_sym table
    for (int i = 1; i < N_MIDI; i++)
      if (midi_sym_s[i])
        midi_sym[i] = gensym(midi_sym_s[i]);
  }
}

// simple MTS-like tuning facility (octave-based tunings only for now)
static t_float note2cps(t_faust_ui_manager *x, int num)
{
  t_float f = num;
  // tuning offset in cents
  if (x->f_tuning) f += x->f_tuning[num%12]/100.0;
  // Pd's mtof() function does the rest
  return mtof(f);
}

// aggraef's homegrown voice allocation algorithm. Note that we simply ignore
// the channel data for now, as faustgen2~ isn't multitimbral (yet). This might
// cause issues with some multi-channel MIDI data sounding slightly off
// depending on the synthesis method being used, but should normally work
// ok. (If all else fails, you can always run separate instances of the dsp for
// different MIDI channels.)

// comment this to disable voice stealing
#define VOICE_STEALING 1

// comment this to disable monophonic/legato mode
// https://ask.audio/articles/understanding-mono-legato-mode-in-synth-sample-vis
#define MONOPHONIC 1

static void voices_noteon(t_faust_ui_manager *x, int num, int val, int chan)
{
#if MONOPHONIC
  if (x->f_nvoices == 1) {
    // monophonic/legato mode, like ye synths of old. Here we simply ignore
    // the free and used lists and keep track of which keys are pressed at any
    // one time.
    t_faust_key *n = getbytes(sizeof(t_faust_key));
    if (n) {
      n->num = num;
      n->next = x->f_keys;
      x->f_keys = n;
    } else {
      pd_error(x->f_owner, "faustgen2~: memory allocation failed - monophony");
    }
    t_faust_voice *v = x->f_voices;
    if (v->freq_c) setfaustflt(x, v->freq_c->p_zone, note2cps(x, num));
    if (v->gain_c) setfaustflt(x, v->gain_c->p_zone, ((double)val)/127.0);
    if (v->gate_c) setfaustflt(x, v->gate_c->p_zone, 1.0);
    return;
  }
#endif
#if VOICE_STEALING
  if (!x->f_free) {
    // no more voices, let's "borrow" one (we can just grab it from the
    // beginning of the used list, that's the longest sounding note)
    t_faust_voice *u = x->f_used;
    if (!u) return; // this can't happen
    x->f_used = u->next_used;
    x->f_free = u;
    u->next_used = u->next_free = NULL;
  }
#endif
  if (x->f_free) {
    // Move this voice to the end of the used list and update the voice
    // controls to kick off the new voice.
    t_faust_voice *v = x->f_free;
    x->f_free = x->f_free->next_free;
    v->next_free = v->next_used = NULL;
    if (x->f_used) {
      t_faust_voice *u = x->f_used;
      while (u->next_used) u = u->next_used;
      u->next_used = v;
    } else {
      x->f_used = v;
    }
    v->num = num;
    // Simply bypass all checking of control ranges and steps for now. We
    // might want to do something more comprehensive later. Also, having MTS
    // support would be nice. :)
    if (v->freq_c) setfaustflt(x, v->freq_c->p_zone, note2cps(x, num));
    if (v->gain_c) setfaustflt(x, v->gain_c->p_zone, ((double)val)/127.0);
    if (v->gate_c) setfaustflt(x, v->gate_c->p_zone, 1.0);
  }
}

static void voices_noteoff(t_faust_ui_manager *x, int num, int chan)
{
#if MONOPHONIC
  if (x->f_nvoices == 1) {
    if (x->f_keys) {
      t_faust_key *n = x->f_keys, *p = NULL;
      while (n && n->num != num) {
        p = n;
        n = n->next;
      }
      if (n) {
        if (p) {
          // not the currently sounding note; simply remove it
          p->next = n->next;
          freebytes(n, sizeof(t_faust_key));
        } else {
          // currently sounding note, change to the previous one (if any)
          t_faust_voice *v = x->f_voices;
          p = n->next;
          freebytes(n, sizeof(t_faust_key));
          if (p) {
            // legato (change to the previous frequency); note that if you
            // want portamento, you'll have to do this in the Faust source
            if (v->freq_c) setfaustflt(x, v->freq_c->p_zone, note2cps(x, p->num));
          } else {
            // note off
            if (v->gate_c) setfaustflt(x, v->gate_c->p_zone, 0.0);
          }
          x->f_keys = p;
        }
      }
    }
    return;
  }
#endif
  t_faust_voice *u = x->f_used, *v = NULL;
  while (u && u->num != num) {
    v = u;
    u = u->next_used;
  }
  if (u) {
    if (v)
      v->next_used = u->next_used;
    else
      x->f_used = u->next_used;
    u->next_free = u->next_used = NULL;
    // Move this voice to the end of the free list and update the gate
    // control to release the voice.
    if (x->f_free) {
      v = x->f_free;
      while (v->next_free) v = v->next_free;
      v->next_free = u;
    } else {
      x->f_free = u;
    }
    if (u->gate_c) setfaustflt(x, u->gate_c->p_zone, 0.0);
  }
}

void faust_ui_manager_all_notes_off(t_faust_ui_manager *x)
{
#if MONOPHONIC
  if (x->f_nvoices == 1) {
    t_faust_voice *v = x->f_voices;
    if (v->gate_c) setfaustflt(x, v->gate_c->p_zone, 0.0);
    while (x->f_keys) {
      t_faust_key *next = x->f_keys->next;
      freebytes(x->f_keys, sizeof(t_faust_key));
      x->f_keys = next;
    }
    return;
  }
#endif
  for (t_faust_voice *u = x->f_used; u; u = u->next_free) {
    if (u->gate_c) setfaustflt(x, u->gate_c->p_zone, 0.0);
    u->next_free = u->next_used;
    u->next_used = NULL;
  }
  // Move all used voices to the end of the free list.
  if (x->f_free) {
    t_faust_voice *v = x->f_free;
    while (v->next_free) v = v->next_free;
    v->next_free = x->f_used;
  } else {
    x->f_free = x->f_used;
  }
  x->f_used = NULL;
}

static bool midichan_check(t_channelmask msk, int chan)
{
  if (chan >= 0 && chan < 64)
    return ((1UL<<chan) & msk) != 0UL;
  else
    return false;
}

int faust_ui_manager_get_midi(t_faust_ui_manager *x, t_symbol const *s, int argc, t_atom* argv, t_channelmask midichanmsk)
{
  int i;
  faust_ui_midi_init();
  for (i = 1; i < N_MIDI; i++) {
    if (s == midi_sym[i]) break;
  }
  if (i < N_MIDI) {
    // Process the message arguments. Note that we generally ignore a
    // trailing channel argument here, unless it is needed in matching. We
    // also ignore any other junk that follows.
    int num, val, chan = -1;
    if (argc < midi_argc[i]) return MIDI_NONE;
    if (midi_argc[i] > 0) {
      if (argv[0].a_type != A_FLOAT) return MIDI_NONE;
      val = (int)argv[0].a_w.w_float;
    }
    if (midi_argc[i] > 1) {
      if (argv[1].a_type != A_FLOAT) return MIDI_NONE;
      num = (int)argv[1].a_w.w_float;
    }
    // Now that we parsed the MIDI message, check whether MIDI processing is
    // actually on. If it is off, we just silently ignore it.
    if (!x->f_midi) return i;
    if (argc > midi_argc[i] && argv[midi_argc[i]].a_type == A_FLOAT) {
      // channel argument
      chan = (int)argv[midi_argc[i]].a_w.w_float;
      // check validity
      if (chan >= 1) {
        // Subtract 1 since channels are zero-based in Faust meta data, but
        // 1-based in Pd. NOTE: Pd allows more than the usual 16 channels,
        // since it treats each MIDI device as a separate block of 16 MIDI
        // channels. Thus 0..15 will denote the channels of the first MIDI
        // device, 16..31 the channels of the second one, etc.
        chan--;
        // match against the object's channel mask
        if (!midichan_check(midichanmsk, chan)) return i;
      } else
        chan = -1;
    }
    // Note messages have their arguments the other way round.
    if (i == MIDI_KEY || i == MIDI_KEYON || i == MIDI_KEYOFF) {
      int temp = num;
      num = val; val = temp;
    }
    // In a polyphonic dsp, process note messages. Note that we only deal with
    // SMMF note messages here, the other variants are only bound by
    // corresponding midi:keyon/off meta data and are handled below.
    if (x->f_voices && i == MIDI_KEY) {
      if (val)
        voices_noteon(x, num, val, chan);
      else
        voices_noteoff(x, num, chan);
    }
    // Run through all the active UI elements with MIDI bindings and update
    // the elements that match.
    t_faust_ui *c = x->f_uis;
    while (c) {
      for (size_t j = 0; j < c->p_nmidi; j++) {
        if (c->p_midi[j].msg == i &&
            (c->p_midi[j].chan < 0 || c->p_midi[j].chan == chan) &&
            c->p_type != FAUST_UI_TYPE_BARGRAPH) {
          bool log = true;
          switch (i) {
          case MIDI_START:
            setfaustflt(x, c->p_zone,
              translate_from_midi(1, 0, 1,
                                  c->p_type, c->p_min, c->p_max, c->p_step));
            break;
          case MIDI_STOP:
            setfaustflt(x, c->p_zone,
              translate_from_midi(0, 0, 1,
                                  c->p_type, c->p_min, c->p_max, c->p_step));
            break;
          case MIDI_CLOCK:
            // square signal which toggles at each clock
            if (c->p_type == FAUST_UI_TYPE_BUTTON ||
                c->p_type == FAUST_UI_TYPE_TOGGLE)
              val = faustflt(x, c->p_zone) == 0.0;
            else
              val = faustflt(x, c->p_zone) == c->p_min;
            setfaustflt(x, c->p_zone,
              translate_from_midi(val, 0, 1,
                                  c->p_type, c->p_min, c->p_max, c->p_step));
            break;
          case MIDI_PITCHWHEEL:
            setfaustflt(x, c->p_zone,
              translate_from_midi(val, 0, 16384,
                                  c->p_type, c->p_min, c->p_max, c->p_step));
            break;
          default:
            if (midi_argc[i] == 1) {
              // Pd counts program changes starting at 1
              if (i == MIDI_PGM) val--;
              setfaustflt(x, c->p_zone,
                translate_from_midi(val, 0, 128,
                                    c->p_type, c->p_min, c->p_max, c->p_step));
            } else if (c->p_midi[j].num == num) {
              setfaustflt(x, c->p_zone,
                translate_from_midi(val, 0, 128,
                                    c->p_type, c->p_min, c->p_max, c->p_step));
            } else {
              log = false;
            }
            break;
          }
          if (log) {
            //logpost(x->f_owner, 3, "%s = %g", c->p_name->s_name, *c->p_zone);
            gui_update(faustflt(x, c->p_zone), c->p_uirecv);
          }
        }
      }
      c = c->p_next;
    }
    return i;
  }
  return MIDI_NONE;
}

static FAUSTFLOAT translate_from_osc(double val, double min, double max,
                                     int p_type,
                                     FAUSTFLOAT p_min, FAUSTFLOAT p_max,
                                     FAUSTFLOAT p_step)
{
  double sign = max>min ? 1.0 : max<min ? -1.0 : 0.0;
  if (sign < 0) {
    double temp = max;
    max = min;
    min = temp;
  }
  // Clamp the value.
  if (val < min) val = min;
  if (val > max) val = max;
  if (p_type == FAUST_UI_TYPE_BUTTON || p_type == FAUST_UI_TYPE_TOGGLE) {
    return val>min?1.0:0.0;
  } else if (sign == 0.0) {
    // In the degenerate case (max == min) we just return the target min value.
    return p_min;
  } else {
    // We go to some lengths here to avoid rounding issues around the min, max
    // and center values.
    double v = sign>0 ? (val-min)/(max-min) : (max-val)/(max-min);
    if (p_min > p_max) {
      FAUSTFLOAT temp = p_min;
      p_min = p_max; p_max = temp; p_step = -p_step;
    }
    v *= (p_max - p_min);
    // round near center value
    v = round_near(v, (p_max-p_min)/2.0, p_min, p_max);
    // round near min and max
    v += p_min;
    v = round_near(v, p_min, p_min, p_max);
    v = round_near(v, p_max, p_min, p_max);
    // Round to the nearest step. This needs to be done last, to avoid
    // rounding issues near the center value.
    if (p_step > FLT_EPSILON) v = p_step*round(v/p_step);
    // clamp the result, to be on the safe side
    if (v < p_min) v = p_min;
    if (v > p_max) v = p_max;
    return v;
  }
}

static double translate_to_osc(FAUSTFLOAT z, FAUSTFLOAT p_min, FAUSTFLOAT p_max,
                               int p_type, double min, double max);

const t_symbol *faust_ui_manager_get_osc(t_faust_ui_manager *x, t_symbol const *s, int argc, t_atom* argv, t_symbol *oscrecv, t_outlet *out)
{
  // The only check here is that the selector looks like a proper OSC message.
  // Any unrecognized OSC message will be simply ignored.
  if (*s->s_name != '/') return NULL;
  // /dump [retsym] gives a quick way to inspect the current values and OSC
  // ranges of all OSC-enabled UI elements.
  if (strcmp(s->s_name, "/dump") == 0) {
    // The optional reply symbol is used as the message selector if given,
    // otherwise the OSC messages are simply output as is.
    t_symbol *r = argc && argv[0].a_type == A_SYMBOL ?
      argv[0].a_w.w_symbol : NULL;
    t_faust_ui *c = x->f_uis;
    while (c) {
      for (size_t j = 0; j < c->p_nosc; j++) {
        int ac = 0;
        t_atom av[4];
        double val = translate_to_osc(faustflt(x, c->p_zone),
                                      c->p_min, c->p_max,
                                      c->p_type, c->p_osc[j].a, c->p_osc[j].b);
        if (r) {
          SETSYMBOL(av+ac, c->p_osc[j].msg); ac++;
        }
        SETFLOAT(av+ac, val); ac++;
        if (r) {
          SETFLOAT(av+ac, c->p_osc[j].a); ac++;
          SETFLOAT(av+ac, c->p_osc[j].b); ac++;
          if (out) outlet_anything(out, r, ac, av);
          if (oscrecv && oscrecv->s_thing)
            typedmess(oscrecv->s_thing, r, ac, av);
        } else {
          // Ranges are only sent if a reply symbol is given, here we only
          // report the current value of an element.
          t_symbol *t = c->p_osc[j].msg;
          if (out) outlet_anything(out, t, ac, av);
          if (oscrecv && oscrecv->s_thing)
            typedmess(oscrecv->s_thing, t, ac, av);
        }
      }
      c = c->p_next;
    }
    if (r) {
      // If we got a reply address, also send an "done" message which signals
      // the end of the reply.
      t_atom av;
      SETSYMBOL(&av, gensym("done"));
      if (out) outlet_anything(out, r, 1, &av);
      if (oscrecv && oscrecv->s_thing)
        typedmess(oscrecv->s_thing, r, 1, &av);
    }
    return s;
  }
  // Run through all the active UI elements with OSC bindings and update
  // the elements that match.
  t_faust_ui *c = x->f_uis;
  while (c) {
    if (c->p_type != FAUST_UI_TYPE_BARGRAPH) {
      for (size_t j = 0; j < c->p_nosc; j++) {
        bool log = true;
        if (argc > 1) {
          int k, n;
          // Multiple arguments are handled by tacking on /0, /1 etc. to the
          // message selector, following the OSC Support section in the Faust
          // manual. First check whether the symbol actually matches a proper
          // prefix of the current message selector.
          size_t l = strlen(s->s_name);
          if (strncmp(s->s_name, c->p_osc[j].msg->s_name, l) == 0 &&
              c->p_osc[j].msg->s_name[l] == '/' &&
              sscanf(c->p_osc[j].msg->s_name+l+1, "%d%n", &k, &n) == 1 &&
              c->p_osc[j].msg->s_name[n+l+1] == 0 && k>=0 && k<argc &&
              argv[k].a_type == A_FLOAT) {
            double val = argv[k].a_w.w_float;
            // Translate the value to the target range.
            setfaustflt(x, c->p_zone,
              translate_from_osc(val, c->p_osc[j].a, c->p_osc[j].b,
                                 c->p_type, c->p_min, c->p_max, c->p_step));
          } else {
            log = false;
          }
        } else if (c->p_osc[j].msg == s &&
                   (argc == 0 || argv[0].a_type == A_FLOAT)) {
          // The Faust manual doesn't say how to handle the case of no
          // arguments. Here we just assume a default value of b in that case.
          double val = argc > 0 ? argv[0].a_w.w_float : c->p_osc[j].b;
          // Translate the value to the target range.
          setfaustflt(x, c->p_zone,
            translate_from_osc(val, c->p_osc[j].a, c->p_osc[j].b,
                               c->p_type, c->p_min, c->p_max, c->p_step));
        } else {
          log = false;
        }
        if (log) {
          //logpost(x->f_owner, 3, "%s = %g", c->p_name->s_name, *c->p_zone);
          gui_update(faustflt(x, c->p_zone), c->p_uirecv);
        }
      }
    }
    c = c->p_next;
  }
  return s;
}

void faust_ui_manager_save_states(t_faust_ui_manager *x)
{
    t_faust_ui *c = x->f_uis;
    while(c)
    {
        c->p_saved = faustflt(x, c->p_zone);
        c = c->p_next;
    }
}

void faust_ui_manager_restore_states(t_faust_ui_manager *x)
{
    t_faust_ui *c = x->f_uis;
    while(c)
    {
        set_zone(x, c->p_zone, c->p_saved, c->p_uirecv);
        c = c->p_next;
    }
}

void faust_ui_manager_restore_default(t_faust_ui_manager *x)
{
    t_faust_ui *c = x->f_uis;
    faust_ui_manager_all_notes_off(x);
    while(c)
    {
        set_zone(x, c->p_zone, c->p_default, c->p_uirecv);
        c = c->p_next;
    }
}

static const char* faust_ui_manager_get_parameter_char(int const type)
{
    if(type == FAUST_UI_TYPE_BUTTON)
        return "button";
    else if(type == FAUST_UI_TYPE_TOGGLE)
        return "toggle";
    else if(type == FAUST_UI_TYPE_NUMBER)
        return "number";
    else
        return "bargraph";
}

void faust_ui_manager_print(t_faust_ui_manager const *x, char const log)
{
    t_faust_ui *c = x->f_uis;
    while(c)
    {
      if (!c->p_voice) {
        const t_symbol *name = c->p_name, *lname = c->p_longname;
        logpost(x->f_owner, 2+log, "parameter: %s [path:%s - type:%s - init:%g - min:%g - max:%g - current:%g]",
                name->s_name, lname->s_name,
                faust_ui_manager_get_parameter_char(c->p_type),
                c->p_default, c->p_min, c->p_max, faustflt(x, c->p_zone));
        if (c->p_midi) {
          for (size_t i = 0; i < c->p_nmidi; i++) {
            if (c->p_midi[i].chan >= 0) {
              if (midi_argc[c->p_midi[i].msg] > 1)
                logpost(x->f_owner, 3, "parameter: %s [midi:%s %d %d]", name->s_name,
                        midi_key[c->p_midi[i].msg], c->p_midi[i].num,
                        c->p_midi[i].chan);
              else
                logpost(x->f_owner, 3, "parameter: %s [midi:%s %d]", name->s_name,
                        midi_key[c->p_midi[i].msg], c->p_midi[i].chan);
            } else {
              if (midi_argc[c->p_midi[i].msg] > 1)
                logpost(x->f_owner, 3, "parameter: %s [midi:%s %d]", name->s_name,
                        midi_key[c->p_midi[i].msg], c->p_midi[i].num);
              else
                logpost(x->f_owner, 3, "parameter: %s [midi:%s]", name->s_name,
                        midi_key[c->p_midi[i].msg]);
            }
          }
        }
        if (c->p_osc) {
          for (size_t i = 0; i < c->p_nosc; i++) {
            logpost(x->f_owner, 3, "parameter: %s [osc:%s %lg %lg]",
                    name->s_name,
                    c->p_osc[i].msg->s_name, c->p_osc[i].a,
                    c->p_osc[i].b);
          }
        }
      }
      c = c->p_next;
    }
}

#define DUMP_MAX_ARGS 100

int faust_ui_manager_dump(t_faust_ui_manager const *x, t_symbol *s, t_outlet *out, t_symbol *outsym)
{
    t_faust_ui *c = x->f_uis;
    t_atom argv[DUMP_MAX_ARGS];
    int argc = 0, n = 0;
    if (outsym && !outsym->s_thing) return 0;
    while(c)
    {
      if (!c->p_voice) {
        SETSYMBOL(argv+argc, c->p_name); argc++;
        SETSYMBOL(argv+argc, c->p_longname); argc++;
        SETSYMBOL(argv+argc, gensym(faust_ui_manager_get_parameter_char(c->p_type))); argc++;
        SETFLOAT(argv+argc, c->p_default); argc++;
        SETFLOAT(argv+argc, c->p_min); argc++;
        SETFLOAT(argv+argc, c->p_max); argc++;
        SETFLOAT(argv+argc, faustflt(x, c->p_zone)); argc++;
        if (c->p_midi) {
          for (size_t i = 0; i < c->p_nmidi && argc+3 < DUMP_MAX_ARGS; i++) {
            char buf[MAXPDSTRING];
            snprintf(buf, MAXPDSTRING, "midi:%s",
                     midi_key[c->p_midi[i].msg]);
            SETSYMBOL(argv+argc, gensym(buf)); argc++;
            if (c->p_midi[i].chan >= 0) {
              if (midi_argc[c->p_midi[i].msg] > 1) {
                SETFLOAT(argv+argc, c->p_midi[i].num); argc++;
                SETFLOAT(argv+argc, c->p_midi[i].chan); argc++;
              } else {
                SETFLOAT(argv+argc, c->p_midi[i].chan); argc++;
              }
            } else if (midi_argc[c->p_midi[i].msg] > 1) {
              SETFLOAT(argv+argc, c->p_midi[i].num); argc++;
            }
          }
        }
        if (c->p_midi) {
          for (size_t i = 0; i < c->p_nosc && argc+3 < DUMP_MAX_ARGS; i++) {
            char buf[MAXPDSTRING];
            snprintf(buf, MAXPDSTRING, "osc:%s",
                     c->p_osc[i].msg->s_name);
            SETSYMBOL(argv+argc, gensym(buf)); argc++;
            SETFLOAT(argv+argc, c->p_osc[i].a); argc++;
            SETFLOAT(argv+argc, c->p_osc[i].b); argc++;
          }
        }
        if (outsym)
          typedmess(outsym->s_thing, s, argc, argv);
        else
          outlet_anything(out, s, argc, argv);
        ++n; argc = 0;
      }
      c = c->p_next;
    }
    return n;
}

void faust_ui_manager_set_tuning(t_faust_ui_manager *x, t_float tuning[12])
{
  if (!x->f_tuning)
    x->f_tuning = getbytes(12*sizeof(t_float));
  if (x->f_tuning) {
    for (int i = 0; i < 12; i++)
      x->f_tuning[i] = tuning[i];
  } else {
    pd_error(x->f_owner, "faustgen2~: memory allocation failed - tuning");
  }
}

t_float *faust_ui_manager_get_tuning(const t_faust_ui_manager *x)
{
  return x->f_tuning;
}

void faust_ui_manager_clear_tuning(t_faust_ui_manager *x)
{
  if (x->f_tuning) {
    freebytes(x->f_tuning, 12*sizeof(t_float));
    x->f_tuning = NULL;
  }
}

static int translate_to_midi(FAUSTFLOAT z, FAUSTFLOAT p_min, FAUSTFLOAT p_max,
                             int min, int max)
{
  if (p_min == p_max)
    // assert(z == p_min)
    return min;
  else {
    // normalize and scale
    z = (z-p_min)/(p_max-p_min)*(max-min);
    // round to integer
    int val = round(z);
    // min should always be zero here, but to be on the safe side...
    z += min;
    // clamp val in the prescribed range
    if (val < min) val = min;
    if (val > max-1) val = max-1;
    return val;
  }
}

static int midi_defaultval(FAUSTFLOAT z, FAUSTFLOAT p_min, FAUSTFLOAT p_max,
                           int p_type, int msg)
{
  if (p_type == FAUST_UI_TYPE_BARGRAPH)
    switch (msg) {
    case MIDI_CLOCK:
    case MIDI_START:
      return 0;
    case MIDI_STOP:
      return 1;
    case MIDI_PITCHWHEEL:
      return translate_to_midi(z, p_min, p_max, 0, 16384);
    default:
      return translate_to_midi(z, p_min, p_max, 0, 128);
    }
  else
    return -1;
}

void faust_ui_manager_midiout(t_faust_ui_manager const *x, int midichan,
                              t_symbol *midirecv, t_outlet *out)
{
  faust_ui_midi_init();
  if (!x->f_midi || (!midirecv && !out)) return; // nothing to do
  // Run through all the passive UI elements with MIDI bindings.
  t_faust_ui *c = x->f_uis;
  while (c) {
    if (c->p_type == FAUST_UI_TYPE_BARGRAPH) {
      for (size_t j = 0; j < c->p_nmidi; j++) {
        int i = c->p_midi[j].msg;
        int num = -1, chan = -1, val = 0, oldval = c->p_midi[j].val;
        t_symbol *s = midi_sym[i];
        int argc = midi_argc[i];
        t_atom argv[3];
        switch (i) {
        case MIDI_START:
          // val means output a start message
          val = faustflt(x, c->p_zone) > c->p_min;
          if (!val) s = NULL;
          break;
        case MIDI_STOP:
          // !val means output a stop message
          val = faustflt(x, c->p_zone) > c->p_min;
          if (val) s = NULL;
          break;
        case MIDI_CLOCK:
          // change in val means output a clock message
          val = faustflt(x, c->p_zone) > c->p_min;
          break;
        case MIDI_PITCHWHEEL:
          val = translate_to_midi(faustflt(x, c->p_zone), c->p_min, c->p_max, 0, 16384);
          // voice message, add channel
          argc++;
          chan = c->p_midi[j].chan;
          break;
        default:
          if (argc == 1) {
            val = translate_to_midi(faustflt(x, c->p_zone), c->p_min, c->p_max, 0, 128);
            // Pd counts program changes starting at 1
            if (i == MIDI_PGM) val++;
          } else {
            val = translate_to_midi(faustflt(x, c->p_zone), c->p_min, c->p_max, 0, 128);
            num = c->p_midi[j].num;
          }
          // voice message, add channel
          argc++;
          chan = c->p_midi[j].chan;
          break;
        }
        // only output changed values
        if (s && val != oldval) {
          c->p_midi[j].val = val;
          // Note messages have their arguments the other way round.
          if (i == MIDI_KEY || i == MIDI_KEYON || i == MIDI_KEYOFF) {
            int temp = num;
            num = val; val = temp;
          }
          if (midi_argc[i] > 0) SETFLOAT(argv+0, val);
          if (midi_argc[i] > 1) SETFLOAT(argv+1, num);
          if (midi_argc[i] < argc) {
            // voice message, add channel (either the object's default MIDI
            // channel, or 0 by default)
            if (chan < 0) chan = midichan>=0?midichan:0;
            // Pd MIDI channels are 1-based
            SETFLOAT(argv+(argc-1), chan+1);
          }
          if (out) outlet_anything(out, s, argc, argv);
          if (midirecv && midirecv->s_thing)
            typedmess(midirecv->s_thing, s, argc, argv);
        }
      }
    }
    c = c->p_next;
  }
}

static double translate_to_osc(FAUSTFLOAT z, FAUSTFLOAT p_min, FAUSTFLOAT p_max,
                               int p_type, double min, double max)
{
  // We must also consider the UI element type here, since this function may
  // also be called for *active* UI elements when processing /dump messages.
  if (p_type == FAUST_UI_TYPE_BUTTON || p_type == FAUST_UI_TYPE_TOGGLE) {
    return z==0.0 ? min : max;
  } else if (fabs(p_min - p_max) < FLT_EPSILON) {
    // assert(z == p_min)
    return min;
  } else {
    // The zone values we get here might be wildly out of range, so clamp them
    // first in order to prevent infinities and (subsequently) NaN values
    // which would completely throw off our mapping computations below.
    if (p_min < p_max) {
      if (z < p_min) z = p_min;
      if (z > p_max) z = p_max;
    } else {
      if (z > p_min) z = p_min;
      if (z < p_max) z = p_max;
    }
    // normalize and scale
    double val = (z-p_min)/(p_max-p_min)*(max-min);
    val += min;
    // clamp the result again so that it stays within the target range
    if (min < max) {
      if (val < min) val = min;
      if (val > max) val = max;
    } else {
      if (val > min) val = min;
      if (val < max) val = max;
    }
    return val;
  }
}

static int osc_defaultval(FAUSTFLOAT z, FAUSTFLOAT p_min, FAUSTFLOAT p_max,
                          int p_type, double a, double b)
{
  if (p_type == FAUST_UI_TYPE_BARGRAPH)
    return translate_to_osc(z, p_min, p_max, p_type, a, b);
  else
    return -1;
}

void faust_ui_manager_oscout(t_faust_ui_manager const *x,
                             t_symbol *oscrecv, t_outlet *out)
{
  if (!x->f_osc || (!oscrecv && !out)) return; // nothing to do
  // Run through all the passive UI elements with OSC bindings.
  t_faust_ui *c = x->f_uis;
  while (c) {
    if (c->p_type == FAUST_UI_TYPE_BARGRAPH) {
      for (size_t j = 0; j < c->p_nosc; j++) {
        t_symbol *s = c->p_osc[j].msg;
        double val = 0.0, oldval = c->p_osc[j].val;
        t_atom argv[1];
        val = translate_to_osc(faustflt(x, c->p_zone), c->p_min, c->p_max,
                               c->p_type, c->p_osc[j].a, c->p_osc[j].b);
        // only output changed values
        if (val != oldval) {
          c->p_osc[j].val = val;
          SETFLOAT(argv+0, val);
          if (out) outlet_anything(out, s, 1, argv);
          if (oscrecv && oscrecv->s_thing)
            typedmess(oscrecv->s_thing, s, 1, argv);
        }
      }
    }
    c = c->p_next;
  }
}

void faust_ui_manager_gui_update(t_faust_ui_manager const *x)
{
  // Run through all the passive UI elements.
  t_faust_ui *c = x->f_uis;
  while (c) {
    if (c->p_type == FAUST_UI_TYPE_BARGRAPH &&
        c->p_uisym && c->p_uisym->s_thing) {
      // only output changed values
      if (faustflt(x, c->p_zone) != c->p_uival) {
        FAUSTFLOAT val = faustflt(x, c->p_zone);
        gui_update(val, c->p_uirecv);
        c->p_uival = val;
      }
    }
    c = c->p_next;
  }
}

static t_symbol *make_sym(t_symbol *dsp_name, t_symbol *longname)
{
  char name[MAXPDSTRING];
  snprintf(name, MAXPDSTRING, "%s/%s", dsp_name->s_name, longname->s_name);
  return gensym(name);
}

void faust_ui_manager_gui(t_faust_ui_manager *x,
                          t_symbol *unique_name, t_symbol *instance_name)
{
  // Check that the target subpatch exists.
  char ui_name[MAXPDSTRING];
  snprintf(ui_name, MAXPDSTRING, "pd-%s", instance_name->s_name);
  t_symbol *ui = gensym(ui_name);
  if (!ui->s_thing) return;
  // Formatting data for the GUI.
  const int black = -1; // foreground color for all GUI elements
  const int white = -0x40000; // background color of active controls
  const int gray  = -0x38e39; // background color of passive controls
  // Spacing of number boxes and horizontal sliders. You may have to adjust
  // this if your Pd version differs from the usual defaults, or if you change
  // the font sizes below.
  const int nentry_x = 75; // nentry_y = 30;
  const int hslider_x = 150, hslider_y = 30;
  // GUI font sizes. fn1 sets the font size of the slider labels, fn2 that of
  // the number boxes. Common font sizes are 10 and 12.
  const int fn1 = 10, fn2 = 10;
  // Run through all UI elements which aren't voice controls. First determine
  // the width and height of the GOP area.
  int wd = 10+hslider_x+nentry_x, ht = hslider_y, y = 0;
  t_faust_ui *c = x->f_uis;
  while (c) {
    if (!c->p_voice) ht += hslider_y;
    c = c->p_next;
  }
  // Initialize the subpatch and create the GOP area.
  int argc = 0;
  t_atom argv[50];
  typedmess(ui->s_thing, gensym("clear"), 0, NULL);
  SETFLOAT(argv+argc, 0); argc++;
  SETFLOAT(argv+argc, -1); argc++;
  SETFLOAT(argv+argc, 1); argc++;
  SETFLOAT(argv+argc, 1); argc++;
  SETFLOAT(argv+argc, wd); argc++;
  SETFLOAT(argv+argc, ht); argc++;
  SETFLOAT(argv+argc, 1); argc++;
  SETFLOAT(argv+argc, 0); argc++;
  SETFLOAT(argv+argc, 0); argc++;
  typedmess(ui->s_thing, gensym("coords"), argc, argv);
  argc = 0;
  // Run through all UI elements again, this time generating the actual
  // contents of the GUI patch.
  c = x->f_uis;
  while (c) {
    if (c->p_voice) {
      // skip voice controls
      c = c->p_next;
      continue;
    }
    t_symbol *s = make_sym(unique_name, c->p_longname);
    c->p_uisym = s;
    if (c->p_uirecv) {
      // No need to recreate any existing receiver, just make sure that the
      // data is up-to-date.
      c->p_uirecv->uisym = s;
      c->p_uirecv->lname = c->p_longname;
    } else
      c->p_uirecv = faust_ui_receive_new(x, s, c->p_longname, 0);
    y += hslider_y;
    switch (c->p_type) {
    case FAUST_UI_TYPE_BUTTON:
    case FAUST_UI_TYPE_TOGGLE:
      // We render both buttons and toggles as Pd toggles, since Pd bangs
      // don't provide the on/off switching functionality that we need.
      SETFLOAT(argv+argc, 10); argc++;
      SETFLOAT(argv+argc, y); argc++;
      SETSYMBOL(argv+argc, gensym("tgl")); argc++;
      SETFLOAT(argv+argc, 15); argc++;
      SETFLOAT(argv+argc, 0); argc++;
      SETSYMBOL(argv+argc, s); argc++;
      SETSYMBOL(argv+argc, s); argc++;
      SETSYMBOL(argv+argc, c->p_name); argc++;
      SETFLOAT(argv+argc, 17); argc++;
      SETFLOAT(argv+argc, 7); argc++;
      SETFLOAT(argv+argc, 0); argc++;
      SETFLOAT(argv+argc, fn1); argc++;
      SETFLOAT(argv+argc, white); argc++;
      SETFLOAT(argv+argc, black); argc++;
      SETFLOAT(argv+argc, black); argc++;
      SETFLOAT(argv+argc, 0); argc++;
      SETFLOAT(argv+argc, 1); argc++;
      typedmess(ui->s_thing, gensym("obj"), argc, argv);
      argc = 0;
      if (s->s_thing) {
        FAUSTFLOAT val = faustflt(x, c->p_zone);
        gui_update(val, c->p_uirecv);
        c->p_uival = val;
      } else {
        // this shouldn't happen
        pd_error(x->f_owner, "faustgen2~: can't initialize %s - gui", s->s_name);
      }
      break;
    case FAUST_UI_TYPE_NUMBER:
    case FAUST_UI_TYPE_BARGRAPH:
      // These are both rendered as horizontal sliders (the bargraphs get a
      // different background color, though, to distinguish it as a passive
      // control).
      SETFLOAT(argv+argc, 10); argc++;
      SETFLOAT(argv+argc, y); argc++;
      SETSYMBOL(argv+argc, gensym("hsl")); argc++;
      SETFLOAT(argv+argc, 128); argc++;
      SETFLOAT(argv+argc, 15); argc++;
      SETFLOAT(argv+argc, c->p_min); argc++;
      SETFLOAT(argv+argc, c->p_max); argc++;
      SETFLOAT(argv+argc, 0); argc++;
      SETFLOAT(argv+argc, 0); argc++;
      SETSYMBOL(argv+argc, s); argc++;
      SETSYMBOL(argv+argc, s); argc++;
      SETSYMBOL(argv+argc, c->p_name); argc++;
      SETFLOAT(argv+argc, -2); argc++;
      SETFLOAT(argv+argc, -6); argc++;
      SETFLOAT(argv+argc, 0); argc++;
      SETFLOAT(argv+argc, fn1); argc++;
      SETFLOAT(argv+argc, c->p_type==FAUST_UI_TYPE_BARGRAPH?gray:white); argc++;
      SETFLOAT(argv+argc, black); argc++;
      SETFLOAT(argv+argc, black); argc++;
      SETFLOAT(argv+argc, 0); argc++;
      SETFLOAT(argv+argc, 1); argc++;
      typedmess(ui->s_thing, gensym("obj"), argc, argv);
      argc = 0;
      SETFLOAT(argv+argc, 10+hslider_x); argc++;
      SETFLOAT(argv+argc, y); argc++;
      SETSYMBOL(argv+argc, gensym("nbx")); argc++;
      SETFLOAT(argv+argc, 5); argc++;
      SETFLOAT(argv+argc, 14); argc++;
      SETFLOAT(argv+argc, c->p_min); argc++;
      SETFLOAT(argv+argc, c->p_max); argc++;
      SETFLOAT(argv+argc, 0); argc++;
      SETFLOAT(argv+argc, 0); argc++;
      SETSYMBOL(argv+argc, s); argc++;
      SETSYMBOL(argv+argc, s); argc++;
      SETSYMBOL(argv+argc, gensym("empty")); argc++;
      SETFLOAT(argv+argc, 0); argc++;
      SETFLOAT(argv+argc, -6); argc++;
      SETFLOAT(argv+argc, 0); argc++;
      SETFLOAT(argv+argc, fn2); argc++;
      SETFLOAT(argv+argc, c->p_type==FAUST_UI_TYPE_BARGRAPH?gray:white); argc++;
      SETFLOAT(argv+argc, black); argc++;
      SETFLOAT(argv+argc, black); argc++;
      SETFLOAT(argv+argc, 256); argc++;
      typedmess(ui->s_thing, gensym("obj"), argc, argv);
      if (s->s_thing) {
        FAUSTFLOAT val = faustflt(x, c->p_zone);
        gui_update(val, c->p_uirecv);
        c->p_uival = val;
      } else {
        // this shouldn't happen
        pd_error(x->f_owner, "faustgen2~: can't initialize %s - gui", s->s_name);
      }
      argc = 0;
      break;
    default:
      // this can't happen
      pd_error(x->f_owner, "faustgen2~: invalid UI type - gui");
      break;
    }
    c = c->p_next;
  }
  // Add the special panic, init and active controls.
  t_symbol *s;
  if (x->f_voices) {
    s = make_sym(unique_name, gensym("panic"));
    SETFLOAT(argv+argc, wd-58); argc++;
    SETFLOAT(argv+argc, 3); argc++;
    SETSYMBOL(argv+argc, gensym("bng")); argc++;
    SETFLOAT(argv+argc, 15); argc++;
    SETFLOAT(argv+argc, 250); argc++;
    SETFLOAT(argv+argc, 50); argc++;
    SETFLOAT(argv+argc, 1); argc++;
    SETSYMBOL(argv+argc, s); argc++;
    SETSYMBOL(argv+argc, s); argc++;
    SETSYMBOL(argv+argc, gensym("empty")); argc++;
    SETFLOAT(argv+argc, 0); argc++;
    SETFLOAT(argv+argc, -6); argc++;
    SETFLOAT(argv+argc, 0); argc++;
    SETFLOAT(argv+argc, fn1); argc++;
    SETFLOAT(argv+argc, gray); argc++;
    SETFLOAT(argv+argc, black); argc++;
    SETFLOAT(argv+argc, black); argc++;
    typedmess(ui->s_thing, gensym("obj"), argc, argv);
    argc = 0;
    if (x->f_panic_recv) {
      x->f_panic_recv->uisym = s;
      x->f_panic_recv->lname = NULL;
    } else
      x->f_panic_recv = faust_ui_receive_new(x, s, NULL, 3);
  }
  s = make_sym(unique_name, gensym("init"));
  SETFLOAT(argv+argc, wd-38); argc++;
  SETFLOAT(argv+argc, 3); argc++;
  SETSYMBOL(argv+argc, gensym("bng")); argc++;
  SETFLOAT(argv+argc, 15); argc++;
  SETFLOAT(argv+argc, 250); argc++;
  SETFLOAT(argv+argc, 50); argc++;
  SETFLOAT(argv+argc, 1); argc++;
  SETSYMBOL(argv+argc, s); argc++;
  SETSYMBOL(argv+argc, s); argc++;
  SETSYMBOL(argv+argc, gensym("empty")); argc++;
  SETFLOAT(argv+argc, 0); argc++;
  SETFLOAT(argv+argc, -6); argc++;
  SETFLOAT(argv+argc, 0); argc++;
  SETFLOAT(argv+argc, fn1); argc++;
  SETFLOAT(argv+argc, white); argc++;
  SETFLOAT(argv+argc, black); argc++;
  SETFLOAT(argv+argc, black); argc++;
  typedmess(ui->s_thing, gensym("obj"), argc, argv);
  argc = 0;
  if (x->f_init_recv) {
    x->f_init_recv->uisym = s;
    x->f_init_recv->lname = NULL;
  } else
    x->f_init_recv = faust_ui_receive_new(x, s, NULL, 2);
  s = make_sym(unique_name, gensym("active"));
  SETFLOAT(argv+argc, wd-18); argc++;
  SETFLOAT(argv+argc, 3); argc++;
  SETSYMBOL(argv+argc, gensym("tgl")); argc++;
  SETFLOAT(argv+argc, 15); argc++;
  SETFLOAT(argv+argc, 1); argc++;
  SETSYMBOL(argv+argc, s); argc++;
  SETSYMBOL(argv+argc, s); argc++;
  SETSYMBOL(argv+argc, gensym("empty")); argc++;
  SETFLOAT(argv+argc, 0); argc++;
  SETFLOAT(argv+argc, -6); argc++;
  SETFLOAT(argv+argc, 0); argc++;
  SETFLOAT(argv+argc, fn1); argc++;
  SETFLOAT(argv+argc, white); argc++;
  SETFLOAT(argv+argc, black); argc++;
  SETFLOAT(argv+argc, black); argc++;
  SETFLOAT(argv+argc, 1); argc++;
  SETFLOAT(argv+argc, 1); argc++;
  typedmess(ui->s_thing, gensym("obj"), argc, argv);
  argc = 0;
  if (x->f_active_recv) {
    x->f_active_recv->uisym = s;
    x->f_active_recv->lname = NULL;
  } else
    x->f_active_recv = faust_ui_receive_new(x, s, NULL, 1);
}

// Stripped-down version of the above which just creates the receivers without
// touching the GUI. To be used for secondary instances in old-style polyphony.
void faust_ui_manager_gui2(t_faust_ui_manager *x,
                           t_symbol *unique_name, t_symbol *instance_name)
{
  t_faust_ui *c = x->f_uis;
  while (c) {
    if (c->p_voice) {
      // skip voice controls
      c = c->p_next;
      continue;
    }
    t_symbol *s = make_sym(unique_name, c->p_longname);
    c->p_uisym = s;
    if (c->p_uirecv) {
      // No need to recreate any existing receiver, just make sure that the
      // data is up-to-date.
      c->p_uirecv->uisym = s;
      c->p_uirecv->lname = c->p_longname;
    } else
      c->p_uirecv = faust_ui_receive_new(x, s, c->p_longname, 0);
    c = c->p_next;
  }
  // Add the special panic, init and active controls.
  t_symbol *s;
  if (x->f_voices) {
    s = make_sym(unique_name, gensym("panic"));
    if (x->f_panic_recv) {
      x->f_panic_recv->uisym = s;
      x->f_panic_recv->lname = NULL;
    } else
      x->f_panic_recv = faust_ui_receive_new(x, s, NULL, 3);
  }
  s = make_sym(unique_name, gensym("init"));
  if (x->f_init_recv) {
    x->f_init_recv->uisym = s;
    x->f_init_recv->lname = NULL;
  } else
    x->f_init_recv = faust_ui_receive_new(x, s, NULL, 2);
  s = make_sym(unique_name, gensym("active"));
  if (x->f_active_recv) {
    x->f_active_recv->uisym = s;
    x->f_active_recv->lname = NULL;
  } else
    x->f_active_recv = faust_ui_receive_new(x, s, NULL, 1);
}

// Receive a value from the GUI.
static void faust_ui_receive(t_faust_ui_proxy *r, t_floatarg v)
{
  if (r->type) {
    // Special active receiver, we need to pass this on to our grandparent
    // faustgen2~ object.
    t_object *ob = r->owner->f_owner;
    if (ob) {
      t_atom a;
      SETFLOAT(&a, v);
      //logpost(r->owner->f_owner, 3, "%s = %g", r->uisym->s_name, v);
      typedmess(&ob->ob_pd, gensym("active"), 1, &a);
    } else {
      pd_error(r->owner->f_owner, "faustgen2~: parent not found - gui");
    }
  } else if (r->type == 0 && !r->recursive) {
    t_faust_ui* c = faust_ui_manager_get(r->owner, r->lname);
    if (c) {
      //logpost(r->owner->f_owner, 3, "%s = %g", r->uisym->s_name, v);
      setfaustflt(r->owner, c->p_zone, v);
    }
  }
}

static void faust_bang_receive(t_faust_ui_proxy *r)
{
  // special init or panic receiver, we can handle this right here
  if (r->type == 2)
    faust_ui_manager_restore_default(r->owner);
  else if (r->type == 3)
    faust_ui_manager_all_notes_off(r->owner);
}
