//
// Created by cline on 2025-02-20.
//

#ifndef AUDIOBUFFERS_H
#define AUDIOBUFFERS_H

#include <atomic>

#include "../MemoryManagement/MemoryX.h"


class audioBuffers {

private:
    NonInterfering<std::atomic<size_t>> mStart{0}, mEnd{0};

    const size_t mBufferSize = 44100;



public:
    audioBuffers();
    ~audioBuffers();

private:










};



#endif //AUDIOBUFFERS_H
