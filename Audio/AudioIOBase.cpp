//
// Created by cline on 2025-01-28.
//

#include "AudioIOBase.h"

#include <cstdio>

#include "AudioIO.h"

int AudioIOBase::testCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData) {
    /* Cast data passed through stream to our structure. */
    auto *data = (testUserData *)userData;
    auto *out = (float *)outputBuffer;
    (void) inputBuffer; /* Prevent unused variable warning. */

    printf("Left Phase: %f  Right Phase: %f \n", data->left_phase, data->right_phase);

    for( unsigned int i = 0; i<framesPerBuffer; i++ )
    {
        *out++ = data->left_phase;  /* left */
        *out++ = data->right_phase;  /* right */

        /* Generate simple sawtooth phaser that ranges between -1.0 and 1.0. */
        data->left_phase += 0.01f;
        /* When signal reaches top, drop back down. */
        if( data->left_phase >= 1.0f ) data->left_phase -= 2.0f;
        /* higher pitch so we can distinguish left and right. */
        data->right_phase += 0.03f;
        if( data->right_phase >= 1.0f ) data->right_phase -= 2.0f;
    }

    return 0;
}

void AudioIOBase::init() {
    AudioIOStream stream = AudioIOStream();

    if (initPortAudio()) {
        stream.initializeAudioStream(0,2, testCallback);
        stream.startStream();
        Pa_Sleep(10000);
        stream.endStream();
        Pa_Terminate();
    } else {
        printf("Error initializing audio stream\n");
    }
}


bool AudioIOBase::initPortAudio() {
    PaError err;

    err = Pa_Initialize();

    return (err == paNoError);
}
