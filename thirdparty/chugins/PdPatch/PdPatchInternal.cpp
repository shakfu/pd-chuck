#include "PdPatchInternal.h"

#include <cstring>
#include <cstdio>
#include <mutex>

// Static members
std::unordered_map<t_pdinstance *, PdPatchInternal *> PdPatchInternal::s_instanceMap;
std::mutex PdPatchInternal::s_instanceMapMutex;

// Global init flag
static std::once_flag g_pdInitFlag;

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
PdPatchInternal::PdPatchInternal(t_CKFLOAT srate)
    : m_pdInstance(nullptr)
    , m_patchHandle(nullptr)
    , m_inWritePos(0)
    , m_outReadPos(0)
    , m_srate(srate)
{
    // Initialize libpd once globally
    std::call_once(g_pdInitFlag, []() {
        libpd_init();
    });

    // Create a new Pd instance for multi-instance support
    m_pdInstance = libpd_new_instance();
    setInstance();

    // Register this instance for callback routing
    {
        std::lock_guard<std::mutex> lock(s_instanceMapMutex);
        s_instanceMap[m_pdInstance] = this;
    }

    // Set up callback hooks
    libpd_set_floathook(floatHook);
    libpd_set_banghook(bangHook);
    libpd_set_symbolhook(symbolHook);
    libpd_set_listhook(listHook);
    libpd_set_messagehook(messageHook);
    libpd_set_printhook(printHook);

    // Initialize audio: 2 input channels, 2 output channels
    libpd_init_audio(PD_NUM_CHANNELS, PD_NUM_CHANNELS, (int)srate);

    // Zero buffers
    memset(m_inBuffer, 0, sizeof(m_inBuffer));
    memset(m_outBuffer, 0, sizeof(m_outBuffer));

    // Turn on DSP
    libpd_start_message(1);
    libpd_add_float(1);
    libpd_finish_message("pd", "dsp");

    // Pre-process one block of silence so output is immediately available
    processPdBlock();
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
PdPatchInternal::~PdPatchInternal()
{
    close();

    // Unbind all receivers
    for (auto & pair : m_bindings)
    {
        setInstance();
        libpd_unbind(pair.second);
    }
    m_bindings.clear();

    // Remove from instance map
    {
        std::lock_guard<std::mutex> lock(s_instanceMapMutex);
        s_instanceMap.erase(m_pdInstance);
    }

    // Free the Pd instance
    if (m_pdInstance)
    {
        libpd_free_instance(m_pdInstance);
        m_pdInstance = nullptr;
    }
}

//-----------------------------------------------------------------------------
// Set the current Pd instance (must be called before any libpd operation)
//-----------------------------------------------------------------------------
void PdPatchInternal::setInstance()
{
    libpd_set_instance(m_pdInstance);
}

//-----------------------------------------------------------------------------
// Process one Pd block (64 samples)
//-----------------------------------------------------------------------------
void PdPatchInternal::processPdBlock()
{
    setInstance();
    // libpd_process_float: ticks=1 means process one 64-sample block
    // Input and output buffers are non-interleaved:
    //   [L0..L63, R0..R63]
    libpd_process_float(1, m_inBuffer, m_outBuffer);
    m_inWritePos = 0;
    m_outReadPos = 0;
}

//-----------------------------------------------------------------------------
// Audio tick - called from CK_DLL_TICKF
//-----------------------------------------------------------------------------
void PdPatchInternal::tick(SAMPLE * in, SAMPLE * out, t_CKUINT nframes)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    for (t_CKUINT f = 0; f < nframes; f++)
    {
        // Accumulate input: interleaved ChucK -> non-interleaved Pd
        m_inBuffer[0 * PD_BLOCK_SIZE + m_inWritePos] = in[f * PD_NUM_CHANNELS + 0]; // L
        m_inBuffer[1 * PD_BLOCK_SIZE + m_inWritePos] = in[f * PD_NUM_CHANNELS + 1]; // R

        // Drain output: non-interleaved Pd -> interleaved ChucK
        out[f * PD_NUM_CHANNELS + 0] = m_outBuffer[0 * PD_BLOCK_SIZE + m_outReadPos]; // L
        out[f * PD_NUM_CHANNELS + 1] = m_outBuffer[1 * PD_BLOCK_SIZE + m_outReadPos]; // R

        m_inWritePos++;
        m_outReadPos++;

        // When we've accumulated a full Pd block, process it
        if (m_inWritePos >= PD_BLOCK_SIZE)
        {
            processPdBlock();
        }
    }
}

//-----------------------------------------------------------------------------
// Patch lifecycle
//-----------------------------------------------------------------------------
int PdPatchInternal::open(const std::string & patchFile, const std::string & dir)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    setInstance();

    // Close any existing patch
    if (m_patchHandle)
    {
        libpd_closefile(m_patchHandle);
        m_patchHandle = nullptr;
    }

    m_patchHandle = libpd_openfile(patchFile.c_str(), dir.c_str());
    if (!m_patchHandle)
    {
        fprintf(stderr, "[PdPatch] error: failed to open patch '%s' in '%s'\n",
                patchFile.c_str(), dir.c_str());
        return -1;
    }

    return 0;
}

void PdPatchInternal::close()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    setInstance();

    if (m_patchHandle)
    {
        libpd_closefile(m_patchHandle);
        m_patchHandle = nullptr;
    }
}

void PdPatchInternal::addSearchPath(const std::string & path)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    setInstance();
    libpd_add_to_search_path(path.c_str());
}

//-----------------------------------------------------------------------------
// Send messages TO Pd
//-----------------------------------------------------------------------------
void PdPatchInternal::sendBang(const std::string & receiver)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    setInstance();
    libpd_bang(receiver.c_str());
}

void PdPatchInternal::sendFloat(const std::string & receiver, float value)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    setInstance();
    libpd_float(receiver.c_str(), value);
}

void PdPatchInternal::sendSymbol(const std::string & receiver, const std::string & symbol)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    setInstance();
    libpd_symbol(receiver.c_str(), symbol.c_str());
}

void PdPatchInternal::sendList(const std::string & receiver, const std::vector<float> & values)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    setInstance();

    libpd_start_message((int)values.size());
    for (float v : values)
    {
        libpd_add_float(v);
    }
    libpd_finish_list(receiver.c_str());
}

void PdPatchInternal::sendMessage(const std::string & receiver, const std::string & msg,
                                  const std::vector<float> & args)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    setInstance();

    libpd_start_message((int)args.size());
    for (float v : args)
    {
        libpd_add_float(v);
    }
    libpd_finish_message(receiver.c_str(), msg.c_str());
}

//-----------------------------------------------------------------------------
// Receive messages FROM Pd
//-----------------------------------------------------------------------------
void PdPatchInternal::bind(const std::string & receiver)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    setInstance();

    // Don't bind twice
    if (m_bindings.find(receiver) != m_bindings.end())
        return;

    void * binding = libpd_bind(receiver.c_str());
    if (binding)
    {
        m_bindings[receiver] = binding;
        // Initialize received value entry
        m_received[receiver] = ReceivedValue();
    }
}

void PdPatchInternal::unbind(const std::string & receiver)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    setInstance();

    auto it = m_bindings.find(receiver);
    if (it != m_bindings.end())
    {
        libpd_unbind(it->second);
        m_bindings.erase(it);
        m_received.erase(receiver);
    }
}

float PdPatchInternal::getFloat(const std::string & receiver)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_received.find(receiver);
    if (it != m_received.end())
        return it->second.floatVal;
    return 0.0f;
}

std::string PdPatchInternal::getSymbol(const std::string & receiver)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_received.find(receiver);
    if (it != m_received.end())
        return it->second.symbolVal;
    return "";
}

int PdPatchInternal::getBang(const std::string & receiver)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_received.find(receiver);
    if (it != m_received.end())
    {
        bool received = it->second.bangReceived;
        it->second.bangReceived = false; // consume
        return received ? 1 : 0;
    }
    return 0;
}

//-----------------------------------------------------------------------------
// Pd array access
//-----------------------------------------------------------------------------
int PdPatchInternal::arraySize(const std::string & name)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    setInstance();
    return libpd_arraysize(name.c_str());
}

int PdPatchInternal::readArray(const std::string & name, float * dest, int destSize, int offset)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    setInstance();
    return libpd_read_array(dest, name.c_str(), offset, destSize);
}

int PdPatchInternal::writeArray(const std::string & name, const float * src, int srcSize, int offset)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    setInstance();
    return libpd_write_array(name.c_str(), offset, const_cast<float *>(src), srcSize);
}

//-----------------------------------------------------------------------------
// MIDI send
//-----------------------------------------------------------------------------
void PdPatchInternal::noteOn(int channel, int pitch, int velocity)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    setInstance();
    libpd_noteon(channel, pitch, velocity);
}

void PdPatchInternal::noteOff(int channel, int pitch, int velocity)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    setInstance();
    // MIDI noteOff is noteOn with velocity 0 in many implementations,
    // but libpd has noteon; send velocity 0 for noteOff
    libpd_noteon(channel, pitch, 0);
}

void PdPatchInternal::controlChange(int channel, int controller, int value)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    setInstance();
    libpd_controlchange(channel, controller, value);
}

void PdPatchInternal::programChange(int channel, int program)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    setInstance();
    libpd_programchange(channel, program);
}

void PdPatchInternal::pitchBend(int channel, int value)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    setInstance();
    libpd_pitchbend(channel, value);
}

void PdPatchInternal::aftertouch(int channel, int value)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    setInstance();
    libpd_aftertouch(channel, value);
}

void PdPatchInternal::polyAftertouch(int channel, int pitch, int value)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    setInstance();
    libpd_polyaftertouch(channel, pitch, value);
}

//-----------------------------------------------------------------------------
// Static callback hooks for libpd
//-----------------------------------------------------------------------------
void PdPatchInternal::floatHook(const char * src, float x)
{
    // Get current Pd instance and route to the correct PdPatchInternal
    t_pdinstance * instance = libpd_this_instance();
    std::lock_guard<std::mutex> lock(s_instanceMapMutex);
    auto it = s_instanceMap.find(instance);
    if (it != s_instanceMap.end())
    {
        // Note: we do NOT lock m_mutex here because this callback is invoked
        // from within libpd_process_float, which is already called under lock
        auto & received = it->second->m_received;
        auto rit = received.find(src);
        if (rit != received.end())
        {
            rit->second.floatVal = x;
        }
    }
}

void PdPatchInternal::bangHook(const char * src)
{
    t_pdinstance * instance = libpd_this_instance();
    std::lock_guard<std::mutex> lock(s_instanceMapMutex);
    auto it = s_instanceMap.find(instance);
    if (it != s_instanceMap.end())
    {
        auto & received = it->second->m_received;
        auto rit = received.find(src);
        if (rit != received.end())
        {
            rit->second.bangReceived = true;
        }
    }
}

void PdPatchInternal::symbolHook(const char * src, const char * sym)
{
    t_pdinstance * instance = libpd_this_instance();
    std::lock_guard<std::mutex> lock(s_instanceMapMutex);
    auto it = s_instanceMap.find(instance);
    if (it != s_instanceMap.end())
    {
        auto & received = it->second->m_received;
        auto rit = received.find(src);
        if (rit != received.end())
        {
            rit->second.symbolVal = sym;
        }
    }
}

void PdPatchInternal::listHook(const char * src, int argc, t_atom * argv)
{
    t_pdinstance * instance = libpd_this_instance();
    std::lock_guard<std::mutex> lock(s_instanceMapMutex);
    auto it = s_instanceMap.find(instance);
    if (it != s_instanceMap.end())
    {
        auto & received = it->second->m_received;
        auto rit = received.find(src);
        if (rit != received.end())
        {
            // Store first element as float or symbol for simple access
            if (argc > 0)
            {
                if (libpd_is_float(argv))
                    rit->second.floatVal = libpd_get_float(argv);
                else if (libpd_is_symbol(argv))
                    rit->second.symbolVal = libpd_get_symbol(argv);
            }
        }
    }
}

void PdPatchInternal::messageHook(const char * src, const char * msg,
                                  int argc, t_atom * argv)
{
    t_pdinstance * instance = libpd_this_instance();
    std::lock_guard<std::mutex> lock(s_instanceMapMutex);
    auto it = s_instanceMap.find(instance);
    if (it != s_instanceMap.end())
    {
        auto & received = it->second->m_received;
        auto rit = received.find(src);
        if (rit != received.end())
        {
            // Store the message selector as a symbol
            rit->second.symbolVal = msg;
        }
    }
}

void PdPatchInternal::printHook(const char * msg)
{
    // Route Pd print messages to stderr with a prefix
    fprintf(stderr, "[Pd] %s", msg);
}
