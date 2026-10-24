// HostBuffer: ChucK reads and writes Pd arrays; HostBufPlay plays one as a UGen
// (see docs/dev/bind.md).

#pragma once

#include "m_pd.h"
#include "chuck_dl.h"

// distinct array names per instance; slots are never freed before the host
#define PDARRAY_MAX_SLOTS 64

// array pointers are re-read every block: Pd can resize or free an array
// between blocks, but not during one
struct PdArraySlot {
    t_symbol* name;
    t_garray* array;    // NULL while no array has the name
    t_word* vec;
    int n;
    bool dirty;         // written this block; redrawn after ChucK::run()
};

// per-instance state; the host owns one per ChucK instance and frees it
// after the VM
struct PdArrayState {
    PdArraySlot slots[PDARRAY_MAX_SLOTS];
    int count = 0;
};

// maps a VM to its state; NULL once the host is tearing down
typedef PdArrayState* (*f_pdarray_lookup)(Chuck_VM* vm);

// set once, before any VM binds HostBuffer
void pdarray_set_lookup(f_pdarray_lookup lookup);

// ChucK::bind() query that declares HostBuffer and the HostBufPlay UGen
t_CKBOOL CK_DLL_CALL pdarray_query(Chuck_DL_Query* QUERY);

// perform routine, before ChucK::run(): look up every named array
void pdarray_refresh(PdArrayState* state);

// perform routine, after ChucK::run(): redraw arrays written in this block
void pdarray_flush(PdArrayState* state);
