//
// Created by cline on 2025-01-28.
//

#include "AudioIO.h"

#include <cstdio>

void AudioIOStream::initializeAudioStream(PaStreamParameters inputParams, PaStreamParameters outputParams, int srate, PaStreamCallback CallbackFXN) {
    static testUserData data;

    PaError err = Pa_OpenStream(
        &mStream,
        &inputParams,
        &outputParams,
        srate,
        paFramesPerBufferUnspecified,
        paNoFlag,
        CallbackFXN,
        &data);

    // PaError err = Pa_OpenDefaultStream(
    //      &stream,
    //      numCaptureChannel,
    //      numPlaybackChannel,
    //      paFloat32,
    //      44100,
    //      256,
    //      CallbackFXN,
    //      &data
    //  );


    printf("Pa_OpenDefaultStream returned %d\n", err);
    if (err != paNoError) printf("Error opening audio stream\n");
}

void AudioIOStream::startStream() {
    Pa_StartStream(mStream);
    printf("Stream started\n");
}

void AudioIOStream::endStream() {
    Pa_StopStream(mStream);
    printf("Stream ended\n");
}

int AudioCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData) {
    /* Cast data passed through stream to our structure. */
    auto *data = (testUserData *)userData;
    auto *out = (float *)outputBuffer;
    (void) inputBuffer; /* Prevent unused variable warning. */



    return 0;
}


AudioIO::AudioIO() {
    if (!initPortAudio()) {
        //REPLACE WITH WARNING POPUP
        printf("Error initializing audio stream\n");
    }
}

AudioIO::~AudioIO() {

    //Handle Exiting IO

    //Kill Stream & callback
    if (!mAudioStream.getStreamStillRunning()) {
        mAudioStream.endStream();
    }


    Pa_Terminate();
    mAudioThread.join();

}

bool AudioIO::initPortAudio() {
    PaError err;

    err = Pa_Initialize();

    return (err == paNoError);
}

void AudioIO::startAudioThread() {
    mAudioThread = std::thread(&AudioIO::audioThread, std::ref(mKillAudioThread));
}

AudioIOStream* AudioIO::startStream() {

    return nullptr;
}



