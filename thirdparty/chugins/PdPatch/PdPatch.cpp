//-----------------------------------------------------------------------------
// PdPatch chugin -- wraps libpd to load and run Pure Data patches in ChucK
//-----------------------------------------------------------------------------

#include "chugin.h"
#include "PdPatchInternal.h"

// Forward declarations
CK_DLL_CTOR(pdpatch_ctor);
CK_DLL_DTOR(pdpatch_dtor);
CK_DLL_TICKF(pdpatch_tickf);

// Patch lifecycle
CK_DLL_MFUN(pdpatch_open);
CK_DLL_MFUN(pdpatch_close);
CK_DLL_MFUN(pdpatch_addSearchPath);

// Send messages to Pd
CK_DLL_MFUN(pdpatch_sendBang);
CK_DLL_MFUN(pdpatch_sendFloat);
CK_DLL_MFUN(pdpatch_sendSymbol);
CK_DLL_MFUN(pdpatch_sendList);
CK_DLL_MFUN(pdpatch_sendMessage);

// Receive messages from Pd
CK_DLL_MFUN(pdpatch_bind);
CK_DLL_MFUN(pdpatch_unbind);
CK_DLL_MFUN(pdpatch_getFloat);
CK_DLL_MFUN(pdpatch_getSymbol);
CK_DLL_MFUN(pdpatch_getBang);

// Pd array access
CK_DLL_MFUN(pdpatch_arraySize);
CK_DLL_MFUN(pdpatch_readArray);
CK_DLL_MFUN(pdpatch_writeArray);

// MIDI send
CK_DLL_MFUN(pdpatch_noteOn);
CK_DLL_MFUN(pdpatch_noteOff);
CK_DLL_MFUN(pdpatch_controlChange);
CK_DLL_MFUN(pdpatch_programChange);
CK_DLL_MFUN(pdpatch_pitchBend);
CK_DLL_MFUN(pdpatch_aftertouch);
CK_DLL_MFUN(pdpatch_polyAftertouch);

// Data offset for internal class pointer
t_CKINT pdpatch_data_offset = 0;

//-----------------------------------------------------------------------------
// Query function: ChucK calls this when loading the chugin
//-----------------------------------------------------------------------------
CK_DLL_QUERY(PdPatch)
{
    QUERY->setname(QUERY, "PdPatch");

    QUERY->begin_class(QUERY, "PdPatch", "UGen");
    QUERY->doc_class(QUERY, "Load and run Pure Data patches in ChucK. "
        "Wraps libpd for audio processing, messaging, array access, and MIDI. "
        "Fixed stereo (2 in / 2 out) with 64-sample block bridging.");

    QUERY->add_ctor(QUERY, pdpatch_ctor);
    QUERY->add_dtor(QUERY, pdpatch_dtor);

    // UGen tick function: 2 in, 2 out
    QUERY->add_ugen_funcf(QUERY, pdpatch_tickf, NULL, 2, 2);

    // --- Patch lifecycle ---

    QUERY->add_mfun(QUERY, pdpatch_open, "int", "open");
    QUERY->doc_func(QUERY, "Open a Pd patch file. Returns 0 on success, -1 on failure.");
    QUERY->add_arg(QUERY, "string", "patchFile");
    QUERY->add_arg(QUERY, "string", "dir");

    QUERY->add_mfun(QUERY, pdpatch_close, "void", "close");
    QUERY->doc_func(QUERY, "Close the currently loaded Pd patch.");

    QUERY->add_mfun(QUERY, pdpatch_addSearchPath, "void", "addSearchPath");
    QUERY->doc_func(QUERY, "Add a directory to Pd's abstraction search path.");
    QUERY->add_arg(QUERY, "string", "path");

    // --- Send messages TO Pd ---

    QUERY->add_mfun(QUERY, pdpatch_sendBang, "void", "sendBang");
    QUERY->doc_func(QUERY, "Send a bang to a named receiver in Pd.");
    QUERY->add_arg(QUERY, "string", "receiver");

    QUERY->add_mfun(QUERY, pdpatch_sendFloat, "void", "sendFloat");
    QUERY->doc_func(QUERY, "Send a float value to a named receiver in Pd.");
    QUERY->add_arg(QUERY, "string", "receiver");
    QUERY->add_arg(QUERY, "float", "value");

    QUERY->add_mfun(QUERY, pdpatch_sendSymbol, "void", "sendSymbol");
    QUERY->doc_func(QUERY, "Send a symbol to a named receiver in Pd.");
    QUERY->add_arg(QUERY, "string", "receiver");
    QUERY->add_arg(QUERY, "string", "symbol");

    QUERY->add_mfun(QUERY, pdpatch_sendList, "void", "sendList");
    QUERY->doc_func(QUERY, "Send a list of floats to a named receiver in Pd.");
    QUERY->add_arg(QUERY, "string", "receiver");
    QUERY->add_arg(QUERY, "float[]", "values");

    QUERY->add_mfun(QUERY, pdpatch_sendMessage, "void", "sendMessage");
    QUERY->doc_func(QUERY, "Send a typed message with float arguments to a named receiver in Pd.");
    QUERY->add_arg(QUERY, "string", "receiver");
    QUERY->add_arg(QUERY, "string", "msg");
    QUERY->add_arg(QUERY, "float[]", "args");

    // --- Receive messages FROM Pd ---

    QUERY->add_mfun(QUERY, pdpatch_bind, "void", "bind");
    QUERY->doc_func(QUERY, "Subscribe to messages from a named sender in Pd.");
    QUERY->add_arg(QUERY, "string", "receiver");

    QUERY->add_mfun(QUERY, pdpatch_unbind, "void", "unbind");
    QUERY->doc_func(QUERY, "Unsubscribe from a named sender in Pd.");
    QUERY->add_arg(QUERY, "string", "receiver");

    QUERY->add_mfun(QUERY, pdpatch_getFloat, "float", "getFloat");
    QUERY->doc_func(QUERY, "Get the last float received from a named sender in Pd.");
    QUERY->add_arg(QUERY, "string", "receiver");

    QUERY->add_mfun(QUERY, pdpatch_getSymbol, "string", "getSymbol");
    QUERY->doc_func(QUERY, "Get the last symbol received from a named sender in Pd.");
    QUERY->add_arg(QUERY, "string", "receiver");

    QUERY->add_mfun(QUERY, pdpatch_getBang, "int", "getBang");
    QUERY->doc_func(QUERY, "Returns 1 if a bang was received from the named sender since last call, 0 otherwise.");
    QUERY->add_arg(QUERY, "string", "receiver");

    // --- Pd array access ---

    QUERY->add_mfun(QUERY, pdpatch_arraySize, "int", "arraySize");
    QUERY->doc_func(QUERY, "Get the size of a named Pd array.");
    QUERY->add_arg(QUERY, "string", "name");

    QUERY->add_mfun(QUERY, pdpatch_readArray, "int", "readArray");
    QUERY->doc_func(QUERY, "Read from a named Pd array into a ChucK float array. Returns 0 on success.");
    QUERY->add_arg(QUERY, "string", "name");
    QUERY->add_arg(QUERY, "float[]", "dest");
    QUERY->add_arg(QUERY, "int", "offset");

    QUERY->add_mfun(QUERY, pdpatch_writeArray, "int", "writeArray");
    QUERY->doc_func(QUERY, "Write a ChucK float array into a named Pd array. Returns 0 on success.");
    QUERY->add_arg(QUERY, "string", "name");
    QUERY->add_arg(QUERY, "float[]", "src");
    QUERY->add_arg(QUERY, "int", "offset");

    // --- MIDI send ---

    QUERY->add_mfun(QUERY, pdpatch_noteOn, "void", "noteOn");
    QUERY->doc_func(QUERY, "Send a MIDI note-on to the Pd patch.");
    QUERY->add_arg(QUERY, "int", "channel");
    QUERY->add_arg(QUERY, "int", "pitch");
    QUERY->add_arg(QUERY, "int", "velocity");

    QUERY->add_mfun(QUERY, pdpatch_noteOff, "void", "noteOff");
    QUERY->doc_func(QUERY, "Send a MIDI note-off to the Pd patch.");
    QUERY->add_arg(QUERY, "int", "channel");
    QUERY->add_arg(QUERY, "int", "pitch");
    QUERY->add_arg(QUERY, "int", "velocity");

    QUERY->add_mfun(QUERY, pdpatch_controlChange, "void", "controlChange");
    QUERY->doc_func(QUERY, "Send a MIDI control change to the Pd patch.");
    QUERY->add_arg(QUERY, "int", "channel");
    QUERY->add_arg(QUERY, "int", "controller");
    QUERY->add_arg(QUERY, "int", "value");

    QUERY->add_mfun(QUERY, pdpatch_programChange, "void", "programChange");
    QUERY->doc_func(QUERY, "Send a MIDI program change to the Pd patch.");
    QUERY->add_arg(QUERY, "int", "channel");
    QUERY->add_arg(QUERY, "int", "program");

    QUERY->add_mfun(QUERY, pdpatch_pitchBend, "void", "pitchBend");
    QUERY->doc_func(QUERY, "Send a MIDI pitch bend to the Pd patch.");
    QUERY->add_arg(QUERY, "int", "channel");
    QUERY->add_arg(QUERY, "int", "value");

    QUERY->add_mfun(QUERY, pdpatch_aftertouch, "void", "aftertouch");
    QUERY->doc_func(QUERY, "Send a MIDI aftertouch (channel pressure) to the Pd patch.");
    QUERY->add_arg(QUERY, "int", "channel");
    QUERY->add_arg(QUERY, "int", "value");

    QUERY->add_mfun(QUERY, pdpatch_polyAftertouch, "void", "polyAftertouch");
    QUERY->doc_func(QUERY, "Send a MIDI poly aftertouch to the Pd patch.");
    QUERY->add_arg(QUERY, "int", "channel");
    QUERY->add_arg(QUERY, "int", "pitch");
    QUERY->add_arg(QUERY, "int", "value");

    // Internal data offset
    pdpatch_data_offset = QUERY->add_mvar(QUERY, "int", "@pd_data", false);

    QUERY->end_class(QUERY);

    return TRUE;
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CK_DLL_CTOR(pdpatch_ctor)
{
    OBJ_MEMBER_INT(SELF, pdpatch_data_offset) = 0;
    PdPatchInternal * pd = new PdPatchInternal(API->vm->srate(VM));
    OBJ_MEMBER_INT(SELF, pdpatch_data_offset) = (t_CKINT)pd;
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
CK_DLL_DTOR(pdpatch_dtor)
{
    PdPatchInternal * pd = (PdPatchInternal *)OBJ_MEMBER_INT(SELF, pdpatch_data_offset);
    if (pd)
    {
        delete pd;
        OBJ_MEMBER_INT(SELF, pdpatch_data_offset) = 0;
    }
}

//-----------------------------------------------------------------------------
// Tick function (multi-channel)
//-----------------------------------------------------------------------------
CK_DLL_TICKF(pdpatch_tickf)
{
    PdPatchInternal * pd = (PdPatchInternal *)OBJ_MEMBER_INT(SELF, pdpatch_data_offset);
    if (pd) pd->tick(in, out, nframes);
    return TRUE;
}

//-----------------------------------------------------------------------------
// Patch lifecycle
//-----------------------------------------------------------------------------
CK_DLL_MFUN(pdpatch_open)
{
    PdPatchInternal * pd = (PdPatchInternal *)OBJ_MEMBER_INT(SELF, pdpatch_data_offset);
    std::string patchFile = GET_NEXT_STRING_SAFE(ARGS);
    std::string dir = GET_NEXT_STRING_SAFE(ARGS);
    RETURN->v_int = pd->open(patchFile, dir);
}

CK_DLL_MFUN(pdpatch_close)
{
    PdPatchInternal * pd = (PdPatchInternal *)OBJ_MEMBER_INT(SELF, pdpatch_data_offset);
    pd->close();
}

CK_DLL_MFUN(pdpatch_addSearchPath)
{
    PdPatchInternal * pd = (PdPatchInternal *)OBJ_MEMBER_INT(SELF, pdpatch_data_offset);
    std::string path = GET_NEXT_STRING_SAFE(ARGS);
    pd->addSearchPath(path);
}

//-----------------------------------------------------------------------------
// Send messages TO Pd
//-----------------------------------------------------------------------------
CK_DLL_MFUN(pdpatch_sendBang)
{
    PdPatchInternal * pd = (PdPatchInternal *)OBJ_MEMBER_INT(SELF, pdpatch_data_offset);
    std::string receiver = GET_NEXT_STRING_SAFE(ARGS);
    pd->sendBang(receiver);
}

CK_DLL_MFUN(pdpatch_sendFloat)
{
    PdPatchInternal * pd = (PdPatchInternal *)OBJ_MEMBER_INT(SELF, pdpatch_data_offset);
    std::string receiver = GET_NEXT_STRING_SAFE(ARGS);
    t_CKFLOAT value = GET_NEXT_FLOAT(ARGS);
    pd->sendFloat(receiver, (float)value);
}

CK_DLL_MFUN(pdpatch_sendSymbol)
{
    PdPatchInternal * pd = (PdPatchInternal *)OBJ_MEMBER_INT(SELF, pdpatch_data_offset);
    std::string receiver = GET_NEXT_STRING_SAFE(ARGS);
    std::string symbol = GET_NEXT_STRING_SAFE(ARGS);
    pd->sendSymbol(receiver, symbol);
}

CK_DLL_MFUN(pdpatch_sendList)
{
    PdPatchInternal * pd = (PdPatchInternal *)OBJ_MEMBER_INT(SELF, pdpatch_data_offset);
    std::string receiver = GET_NEXT_STRING_SAFE(ARGS);
    Chuck_ArrayFloat * arr = (Chuck_ArrayFloat *)GET_NEXT_OBJECT(ARGS);

    std::vector<float> values;
    if (arr)
    {
        int size = API->object->array_float_size(arr);
        values.resize(size);
        for (int i = 0; i < size; i++)
            values[i] = (float)API->object->array_float_get_idx(arr, i);
    }
    pd->sendList(receiver, values);
}

CK_DLL_MFUN(pdpatch_sendMessage)
{
    PdPatchInternal * pd = (PdPatchInternal *)OBJ_MEMBER_INT(SELF, pdpatch_data_offset);
    std::string receiver = GET_NEXT_STRING_SAFE(ARGS);
    std::string msg = GET_NEXT_STRING_SAFE(ARGS);
    Chuck_ArrayFloat * arr = (Chuck_ArrayFloat *)GET_NEXT_OBJECT(ARGS);

    std::vector<float> args_vec;
    if (arr)
    {
        int size = API->object->array_float_size(arr);
        args_vec.resize(size);
        for (int i = 0; i < size; i++)
            args_vec[i] = (float)API->object->array_float_get_idx(arr, i);
    }
    pd->sendMessage(receiver, msg, args_vec);
}

//-----------------------------------------------------------------------------
// Receive messages FROM Pd
//-----------------------------------------------------------------------------
CK_DLL_MFUN(pdpatch_bind)
{
    PdPatchInternal * pd = (PdPatchInternal *)OBJ_MEMBER_INT(SELF, pdpatch_data_offset);
    std::string receiver = GET_NEXT_STRING_SAFE(ARGS);
    pd->bind(receiver);
}

CK_DLL_MFUN(pdpatch_unbind)
{
    PdPatchInternal * pd = (PdPatchInternal *)OBJ_MEMBER_INT(SELF, pdpatch_data_offset);
    std::string receiver = GET_NEXT_STRING_SAFE(ARGS);
    pd->unbind(receiver);
}

CK_DLL_MFUN(pdpatch_getFloat)
{
    PdPatchInternal * pd = (PdPatchInternal *)OBJ_MEMBER_INT(SELF, pdpatch_data_offset);
    std::string receiver = GET_NEXT_STRING_SAFE(ARGS);
    RETURN->v_float = pd->getFloat(receiver);
}

CK_DLL_MFUN(pdpatch_getSymbol)
{
    PdPatchInternal * pd = (PdPatchInternal *)OBJ_MEMBER_INT(SELF, pdpatch_data_offset);
    std::string receiver = GET_NEXT_STRING_SAFE(ARGS);
    std::string sym = pd->getSymbol(receiver);
    RETURN->v_string = (Chuck_String *)API->object->create_string(VM, sym.c_str(), false);
}

CK_DLL_MFUN(pdpatch_getBang)
{
    PdPatchInternal * pd = (PdPatchInternal *)OBJ_MEMBER_INT(SELF, pdpatch_data_offset);
    std::string receiver = GET_NEXT_STRING_SAFE(ARGS);
    RETURN->v_int = pd->getBang(receiver);
}

//-----------------------------------------------------------------------------
// Pd array access
//-----------------------------------------------------------------------------
CK_DLL_MFUN(pdpatch_arraySize)
{
    PdPatchInternal * pd = (PdPatchInternal *)OBJ_MEMBER_INT(SELF, pdpatch_data_offset);
    std::string name = GET_NEXT_STRING_SAFE(ARGS);
    RETURN->v_int = pd->arraySize(name);
}

CK_DLL_MFUN(pdpatch_readArray)
{
    PdPatchInternal * pd = (PdPatchInternal *)OBJ_MEMBER_INT(SELF, pdpatch_data_offset);
    std::string name = GET_NEXT_STRING_SAFE(ARGS);
    Chuck_ArrayFloat * dest = (Chuck_ArrayFloat *)GET_NEXT_OBJECT(ARGS);
    t_CKINT offset = GET_NEXT_INT(ARGS);

    if (!dest)
    {
        RETURN->v_int = -1;
        return;
    }

    int size = API->object->array_float_size(dest);
    std::vector<float> buf(size);
    int result = pd->readArray(name, buf.data(), size, (int)offset);
    if (result == 0)
    {
        for (int i = 0; i < size; i++)
            API->object->array_float_set_idx(dest, i, (t_CKFLOAT)buf[i]);
    }
    RETURN->v_int = result;
}

CK_DLL_MFUN(pdpatch_writeArray)
{
    PdPatchInternal * pd = (PdPatchInternal *)OBJ_MEMBER_INT(SELF, pdpatch_data_offset);
    std::string name = GET_NEXT_STRING_SAFE(ARGS);
    Chuck_ArrayFloat * src = (Chuck_ArrayFloat *)GET_NEXT_OBJECT(ARGS);
    t_CKINT offset = GET_NEXT_INT(ARGS);

    if (!src)
    {
        RETURN->v_int = -1;
        return;
    }

    int size = API->object->array_float_size(src);
    std::vector<float> buf(size);
    for (int i = 0; i < size; i++)
        buf[i] = (float)API->object->array_float_get_idx(src, i);
    RETURN->v_int = pd->writeArray(name, buf.data(), size, (int)offset);
}

//-----------------------------------------------------------------------------
// MIDI send
//-----------------------------------------------------------------------------
CK_DLL_MFUN(pdpatch_noteOn)
{
    PdPatchInternal * pd = (PdPatchInternal *)OBJ_MEMBER_INT(SELF, pdpatch_data_offset);
    t_CKINT channel = GET_NEXT_INT(ARGS);
    t_CKINT pitch = GET_NEXT_INT(ARGS);
    t_CKINT velocity = GET_NEXT_INT(ARGS);
    pd->noteOn((int)channel, (int)pitch, (int)velocity);
}

CK_DLL_MFUN(pdpatch_noteOff)
{
    PdPatchInternal * pd = (PdPatchInternal *)OBJ_MEMBER_INT(SELF, pdpatch_data_offset);
    t_CKINT channel = GET_NEXT_INT(ARGS);
    t_CKINT pitch = GET_NEXT_INT(ARGS);
    t_CKINT velocity = GET_NEXT_INT(ARGS);
    pd->noteOff((int)channel, (int)pitch, (int)velocity);
}

CK_DLL_MFUN(pdpatch_controlChange)
{
    PdPatchInternal * pd = (PdPatchInternal *)OBJ_MEMBER_INT(SELF, pdpatch_data_offset);
    t_CKINT channel = GET_NEXT_INT(ARGS);
    t_CKINT controller = GET_NEXT_INT(ARGS);
    t_CKINT value = GET_NEXT_INT(ARGS);
    pd->controlChange((int)channel, (int)controller, (int)value);
}

CK_DLL_MFUN(pdpatch_programChange)
{
    PdPatchInternal * pd = (PdPatchInternal *)OBJ_MEMBER_INT(SELF, pdpatch_data_offset);
    t_CKINT channel = GET_NEXT_INT(ARGS);
    t_CKINT program = GET_NEXT_INT(ARGS);
    pd->programChange((int)channel, (int)program);
}

CK_DLL_MFUN(pdpatch_pitchBend)
{
    PdPatchInternal * pd = (PdPatchInternal *)OBJ_MEMBER_INT(SELF, pdpatch_data_offset);
    t_CKINT channel = GET_NEXT_INT(ARGS);
    t_CKINT value = GET_NEXT_INT(ARGS);
    pd->pitchBend((int)channel, (int)value);
}

CK_DLL_MFUN(pdpatch_aftertouch)
{
    PdPatchInternal * pd = (PdPatchInternal *)OBJ_MEMBER_INT(SELF, pdpatch_data_offset);
    t_CKINT channel = GET_NEXT_INT(ARGS);
    t_CKINT value = GET_NEXT_INT(ARGS);
    pd->aftertouch((int)channel, (int)value);
}

CK_DLL_MFUN(pdpatch_polyAftertouch)
{
    PdPatchInternal * pd = (PdPatchInternal *)OBJ_MEMBER_INT(SELF, pdpatch_data_offset);
    t_CKINT channel = GET_NEXT_INT(ARGS);
    t_CKINT pitch = GET_NEXT_INT(ARGS);
    t_CKINT value = GET_NEXT_INT(ARGS);
    pd->polyAftertouch((int)channel, (int)pitch, (int)value);
}
