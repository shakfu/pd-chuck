# pd-puck

An ongoing attempt to marry puredata and chuck.


## TODO

- [ ] integrate dsp callback





## Prior Art

### chuck-embed

see `chuck-embed` example in : <https://chuck.stanford.edu/release/files/examples/>


### chuck~ for Max/MSP

Brad Garton's [chuck~](http://sites.music.columbia.edu/brad/chuck~/) brings the sound synthesis and signal-processing capabilities of the ChucK music programming language into Max/MSP real-time music environment.

Last update 2011 (works 32bit only for Max 5)


### ChucKDesigner

David Braun's [ChucKDesigner](https://github.com/DBraun/ChucKDesigner) is an integration of the ChucK music/audio programming language with the TouchDesigner visual programming language.



```cpp
CHUCKDESIGNERSHARED_API bool processBlock(unsigned int chuckID, const float** inBuffer, int inBufferNumChannels, int inBufferNumSamples, float* inChucKBuffer, float* outChucKBuffer, float** outBuffer, int numOutSamples, int numOutChannels) {

        if (chuck_instances.count(chuckID) == 0) {
            return false;
        }
        ChucK* chuck = chuck_instances[chuckID];

        int numOutChans = chuck->vm()->m_num_dac_channels;
        if (numOutChans != numOutChannels) {
            return false;
        }

        int numSamples;
        int numInChannels = std::min<int>(inBufferNumChannels, (int)chuck->vm()->m_num_adc_channels);

        for (int i = 0; i < numOutSamples; i += CHUCKDESIGNERCHOP_BUFFER_SIZE) {

            // chuck->run(inbuffer, *output->channels, output->numSamples); // this doesn't work because of interleaved samples.
            // Chuck returns LRLRLRLR but for touchdesigner we want LLLLRRRR.
            // Therefore we must use an intermediate buffer
            float* inPtr = inChucKBuffer;

            numSamples = min(CHUCKDESIGNERCHOP_BUFFER_SIZE, numOutSamples - i);

            if (inBuffer) {
                for (int samp = i; samp < std::min<int>(inBufferNumSamples, i + CHUCKDESIGNERCHOP_BUFFER_SIZE); samp++) {
                    for (int chan = 0; chan < numInChannels; chan++) {
                        *(inPtr++) = inBuffer[chan][samp];
                    }
                }
            }
            float* outPtr = outChucKBuffer;

            chuck->run(inChucKBuffer, outChucKBuffer, numSamples);

            for (int samp = 0; samp < numSamples; samp++) {
                for (int chan = 0; chan < numOutChans; chan++) {
                    outBuffer[chan][i + samp] = *outPtr++;
                }
            }

        }

        return true;
    }
```

## Pd and CoreAudio


Interesting [blog post](https://christianfloisand.wordpress.com/2014/12/07/pure-data-and-libpd-integrating-with-native-code-for-interactive-testing/) by Christian Floisand on puredata, libpd and CoreAudio.

```cpp
OSStatus AudioRenderProc (void *inRefCon,
                          AudioUnitRenderActionFlags *ioActionFlags,
                          const AudioTimeStamp *inTimeStamp,
                          UInt32 inBusNumber,
                          UInt32 inNumberFrames,
                          AudioBufferList *ioData)
    {
        TestData *testData = (TestData *)inRefCon;
 
        // Don't require input, but libpd requires valid array.
        float inBuffer[0];
 
        // Only read from Pd patch if the sample excess is less than the number of frames being processed.
        // This effectively empties the ring buffer when it has enough samples for the current block, preventing the
        // write pointer from catching up to the read pointer.
        if (testData->framesInReserve < inNumberFrames)
        {
            testData->pd->processFloat(testData->pdTicks, inBuffer, testData->pdBuffer);
            for (int i = 0; i < testData->pdSamplesPerBlock; ++i)
            {
                testData->ringBuffer.write(testData->pdBuffer[i]);
            }
            testData->framesInReserve += (testData->maxFramesPerSlice - inNumberFrames);
        }
        else
        {
            testData->framesInReserve -= inNumberFrames;
        }
 
        // NOTE: Audio data from Pd patch is interleaved, whereas Core Audio buffers are non-interleaved.
        for (UInt32 frame = 0; frame < inNumberFrames; ++frame)
        {
            Float32 *data = (Float32 *)ioData->mBuffers[0].mData;
            data[frame] = testData->ringBuffer.read();
            data = (Float32 *)ioData->mBuffers[1].mData;
            data[frame] = testData->ringBuffer.read();
        }
 
        if (testData->effectCallback != nullptr)
        {
            testData->effectCallback(ioData, inNumberFrames);
        }
 
        return noErr;
```


## ZenGarden

[ZenGarden](https://github.com/mhroth/ZenGarden) (ZG) is a runtime for the Pure Data (Pd) audio programming language. It is implemented as an extensible audio library allowing full control over signal processing, message passing, and graph manipulation. ZenGarden does not have a GUI, but easily allows one to be built on top of it.


```c
// include the ZenGarden API definition
#include "ZenGarden.h"


// A ZenGarden context (see below) communicates via a single callback funtion as show here.
// A number of ZGCallbackFunction types are available with the most common being print commands.
void *callbackFunction(ZGCallbackFunction function, void *userData, void *ptr) {
  switch (function) {
    case ZG_PRINT_STD: {
      // A standard print command.
      printf("%s\n", (const char *) ptr);
      break;
    }
    case ZG_PRINT_ERR: {
      // A error print command.
      printf("ERROR: %s\n", (const char *) ptr);
      break;
    }
    default: break;
  }
  return NULL;
}

int main(int argc, char * const argv[]) {

  // The number of samples to be processed per block. This can be any positive integer,
  // though a power of two is strongly suggested. Typical values range between 64 and 1024.
  const int blockSize = 64;

  // The number of input channels. This can be any non-negative integer. Typical values are 0 (no input),
  // 1 (mono input), or 2 (stereo input).
  const int numInputChannels = 2;

  // The number of output channels. This can be any non-negative integer. Typical values are 0 (no output),
  // 1 (mono output), or 2 (stereo output).
  const int numOutputChannels = 2;

  // The sample rate. Any positive sample rate is supported. Typical values are i.e. 8000.0f, 22050.0f, or 44100.0f.
  const float sampleRate = 22050.0f;

  // Create a new context.
  ZGContext *context = zg_context_new(numInputChannels, numOutputChannels, blockSize, sampleRate, callbackFunction, NULL);

  // Create a new graph from the given file. The returned graph will be fully functional but it will not be attached
  // to the context. Only attached graphs are processed by the context. Unattached graphs created in a context do not
  // exist from the perspective of the context and add no overhead, but can be quickly added (and removed) as needed.
  // Graphs are not created in the audio thread and will not cause audio underruns.
  PdGraph *graph = zg_context_new_graph_from_file(context, "/path/to/", "file.pd");
  if (graph == NULL) {
    // if the returned graph is NULL then something has gone wrong.
    // If a callback function was provided to the context, then an error message will likely be reported there.
    return 1;
  }

  // Attach the graph to its context. Attaching a graph pauses audio playback, but it is typically a fast operation
  // and is unlikely to cause an underrun.
  zg_graph_attach(graph);

  // dummy input and output audio buffers. Note their size.
  float finputBuffers[blockSize * numInputChannels];
  float foutputBuffers[blockSize * numOutputChannels];

  // the audio loop
  while (1) {
    // Process the context. Messages are executed and audio buffers are consumed and produced.

    // if input and output audio buffers are presented as non-interleaved floating point (32-bit) samples
    // (ZenGarden's native format), they can be processed as shown here.
    zg_context_process(context, finputBuffers, foutputBuffers);

    // if input and output audio buffers are presented as interleaved signed integer (16-bit) samples,
    // they can be processed as shown here. ZenGarden automatically takes care of the translation to and
    // from the native non-interleaved format.
    // zg_context_process_s(context, sinputBuffers, soutputBuffer);
  }

  // Free memory. Memory can be freed in many ways.

  // The easiest is to delete the context. This not deletes the context and everything attached to it.
  zg_context_delete(context);

  // On the other hand, individual graphs can be deleted when they are no longer needed.
  // If an attached graph is deleted, it is automatically removed from the context first.
  // zg_graph_delete(graph);
  // zg_context_delete(context); // ultimately also the context must be deleted.

  return 0;
}

```

## pdpatchrepo posts -- How to process non-interleaved audio with libpd_process_float?

see: https://forum.pdpatchrepo.info/topic/13262/how-to-process-non-interleaved-audio-with-libpd_process_float

I ended up ditching the ring buffers and doing it like this — I haven't seen any issues tapping from an mp4 input so far without the use of ring buffers:

```cpp
if (context->frameSize && outputBufferSize > 0) {
    if (bufferListInOut->mNumberBuffers > 1) {
        float *left = (float *)bufferListInOut->mBuffers[0].mData;
        float *right = (float *)bufferListInOut->mBuffers[1].mData;
            
        //manually interleave channels
        for (int i = 0; i < outputBufferSize; i += 2) {
            context->interleaved[i] = left[i / 2];
            context->interleaved[i + 1] = right[i / 2];
        }
        [PdBase processFloatWithInputBuffer:context->interleaved outputBuffer:context->interleaved ticks:64];
        //de-interleave
        for (int i = 0; i < outputBufferSize; i += 2) {
            left[i / 2] = context->interleaved[i];
            right[i / 2] = context->interleaved[i + 1];
        }
    } else {
        context->interleaved = (float *)bufferListInOut->mBuffers[0].mData;
        [PdBase processFloatWithInputBuffer:context->interleaved outputBuffer:context->interleaved ticks:32];
    }
}
```

## pdpatchrepo

see: https://forum.pdpatchrepo.info/topic/12905/bang-bangs-before-the-end-of-a-dsp-block-at-startup/3

also: [PD's scheduler, timing, control-rate, audio-rate, block-size, (sub)sample accuracy](https://forum.pdpatchrepo.info/topic/12390/pd-s-scheduler-timing-control-rate-audio-rate-block-size-sub-sample-accuracy)



