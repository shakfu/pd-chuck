#ifndef PD_PATCH_INTERNAL_H
#define PD_PATCH_INTERNAL_H

#include "chugin.h"

#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>

// libpd headers
#include "z_libpd.h"

// Pd's fixed block size
#define PD_BLOCK_SIZE 64
// Number of channels (stereo)
#define PD_NUM_CHANNELS 2

struct ReceivedValue
{
    float floatVal = 0.0f;
    std::string symbolVal;
    bool bangReceived = false;
};

class PdPatchInternal
{
public:
    PdPatchInternal(t_CKFLOAT srate);
    ~PdPatchInternal();

    // Audio processing
    void tick(SAMPLE * in, SAMPLE * out, t_CKUINT nframes);

    // Patch lifecycle
    int open(const std::string & patchFile, const std::string & dir);
    void close();
    void addSearchPath(const std::string & path);

    // Send messages to Pd
    void sendBang(const std::string & receiver);
    void sendFloat(const std::string & receiver, float value);
    void sendSymbol(const std::string & receiver, const std::string & symbol);
    void sendList(const std::string & receiver, const std::vector<float> & values);
    void sendMessage(const std::string & receiver, const std::string & msg,
                     const std::vector<float> & args);

    // Receive messages from Pd
    void bind(const std::string & receiver);
    void unbind(const std::string & receiver);
    float getFloat(const std::string & receiver);
    std::string getSymbol(const std::string & receiver);
    int getBang(const std::string & receiver);

    // Pd array access
    int arraySize(const std::string & name);
    int readArray(const std::string & name, float * dest, int destSize, int offset);
    int writeArray(const std::string & name, const float * src, int srcSize, int offset);

    // MIDI send
    void noteOn(int channel, int pitch, int velocity);
    void noteOff(int channel, int pitch, int velocity);
    void controlChange(int channel, int controller, int value);
    void programChange(int channel, int program);
    void pitchBend(int channel, int value);
    void aftertouch(int channel, int value);
    void polyAftertouch(int channel, int pitch, int value);

    // Static callbacks for libpd
    static void floatHook(const char * src, float x);
    static void bangHook(const char * src);
    static void symbolHook(const char * src, const char * sym);
    static void listHook(const char * src, int argc, t_atom * argv);
    static void messageHook(const char * src, const char * msg,
                            int argc, t_atom * argv);
    static void printHook(const char * msg);

private:
    void processPdBlock();
    void setInstance();

    t_pdinstance * m_pdInstance;
    void * m_patchHandle;

    // Audio buffers (non-interleaved, PD_NUM_CHANNELS * PD_BLOCK_SIZE)
    float m_inBuffer[PD_NUM_CHANNELS * PD_BLOCK_SIZE];
    float m_outBuffer[PD_NUM_CHANNELS * PD_BLOCK_SIZE];

    int m_inWritePos;
    int m_outReadPos;

    t_CKFLOAT m_srate;

    // Received values from Pd
    std::unordered_map<std::string, ReceivedValue> m_received;

    // Bound receivers (name -> void* binding)
    std::unordered_map<std::string, void *> m_bindings;

    // Per-instance mutex for thread safety
    std::mutex m_mutex;

    // Map from t_pdinstance* to PdPatchInternal* for callback routing
    static std::unordered_map<t_pdinstance *, PdPatchInternal *> s_instanceMap;
    static std::mutex s_instanceMapMutex;
};

#endif // PD_PATCH_INTERNAL_H
