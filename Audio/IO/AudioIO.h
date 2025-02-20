//
// Created by cline on 2025-01-28.
//

#ifndef AUDIOIO_H
#define AUDIOIO_H

#include <portaudio.h>
#include <thread>

#ifndef __WXMSW__
#include <pa_win_wasapi.h>
#endif

struct testUserData {
    float left_phase;
    float right_phase;
};

struct audioProcessingUserData {

}

struct audioIoStreamOptions {
    unsigned int mCaptureChannels;
    unsigned int mPlaybackChannels;

    unsigned int mSampleRate;


};

static int AudioCallback(const void *inputBuffer, void *outputBuffer,  unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData);

class AudioIOBase {

public:
    unsigned int getPlaybackDevIndex();
    unsigned int getCaptureDevIndex();
    unsigned int getNumPlaybackChannels();
    unsigned int getNumCaptureChannels();
};



class AudioIOStream {

public:
    void initializeAudioStream(PaStreamParameters, PaStreamParameters, int,  PaStreamCallback CallbackFXN);
    void startStream();
    void endStream();

private:
    PaStream *mStream = nullptr;

public:
    bool getStreamStillRunning();
};

class AudioIoCallback
    : public AudioIOBase{

protected:
    //Audio thread Vars
    std::thread mAudioThread;

    std::atomic<bool> mKillAudioThread{false};
public:
    AudioIoCallback();
    ~AudioIoCallback();

    void AudioIOCallback();
protected:
    void doPlayback();
    void doInput();
};

class AudioIO
    : public AudioIoCallback{

    //use threads to handle input?
    //also for reading and writing the audio... output buffer should be filled by this
    //output should be handled by callback but input is taxing so maybe threading will be better so i dont delay audio proscessing

    AudioIOStream mAudioStream;

public:
  //Init Functions
    AudioIO();
    ~AudioIO();

    AudioIOStream* startStream();

    //audio processing functions (while part of stream)
    void audioThread();

private:

    bool initPortAudio();

    void startAudioThread();

};


#endif //AUDIOIO_H
