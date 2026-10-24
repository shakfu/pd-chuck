// HostIn / HostMsg: Pd messages into ChucK (see docs/dev/bind.md).
// No Pd dependency: the host converts atoms and calls pdin_deliver.

#pragma once

#include "chuck_dl.h"

#include <deque>
#include <string>
#include <vector>

// max atoms carried by one message after its name
#define PDIN_MAX_ATOMS 32
// messages a HostIn holds before recv(); further messages are dropped
#define PDIN_MAX_PENDING 64

enum PdInType { PDIN_FLOAT, PDIN_STRING };

// strings must stay valid for the life of the process, as Pd symbols do
struct PdInAtom {
    PdInType type;
    union {
        t_CKFLOAT f;
        const char* s;
    };
};

struct PdInMessage {
    const char* name;
    long argc;
    PdInAtom argv[PDIN_MAX_ATOMS];
};

struct PdInState;

// one per HostIn object; owned by the object, listed in its state while live
struct PdInEntry {
    PdInState* state;
    Chuck_Object* obj;
    Chuck_VM_Shred* shred;              // creating shred, for the watcher
    bool registered;
    std::vector<std::string> names;     // from addAddress()
    std::deque<PdInMessage> pending;
};

// per-VM state; the host owns one per ChucK instance and frees it after the VM
struct PdInState {
    std::vector<PdInEntry*> entries;
};

// maps a VM to its state; NULL once the host is tearing down
typedef PdInState* (*f_pdin_lookup)(Chuck_VM* vm);

// set once, before any VM binds HostIn
void pdin_set_lookup(f_pdin_lookup lookup);

// ChucK::bind() query that declares HostIn and HostMsg
t_CKBOOL CK_DLL_CALL pdin_query(Chuck_DL_Query* QUERY);

// shreds watcher (ckvm_shreds_watch_REMOVE, userdata = the PdInState).
// a removed shred leaks its locals, so their dtors never unregister them
void CK_DLL_CALL pdin_watcher(Chuck_VM_Shred* shred, t_CKINT code,
                              t_CKINT param, Chuck_VM* vm, void* userdata);

// hand a message to every HostIn listening for name and broadcast it; argc
// above PDIN_MAX_ATOMS is truncated. must not run concurrently with
// ChucK::run(); returns the number of HostIn objects that took it
int pdin_deliver(PdInState* state, const char* name, long argc,
                 const PdInAtom* argv);
