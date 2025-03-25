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
#include "../../Playback/Track.h"
#include "../../Playback/Sequences/AudioIOSequences.h"
#include "../../Saving/DBConnection.h"




struct testUserData {
    float left_phase;
    float right_phase;
};

struct audioProcessingUserData {

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
    AudioIOStream(PaError &err,PaStreamParameters *inputParams, PaStreamParameters *outputParams, int srate, PaStreamCallback CallbackFXN);
    ~AudioIOStream();
    PaError initializeAudioStream(PaStreamParameters*, PaStreamParameters*, int,  PaStreamCallback CallbackFXN);
    PaError startStream();
    void endStream();
    void abortStream();

    double getOutputLatency() {return Pa_GetStreamInfo(mStream)->outputLatency;}

private:
    PaStream *mStream = nullptr;

public:
    bool getStreamStillRunning(){return Pa_IsStreamActive(mStream);};
    bool isValid(){return mStream != nullptr;}
    void PrintSFormat();
};

enum Acknowledge {
    eNone,
    eStart,
    eStop
};

class AudioIoCallback
    : public AudioIOBase{

protected:
    //Audio thread Vars
    std::thread mAudioThread;

    std::atomic<bool> mKillAudioThread{false};

    //Audio Settings
    SampleFormat mCaptureFormat = floatSample;

    int mCallbackReturn;

    unsigned long mMaxFramesOutput;

    //channels
    size_t mNumPlaybackChannels;
    size_t mMaxPLaybackChannels;
    constPlayableSequences mPlayableSequences;
    PlaybackSchedule mPlaybackShchedule;
     
    int mbHasSoloSequences;

    size_t mNumCaptureChannels;
    size_t mMaxNumCaptureChannels;
    recordingSequences mRecordingSequences;
    RecordingSchedule mRecordingSchedule;

    std::vector<recordingSequences> mCaptureMap;
    constPlayableSequences mPlaybackMap;

    double mSeek = 0;

    //AudioThread Settings
    std::atomic<bool> mAudioThreadSequenceBufferExchangeActive {false};
    std::atomic<bool> mAudioThreadShouldSequenceBufferExchangeOnce {false};
    std::atomic<bool> mAudioThreadSequenceBufferExchangeLoopRunning {false};
    std::atomic<Acknowledge> mAudioThreadAcknowledge { eNone };

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
    int AudioIOCallback(constSamplePtr inputBuffer, float* outputBuffer,  unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData);
    void CallbackCompletion(int& callbackReturn, unsigned long len);

    void UpdateTimePosition(unsigned long framesPerBuffer);

    void FillOutputBuffers(float* outputFloats, unsigned long framesPerBuffer);
    void DrainInputBuffers(constSamplePtr inputBuffer, unsigned long framesPerBuffer, float* tempFloats);

    unsigned CountSoloSequences();

    bool isPaused() const {return mPaused.load(std::memory_order_relaxed);}

    void doSeek(double amount) {mSeek = amount;}

    double getCurrentPlaybackTime(){return mPlaybackShchedule.mTimeQueue.GetLastTime();}
    double getRecordingTime(){return mRecordingSchedule.mPosition;}
    //Playback stuff



protected:
    int CallbackDoSeek();

    void startAudioThread();
    void stopAudioThread();

    void waitForAudioThreadStarted();
    void waitForAudioThreadStopped();
    void processOnceAndWait();

    void doPlayback();
    void doInput();

    bool BuildMaps();

    static size_t MinValues(const audioBuffers &buffers, size_t(audioBuffer::*pmf)() const);

    size_t CommonlyReadyPlayback();
};

struct TransportSequence {
    recordingSequences captureSequences = {};
    constPlayableSequences playableSequences = {};
};

struct audioIoStreamOptions {
    unsigned int mCaptureChannels = 0;
    unsigned int mPlaybackChannels = 0;

    PaDeviceIndex InDev = 0;
    PaDeviceIndex OutDev = 0;

    unsigned int mSampleRate = 0;

    std::optional<double> mStartTime;
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

    int startStream(const TransportSequence &transportSequence, double t0, double t1, const audioIoStreamOptions & options);
    void stopStream();


    bool isStreamRunning() {return mAudioStream->getStreamStillRunning();};

    //audio processing functions (while part of stream)
    static void audioThread(std::atomic<bool>& finish);

    void sequenceBufferExchange();

    void togglePause();


private:

    void startThread();



    bool createPortAudioStream(const audioIoStreamOptions &options);

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
