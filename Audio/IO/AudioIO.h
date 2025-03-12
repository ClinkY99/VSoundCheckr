//
// Created by cline on 2025-01-28.
//

#ifndef AUDIOIO_H
#define AUDIOIO_H

#include <memory>
#include <portaudio.h>
#include <thread>
#include <vector>

#include "../audioBuffers.h"
#include "../../Saving/DBConnection.h"

#ifndef __WXMSW__
#include <pa_win_wasapi.h>
#endif


struct testUserData {
    float left_phase;
    float right_phase;
};

struct audioProcessingUserData {

};

struct audioIoStreamOptions {
    unsigned int mCaptureChannels;
    unsigned int mPlaybackChannels;

    unsigned int mSampleRate;


};

static int AudioCallback(const void *inputBuffer, void *outputBuffer,  unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData);

using Duration = std::chrono::duration<double>;

struct BufferTime {
    Duration batchSize;
    Duration latency;
    Duration RingBufferDelay;
};
static BufferTime getBufferTimes() {
    using namespace std::chrono;
    return { 0.05s, 0.05s, 0.25s };
}

class AudioIOBase {

public:
    static std::shared_ptr<DBConnection> sAudioDB;

protected:
    double mRate;



public:
    static void initAudio();
    static void createDB();

    unsigned int getPlaybackDevIndex();
    unsigned int getCaptureDevIndex();
    unsigned int getNumPlaybackChannels();
    unsigned int getNumCaptureChannels();
};



class AudioIOStream {

public:
    AudioIOStream(PaError &err,PaStreamParameters inputParams, PaStreamParameters outputParams, int srate, PaStreamCallback CallbackFXN);
    PaError initializeAudioStream(PaStreamParameters, PaStreamParameters, int,  PaStreamCallback CallbackFXN);
    void startStream();
    void endStream();

private:
    PaStream *mStream = nullptr;

public:
    bool getStreamStillRunning(){return false;};
};

class AudioIoCallback
    : public AudioIOBase{

protected:
    //Audio thread Vars
    std::thread mAudioThread;

    std::atomic<bool> mKillAudioThread{false};

    //Audio Settings
    SampleFormat mCaptureFormat;

    //channels
    size_t mNumPlaybackChannels;
    size_t mNumCaptureChannels;

    //buffers
    using audioBuffers = std::vector<std::unique_ptr<audioBuffer>>;

    audioBuffers mPlaybackBuffers;
    audioBuffers mCaptureBuffers;
    //audioBuffer mMaster;

    //buffer Settings
    double mPlaybackBufferSecs;
    double mPlaybackSamplesToCopy;

    double mCaptureBufferSecs;
    double mMinCaptureBufferSecsToCopy;

    size_t mHardwarePlaybackLatency;




public:
    // AudioIoCallback();
    // ~AudioIoCallback();
protected:
    void doPlayback();
    void doInput();
};


class AudioIO
    : public AudioIoCallback{

    //use threads to handle input?
    //also for reading and writing the audio... output buffer should be filled by this
    //output should be handled by callback but input is taxing so maybe threading will be better so i dont delay audio proscessing

    AudioIOStream* mAudioStream;


public:
  //Init Functions
    AudioIO();
    ~AudioIO();

    int startStream();

    bool createPortAudioStream() {return false;};


    //audio processing functions (while part of stream)
    static void audioThread(std::atomic<bool>& finish) {};

private:

    bool initPortAudio();

    void startAudioThread();

    void startStreamCleanup(bool bClearBuffersOnly = false);

    bool AllocateBuffers();

};


#endif //AUDIOIO_H
