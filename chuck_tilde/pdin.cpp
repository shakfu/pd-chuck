#include "pdin.h"

#include "chuck_oo.h"
#include "chuck_vm.h"

#include <algorithm>

static f_pdin_lookup g_lookup = NULL;

// member offsets, the same for every VM that binds the classes
static t_CKUINT pdin_offset_entry = 0;
static t_CKUINT pdmsg_offset_data = 0;

void pdin_set_lookup(f_pdin_lookup lookup)
{
    g_lookup = lookup;
}

static void unregister(PdInEntry* e)
{
    if (!e->registered) {
        return;
    }
    std::vector<PdInEntry*>& v = e->state->entries;
    v.erase(std::remove(v.begin(), v.end(), e), v.end());
    e->registered = false;
    e->pending.clear();
}

static bool listens(const PdInEntry* e, const char* name)
{
    for (const std::string& n : e->names) {
        if (n == name) {
            return true;
        }
    }
    return false;
}

int pdin_deliver(PdInState* state, const char* name, long argc,
                 const PdInAtom* argv)
{
    PdInMessage m;
    m.name = name;
    m.argc = std::max(0L, std::min(argc, (long)PDIN_MAX_ATOMS));
    std::copy(argv, argv + m.argc, m.argv);
    int taken = 0;
    // broadcast_local only moves waiting shreds to the shreduler; it runs
    // no ChucK code, so entries cannot change underneath this loop
    for (PdInEntry* e : state->entries) {
        if (listens(e, name) && e->pending.size() < PDIN_MAX_PENDING) {
            e->pending.push_back(m);
            ((Chuck_Event*)e->obj)->broadcast_local();
            taken++;
        }
    }
    return taken;
}

void CK_DLL_CALL pdin_watcher(Chuck_VM_Shred* shred, t_CKINT code,
                              t_CKINT param, Chuck_VM* vm, void* userdata)
{
    PdInState* state = (PdInState*)userdata;
    if (state == NULL) {
        return;
    }
    for (size_t i = 0; i < state->entries.size();) {
        PdInEntry* e = state->entries[i];
        if (e->shred == shred) {
            unregister(e);
        } else {
            i++;
        }
    }
}


//-----------------------------------------------------------------------------
// ChucK classes

static PdInEntry* entry_of(Chuck_Object* self)
{
    return (PdInEntry*)OBJ_MEMBER_INT(self, pdin_offset_entry);
}

static PdInMessage* message_of(Chuck_Object* self)
{
    return (PdInMessage*)OBJ_MEMBER_INT(self, pdmsg_offset_data);
}

CK_DLL_CTOR(pdin_ctor)
{
    PdInEntry* e = new PdInEntry;
    e->state = g_lookup ? g_lookup(VM) : NULL;
    e->obj = SELF;
    e->shred = SHRED;
    e->registered = e->state != NULL;
    if (e->registered) {
        e->state->entries.push_back(e);
    }
    OBJ_MEMBER_INT(SELF, pdin_offset_entry) = (t_CKINT)e;
}

// the host frees the state after the VM, so e->state is valid here
CK_DLL_DTOR(pdin_dtor)
{
    PdInEntry* e = entry_of(SELF);
    if (e) {
        unregister(e);
        delete e;
        OBJ_MEMBER_INT(SELF, pdin_offset_entry) = 0;
    }
}

CK_DLL_MFUN(pdin_add_address)
{
    PdInEntry* e = entry_of(SELF);
    Chuck_String* name = GET_NEXT_STRING(ARGS);
    if (e && name) {
        e->names.push_back(name->str());
    }
}

CK_DLL_MFUN(pdin_recv)
{
    PdInEntry* e = entry_of(SELF);
    Chuck_Object* msg = GET_NEXT_OBJECT(ARGS);
    RETURN->v_int = 0;
    if (e == NULL || msg == NULL || e->pending.empty()) {
        return;
    }
    *message_of(msg) = e->pending.front();
    e->pending.pop_front();
    RETURN->v_int = 1;
}

CK_DLL_CTOR(pdmsg_ctor)
{
    PdInMessage* m = new PdInMessage;
    m->name = "";
    m->argc = 0;
    OBJ_MEMBER_INT(SELF, pdmsg_offset_data) = (t_CKINT)m;
}

CK_DLL_DTOR(pdmsg_dtor)
{
    delete message_of(SELF);
    OBJ_MEMBER_INT(SELF, pdmsg_offset_data) = 0;
}

static const PdInAtom* arg(Chuck_Object* self, t_CKINT i)
{
    PdInMessage* m = message_of(self);
    return (m && i >= 0 && i < m->argc) ? &m->argv[i] : NULL;
}

CK_DLL_MFUN(pdmsg_name)
{
    PdInMessage* m = message_of(SELF);
    RETURN->v_string = (Chuck_String*)API->object->create_string(
        VM, m ? m->name : "", FALSE);
}

CK_DLL_MFUN(pdmsg_num_args)
{
    PdInMessage* m = message_of(SELF);
    RETURN->v_int = m ? m->argc : 0;
}

// Pd numbers are all floats, so getInt truncates
CK_DLL_MFUN(pdmsg_get_int)
{
    const PdInAtom* a = arg(SELF, GET_NEXT_INT(ARGS));
    RETURN->v_int = (a && a->type == PDIN_FLOAT) ? (t_CKINT)a->f : 0;
}

CK_DLL_MFUN(pdmsg_get_float)
{
    const PdInAtom* a = arg(SELF, GET_NEXT_INT(ARGS));
    RETURN->v_float = (a && a->type == PDIN_FLOAT) ? a->f : 0;
}

CK_DLL_MFUN(pdmsg_get_string)
{
    const PdInAtom* a = arg(SELF, GET_NEXT_INT(ARGS));
    RETURN->v_string = (Chuck_String*)API->object->create_string(
        VM, (a && a->type == PDIN_STRING) ? a->s : "", FALSE);
}

t_CKBOOL CK_DLL_CALL pdin_query(Chuck_DL_Query* QUERY)
{
    // HostMsg first: HostIn.recv() takes one
    QUERY->begin_class(QUERY, "HostMsg", "Object");
    QUERY->doc_class(QUERY, "one message received by a HostIn");
    QUERY->add_ctor(QUERY, pdmsg_ctor);
    QUERY->add_dtor(QUERY, pdmsg_dtor);
    pdmsg_offset_data = QUERY->add_mvar(QUERY, "int", "@data", FALSE);

    QUERY->add_mfun(QUERY, pdmsg_name, "string", "name");
    QUERY->doc_func(QUERY, "message name: 'note' in 'msg note 60'");
    QUERY->add_mfun(QUERY, pdmsg_num_args, "int", "numArgs");
    QUERY->doc_func(QUERY, "number of arguments after the name");
    QUERY->add_mfun(QUERY, pdmsg_get_int, "int", "getInt");
    QUERY->add_arg(QUERY, "int", "index");
    QUERY->doc_func(QUERY, "argument as int, truncated; strings give 0");
    QUERY->add_mfun(QUERY, pdmsg_get_float, "float", "getFloat");
    QUERY->add_arg(QUERY, "int", "index");
    QUERY->doc_func(QUERY, "argument as float; strings give 0");
    QUERY->add_mfun(QUERY, pdmsg_get_string, "string", "getString");
    QUERY->add_arg(QUERY, "int", "index");
    QUERY->doc_func(QUERY, "argument as string; numbers give \"\"");
    QUERY->end_class(QUERY);

    QUERY->begin_class(QUERY, "HostIn", "Event");
    QUERY->doc_class(QUERY, "receives 'msg <name> ...' sent to chuck~; "
                            "broadcasts once per message");
    QUERY->add_ctor(QUERY, pdin_ctor);
    QUERY->add_dtor(QUERY, pdin_dtor);
    pdin_offset_entry = QUERY->add_mvar(QUERY, "int", "@entry", FALSE);

    QUERY->add_mfun(QUERY, pdin_add_address, "void", "addAddress");
    QUERY->add_arg(QUERY, "string", "name");
    QUERY->doc_func(QUERY, "receive messages with this name");
    QUERY->add_mfun(QUERY, pdin_recv, "int", "recv");
    QUERY->add_arg(QUERY, "HostMsg", "msg");
    QUERY->doc_func(QUERY, "move the oldest message into msg; 0 if none");
    QUERY->end_class(QUERY);
    return TRUE;
}
