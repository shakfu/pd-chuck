#include "pdarray.h"

#include "chuck_oo.h"
#include "chuck_vm.h"

#include <cmath>

static f_pdarray_lookup g_lookup = NULL;

// member offsets, the same for every VM that binds the class
static t_CKUINT pdarray_offset_state = 0;
static t_CKUINT pdarray_offset_slot = 0;

void pdarray_set_lookup(f_pdarray_lookup lookup)
{
    g_lookup = lookup;
}

// pd_findbyclass is a pointer check unless the name is bound more than once
static void resolve(PdArraySlot& s)
{
    s.array = (t_garray*)pd_findbyclass(s.name, garray_class);
    if (s.array == NULL || !garray_getfloatwords(s.array, &s.n, &s.vec)) {
        s.array = NULL;
        s.vec = NULL;
        s.n = 0;
    }
}

void pdarray_refresh(PdArrayState* state)
{
    for (int i = 0; i < state->count; i++) {
        resolve(state->slots[i]);
    }
}

// garray_redraw only queues GUI work, as tabwrite~ relies on in its perform
void pdarray_flush(PdArrayState* state)
{
    for (int i = 0; i < state->count; i++) {
        PdArraySlot& s = state->slots[i];
        if (s.dirty) {
            if (s.array) {
                garray_redraw(s.array);
            }
            s.dirty = false;
        }
    }
}

// -1 when all slots are taken
static int slot_for(PdArrayState* state, t_symbol* name)
{
    for (int i = 0; i < state->count; i++) {
        if (state->slots[i].name == name) {
            return i;
        }
    }
    if (state->count == PDARRAY_MAX_SLOTS) {
        return -1;
    }
    PdArraySlot& s = state->slots[state->count];
    s.name = name;
    s.dirty = false;
    resolve(s);
    return state->count++;
}

// NULL until name() is called and while no array has the name
static PdArraySlot* live_slot(PdArrayState* st, t_CKINT slot)
{
    if (st == NULL || slot < 0 || st->slots[slot].vec == NULL) {
        return NULL;
    }
    return &st->slots[slot];
}

static const char* slot_name(PdArrayState* st, t_CKINT slot)
{
    return (st && slot >= 0) ? st->slots[slot].name->s_name : "";
}


//-----------------------------------------------------------------------------
// HostBuffer

static PdArrayState* state_of(Chuck_Object* self)
{
    return (PdArrayState*)OBJ_MEMBER_INT(self, pdarray_offset_state);
}

static PdArraySlot* slot_of(Chuck_Object* self)
{
    return live_slot(state_of(self), OBJ_MEMBER_INT(self, pdarray_offset_slot));
}

CK_DLL_CTOR(pdarray_ctor)
{
    // the host frees the state after the VM, so caching it here is safe
    OBJ_MEMBER_INT(SELF, pdarray_offset_state) = (t_CKINT)(g_lookup ? g_lookup(VM) : NULL);
    OBJ_MEMBER_INT(SELF, pdarray_offset_slot) = -1;
}

CK_DLL_MFUN(pdarray_set_name)
{
    PdArrayState* st = state_of(SELF);
    Chuck_String* name = GET_NEXT_STRING(ARGS);
    t_CKINT slot = (st && name) ? slot_for(st, gensym(name->c_str())) : -1;
    OBJ_MEMBER_INT(SELF, pdarray_offset_slot) = slot;
    RETURN->v_string = (Chuck_String*)API->object->create_string(
        VM, slot_name(st, slot), FALSE);
}

CK_DLL_MFUN(pdarray_get_name)
{
    RETURN->v_string = (Chuck_String*)API->object->create_string(
        VM, slot_name(state_of(SELF), OBJ_MEMBER_INT(SELF, pdarray_offset_slot)), FALSE);
}

CK_DLL_MFUN(pdarray_ready)
{
    RETURN->v_int = slot_of(SELF) != NULL;
}

CK_DLL_MFUN(pdarray_frames)
{
    PdArraySlot* s = slot_of(SELF);
    RETURN->v_int = s ? s->n : 0;
}

// a Pd array is one channel; the channel arguments match HostBuffer in the
// other hosts
CK_DLL_MFUN(pdarray_channels)
{
    RETURN->v_int = slot_of(SELF) ? 1 : 0;
}

// Pd arrays carry no rate of their own; they play at the patch's
CK_DLL_MFUN(pdarray_sample_rate)
{
    RETURN->v_float = slot_of(SELF) ? (t_CKFLOAT)API->vm->srate(VM) : 0;
}

static t_CKFLOAT peek(Chuck_Object* self, t_CKINT i, t_CKINT chan)
{
    PdArraySlot* s = slot_of(self);
    return (s && chan == 0 && i >= 0 && i < s->n) ? s->vec[i].w_float : 0;
}

static void poke(Chuck_Object* self, t_CKINT i, t_CKINT chan, t_CKFLOAT value)
{
    PdArraySlot* s = slot_of(self);
    if (s && chan == 0 && i >= 0 && i < s->n) {
        s->vec[i].w_float = (t_float)value;
        s->dirty = true;
    }
}

CK_DLL_MFUN(pdarray_peek)
{
    t_CKINT i = GET_NEXT_INT(ARGS);
    RETURN->v_float = peek(SELF, i, 0);
}

CK_DLL_MFUN(pdarray_peek_chan)
{
    t_CKINT i = GET_NEXT_INT(ARGS);
    t_CKINT chan = GET_NEXT_INT(ARGS);
    RETURN->v_float = peek(SELF, i, chan);
}

CK_DLL_MFUN(pdarray_poke)
{
    t_CKINT i = GET_NEXT_INT(ARGS);
    t_CKFLOAT value = GET_NEXT_FLOAT(ARGS);
    poke(SELF, i, 0, value);
    RETURN->v_float = value;
}

CK_DLL_MFUN(pdarray_poke_chan)
{
    t_CKINT i = GET_NEXT_INT(ARGS);
    t_CKINT chan = GET_NEXT_INT(ARGS);
    t_CKFLOAT value = GET_NEXT_FLOAT(ARGS);
    poke(SELF, i, chan, value);
    RETURN->v_float = value;
}


//-----------------------------------------------------------------------------
// HostBufPlay: plays a Pd array as a UGen

static t_CKUINT pdarrayplay_offset_data = 0;

struct ArrayPlay {
    PdArrayState* state;
    t_CKINT slot;
    double pos;      // in frames
    double rate;
    t_CKINT loop;
    t_CKINT interp;  // 0 none, 1 linear
    t_CKINT chan;    // only 0 plays: a Pd array is one channel
    bool warned;     // posted the silent-channel warning
};

static ArrayPlay* play_of(Chuck_Object* self)
{
    return (ArrayPlay*)OBJ_MEMBER_INT(self, pdarrayplay_offset_data);
}

CK_DLL_CTOR(pdarrayplay_ctor)
{
    ArrayPlay* p = new ArrayPlay;
    p->state = g_lookup ? g_lookup(VM) : NULL;
    p->slot = -1;
    p->pos = 0;
    p->rate = 1;
    p->loop = 0;
    p->interp = 1;
    p->chan = 0;
    p->warned = false;
    OBJ_MEMBER_INT(SELF, pdarrayplay_offset_data) = (t_CKINT)p;
}

CK_DLL_DTOR(pdarrayplay_dtor)
{
    delete play_of(SELF);
    OBJ_MEMBER_INT(SELF, pdarrayplay_offset_data) = 0;
}

CK_DLL_TICK(pdarrayplay_tick)
{
    ArrayPlay* p = play_of(SELF);
    PdArraySlot* s = live_slot(p->state, p->slot);
    *out = 0;
    if (s == NULL || s->n <= 0 || p->chan != 0) {
        return TRUE;
    }
    double n = (double)s->n;
    if (p->loop) {
        p->pos = std::fmod(p->pos, n);
        if (p->pos < 0) {
            p->pos += n;
        }
    }
    if (p->pos >= 0 && p->pos < n) {
        long i = (long)p->pos;
        double x = s->vec[i].w_float;
        if (p->interp) {
            // past the last frame: wrap when looping, else hold
            long j = (i + 1 < s->n) ? i + 1 : (p->loop ? 0 : i);
            x += (p->pos - i) * (s->vec[j].w_float - x);
        }
        *out = (SAMPLE)x;
        p->pos += p->rate;
    }
    return TRUE;
}

CK_DLL_MFUN(pdarrayplay_set_name)
{
    ArrayPlay* p = play_of(SELF);
    Chuck_String* name = GET_NEXT_STRING(ARGS);
    p->slot = (p->state && name) ? slot_for(p->state, gensym(name->c_str())) : -1;
    p->pos = 0;
    RETURN->v_string = (Chuck_String*)API->object->create_string(
        VM, slot_name(p->state, p->slot), FALSE);
}

CK_DLL_MFUN(pdarrayplay_get_name)
{
    ArrayPlay* p = play_of(SELF);
    RETURN->v_string = (Chuck_String*)API->object->create_string(
        VM, slot_name(p->state, p->slot), FALSE);
}

CK_DLL_MFUN(pdarrayplay_ready)
{
    ArrayPlay* p = play_of(SELF);
    RETURN->v_int = live_slot(p->state, p->slot) != NULL;
}

CK_DLL_MFUN(pdarrayplay_frames)
{
    ArrayPlay* p = play_of(SELF);
    PdArraySlot* s = live_slot(p->state, p->slot);
    RETURN->v_int = s ? s->n : 0;
}

CK_DLL_MFUN(pdarrayplay_set_pos)
{
    play_of(SELF)->pos = (double)GET_NEXT_INT(ARGS);
    RETURN->v_int = (t_CKINT)play_of(SELF)->pos;
}

CK_DLL_MFUN(pdarrayplay_get_pos)
{
    RETURN->v_int = (t_CKINT)std::floor(play_of(SELF)->pos);
}

CK_DLL_MFUN(pdarrayplay_set_rate)
{
    RETURN->v_float = play_of(SELF)->rate = GET_NEXT_FLOAT(ARGS);
}

CK_DLL_MFUN(pdarrayplay_get_rate)
{
    RETURN->v_float = play_of(SELF)->rate;
}

CK_DLL_MFUN(pdarrayplay_set_loop)
{
    RETURN->v_int = play_of(SELF)->loop = GET_NEXT_INT(ARGS) != 0;
}

CK_DLL_MFUN(pdarrayplay_get_loop)
{
    RETURN->v_int = play_of(SELF)->loop;
}

CK_DLL_MFUN(pdarrayplay_set_interp)
{
    RETURN->v_int = play_of(SELF)->interp = GET_NEXT_INT(ARGS) != 0;
}

CK_DLL_MFUN(pdarrayplay_get_interp)
{
    RETURN->v_int = play_of(SELF)->interp;
}

// code written for a multichannel buffer~ would otherwise go silent unseen
CK_DLL_MFUN(pdarrayplay_set_channel)
{
    ArrayPlay* p = play_of(SELF);
    p->chan = GET_NEXT_INT(ARGS);
    if (p->chan != 0 && !p->warned) {
        logpost(NULL, PD_NORMAL, "[chuck~ warning] HostBufPlay: channel %ld "
                "plays silence, a Pd array has only channel 0", (long)p->chan);
        p->warned = true;
    }
    RETURN->v_int = p->chan;
}

CK_DLL_MFUN(pdarrayplay_get_channel)
{
    RETURN->v_int = play_of(SELF)->chan;
}

static void query_pdarrayplay(Chuck_DL_Query* QUERY)
{
    QUERY->begin_class(QUERY, "HostBufPlay", "UGen");
    QUERY->doc_class(QUERY, "plays a named Pd array; silent while no array "
                            "has the name");
    QUERY->add_ctor(QUERY, pdarrayplay_ctor);
    QUERY->add_dtor(QUERY, pdarrayplay_dtor);
    QUERY->add_ugen_func(QUERY, pdarrayplay_tick, NULL, 1, 1);
    pdarrayplay_offset_data = QUERY->add_mvar(QUERY, "int", "@data", FALSE);

    QUERY->add_mfun(QUERY, pdarrayplay_set_name, "string", "name");
    QUERY->add_arg(QUERY, "string", "name");
    QUERY->doc_func(QUERY, "play the array with this name, from frame 0");
    QUERY->add_mfun(QUERY, pdarrayplay_get_name, "string", "name");
    QUERY->doc_func(QUERY, "the array name, or \"\"");
    QUERY->add_mfun(QUERY, pdarrayplay_ready, "int", "ready");
    QUERY->doc_func(QUERY, "1 if the array exists");
    QUERY->add_mfun(QUERY, pdarrayplay_frames, "int", "frames");
    QUERY->doc_func(QUERY, "length in points; 0 if not ready");
    QUERY->add_mfun(QUERY, pdarrayplay_set_pos, "int", "pos");
    QUERY->add_arg(QUERY, "int", "frame");
    QUERY->doc_func(QUERY, "set the play position in points");
    QUERY->add_mfun(QUERY, pdarrayplay_get_pos, "int", "pos");
    QUERY->doc_func(QUERY, "the play position in points");
    QUERY->add_mfun(QUERY, pdarrayplay_set_rate, "float", "rate");
    QUERY->add_arg(QUERY, "float", "rate");
    QUERY->doc_func(QUERY, "points per sample; 1 is normal, negative plays backward");
    QUERY->add_mfun(QUERY, pdarrayplay_get_rate, "float", "rate");
    QUERY->doc_func(QUERY, "playback rate");
    QUERY->add_mfun(QUERY, pdarrayplay_set_loop, "int", "loop");
    QUERY->add_arg(QUERY, "int", "loop");
    QUERY->doc_func(QUERY, "1 loops; 0 (default) stops at either end");
    QUERY->add_mfun(QUERY, pdarrayplay_get_loop, "int", "loop");
    QUERY->doc_func(QUERY, "1 if looping");
    QUERY->add_mfun(QUERY, pdarrayplay_set_interp, "int", "interp");
    QUERY->add_arg(QUERY, "int", "interp");
    QUERY->doc_func(QUERY, "1 (default) linear interpolation; 0 none");
    QUERY->add_mfun(QUERY, pdarrayplay_get_interp, "int", "interp");
    QUERY->doc_func(QUERY, "1 if interpolating");
    QUERY->add_mfun(QUERY, pdarrayplay_set_channel, "int", "channel");
    QUERY->add_arg(QUERY, "int", "chan");
    QUERY->doc_func(QUERY, "channel to play; a Pd array has only channel 0, "
                           "any other is silent");
    QUERY->add_mfun(QUERY, pdarrayplay_get_channel, "int", "channel");
    QUERY->doc_func(QUERY, "channel being played");
    QUERY->end_class(QUERY);
}

t_CKBOOL CK_DLL_CALL pdarray_query(Chuck_DL_Query* QUERY)
{
    QUERY->begin_class(QUERY, "HostBuffer", "Object");
    QUERY->doc_class(QUERY, "reads and writes the points of a named Pd array");
    QUERY->add_ctor(QUERY, pdarray_ctor);
    pdarray_offset_state = QUERY->add_mvar(QUERY, "int", "@state", FALSE);
    pdarray_offset_slot = QUERY->add_mvar(QUERY, "int", "@slot", FALSE);

    QUERY->add_mfun(QUERY, pdarray_set_name, "string", "name");
    QUERY->add_arg(QUERY, "string", "name");
    QUERY->doc_func(QUERY, "refer to the array with this name; returns the "
                           "name, or \"\" if 64 names are already in use");
    QUERY->add_mfun(QUERY, pdarray_get_name, "string", "name");
    QUERY->doc_func(QUERY, "the array name, or \"\"");
    QUERY->add_mfun(QUERY, pdarray_ready, "int", "ready");
    QUERY->doc_func(QUERY, "1 if the array exists");
    QUERY->add_mfun(QUERY, pdarray_frames, "int", "frames");
    QUERY->doc_func(QUERY, "length in points; 0 if not ready");
    QUERY->add_mfun(QUERY, pdarray_channels, "int", "channels");
    QUERY->doc_func(QUERY, "1 if ready, else 0: a Pd array is one channel");
    QUERY->add_mfun(QUERY, pdarray_sample_rate, "float", "sampleRate");
    QUERY->doc_func(QUERY, "the patch sample rate; 0 if not ready");
    QUERY->add_mfun(QUERY, pdarray_peek, "float", "peek");
    QUERY->add_arg(QUERY, "int", "frame");
    QUERY->doc_func(QUERY, "value at a point; 0 if out of range");
    QUERY->add_mfun(QUERY, pdarray_peek_chan, "float", "peek");
    QUERY->add_arg(QUERY, "int", "frame");
    QUERY->add_arg(QUERY, "int", "chan");
    QUERY->doc_func(QUERY, "value at a point of channel 0; 0 for any other channel");
    QUERY->add_mfun(QUERY, pdarray_poke, "float", "poke");
    QUERY->add_arg(QUERY, "int", "frame");
    QUERY->add_arg(QUERY, "float", "value");
    QUERY->doc_func(QUERY, "write value at a point; ignored if out of range");
    QUERY->add_mfun(QUERY, pdarray_poke_chan, "float", "poke");
    QUERY->add_arg(QUERY, "int", "frame");
    QUERY->add_arg(QUERY, "int", "chan");
    QUERY->add_arg(QUERY, "float", "value");
    QUERY->doc_func(QUERY, "write value at a point of channel 0; ignored for any other channel");
    QUERY->end_class(QUERY);

    query_pdarrayplay(QUERY);
    return TRUE;
}
