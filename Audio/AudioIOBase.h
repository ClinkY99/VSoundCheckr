//
// Created by cline on 2025-01-28.
//

#ifndef AUDIOIOBASE_H
#define AUDIOIOBASE_H

#include <portaudio.h>

struct testUserData {
    float left_phase;
    float right_phase;
};

class AudioIOBase {

public:
    static void init();
    static int testCallback(const void *inputBuffer, void *outputBuffer,  unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData);
private:
    static bool initPortAudio();

};





#endif //AUDIOIOBASE_H
