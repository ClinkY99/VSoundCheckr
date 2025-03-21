//
// Created by cline on 2025-01-28.
//

#ifndef AUDIOIO_H
#define AUDIOIO_H

#include <memory>
#include <portaudio.h>
#include <thread>
#include <vector>

#include "PlaybackSchedules.h"
#include "Resample.h"
#include "../audioBuffers.h"
#include "../../Playback/Sequences/AudioIOSequences.h"
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

void ClampBuffer(float* pBuffer, unsigned long len);

using Duration = std::chrono::duration<double>;

class AudioIOBase {

public:
    static std::shared_ptr<DBConnection> sAudioDB;
    static std::unique_ptr<AudioIOBase> ugAudioIO;

protected:
    double mRate;

public:
    static void initAudio();
    static void createDB();

    bool isMonitoring() {return false;}

    unsigned int getPlaybackDevIndex();
    unsigned int getCaptureDevIndex();
    unsigned int getNumPlaybackChannels();
    unsigned int getNumCaptureChannels();

//Static Member Functions
public:
    static AudioIOBase* Get() {return ugAudioIO.get();}

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

    int mCallbackReturn;

    unsigned long mMaxFramesOutput;

    //channels
    size_t mNumPlaybackChannels;
    constPlayableSequences mPlayableSequences;
    PlaybackSchedule mPlaybackShchedule;

    int mbHasSoloSequences;

    size_t mNumCaptureChannels;
    recordingSequences mRecordingSequences;
    RecordingSchedule mRecordingSchedule;

    //AudioThread Settings
    std::atomic<bool> mAudioThreadSequenceBufferExchangeActive {false};
    std::atomic<bool> mAudioThreadShouldSequenceBufferExchangeOnce {false};
    std::atomic<bool> mAudioThreadSequenceBufferExchangeLoopRunning {false};

    //buffers
    using audioBuffers = std::vector<std::unique_ptr<audioBuffer>>;

    audioBuffers mPlaybackBuffers;
    audioBuffers mCaptureBuffers;
    std::vector<std::vector<float>> mProcessingBuffers;

    size_t mLostSamples;
    //audioBuffer mMaster;

    //buffer Settings
    double mPlaybackBufferSecs;
    size_t mPlaybackSamplesToCopy;

    double mCaptureBufferSecs;
    double mMinCaptureBufferSecsToCopy;

    size_t mHardwarePlaybackLatency;

    //Resampling Stuff??
    double mFactor;
    std::vector<std::unique_ptr<Resample>> mResample;

    //State Stuff
    std::atomic<bool> mPaused{false};
    std::atomic<bool> mRecording{false};
    std::atomic<bool> mPlayback{false};



public:
    // AudioIoCallback();
    // ~AudioIoCallback();

    bool SequenceShouldBeSilent(const PlaybackSequence &ps);
    int AudioCallback(constSamplePtr inputBuffer, float* outputBuffer,  unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData);
    void CallbackCompletion(int& callbackReturn, unsigned long len);

    void UpdateTimePosition(unsigned long framesPerBuffer);

    void FillOutputBuffers(float* outputFloats, unsigned long framesPerBuffer);
    void DrainInputBuffers(constSamplePtr inputBuffer, unsigned long framesPerBuffer, float* tempFloats);


    unsigned CountSoloSequences();


    bool isPaused() const {return mPaused.load(std::memory_order_relaxed);}

protected:
    void doPlayback();
    void doInput();

    static size_t MinValues(const audioBuffers &buffers, size_t(audioBuffer::*pmf)() const);

    size_t CommonlyReadyPlayback();
};


class AudioIO
    : public AudioIoCallback{

    //use threads to handle input?
    //also for reading and writing the audio... output buffer should be filled by this
    //output should be handled by callback but input is taxing so maybe threading will be better so i dont delay audio proscessing

    AudioIOStream* mAudioStream;

    size_t mPlaybackQueueMinimum;


public:
  //Init Functions
    AudioIO();
    ~AudioIO();

    int startStream();

    bool isStreamRunning() {return mAudioStream->getStreamStillRunning();};

    bool createPortAudioStream() {return false;};

    //audio processing functions (while part of stream)
    static void audioThread(std::atomic<bool>& finish);

    void sequenceBufferExchange();

private:

    bool initPortAudio();

    void startAudioThread();

    void startStreamCleanup(bool bClearBuffersOnly = false);

    bool AllocateBuffers(double sampleRate);

    size_t GetCommonlyAvailCapture();
    size_t GetCommonlyFreePlayback();
    size_t GetCommonlyWrittenForPlayback();

    //Buffer Exchange
    void DrainRecordBuffers();
    void FillPlayBuffers();
    bool ProcessPlaybackSlices(size_t avail);

    sampleCount getWritePos();

//Static Member Functions
public:
    static void Init();
    static void DeInit();

    static AudioIO* Get() {return static_cast<AudioIO*>(AudioIOBase::Get());}

};


#endif //AUDIOIO_H
