//
// Created by cline on 2025-01-28.
//

#include "AudioIO.h"

#include <cstdio>
#include <portaudio.h>

void AudioIOStream::initializeAudioStream(unsigned int numCaptureChannel, unsigned int numPlaybackChannel , PaStreamCallback CallbackFXN) {
    static testUserData data;

    PaError err = Pa_OpenDefaultStream(
         &stream,
         numCaptureChannel,
         numPlaybackChannel,
         paFloat32,
         44100,
         256,
         CallbackFXN,
         &data
     );
    printf("Pa_OpenDefaultStream returned %d\n", err);
    if (err != paNoError) printf("Error opening audio stream\n");
}

void AudioIOStream::startStream() {
    Pa_StartStream(stream);
    printf("Stream started\n");
}

void AudioIOStream::endStream() {
    Pa_StopStream(stream);
    printf("Stream ended\n");
}
