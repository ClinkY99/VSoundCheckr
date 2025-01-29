//
// Created by cline on 2025-01-28.
//

#ifndef AUDIOIO_H
#define AUDIOIO_H

#include "AudioIOBase.h"


class AudioIOStream {

public:
    void initializeAudioStream(unsigned int numCaptureChannel, unsigned int numPlaybackChannel, PaStreamCallback CallbackFXN);
    void startStream();
    void endStream();

private:
    PaStream *stream = nullptr;
};




#endif //AUDIOIO_H
