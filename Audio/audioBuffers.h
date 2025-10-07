/*
 * This file is part of VSC+
 * Copyright (C) 2025 Kieran Cline
 *
 * Licensed under the GNU General Public License v3.0
 * See LICENSE file for details.
 */
#ifndef AUDIOBUFFERS_H
#define AUDIOBUFFERS_H

#include <atomic>

#include "../MemoryManagement/MemoryTypes.h"
#include "SampleFormat.h"

//THIS IS A CIRCULAR BUFFER
class audioBuffer{

private:

    size_t mWritten{0};
    size_t mLastPadding{0};

    NonInterfering<std::atomic<size_t>> mStart{0}, mEnd{0};

    const size_t mBufferSize = 44100;

    const SampleFormat mFormat;
    const SampleBuffer mBuffer;

public:
    audioBuffer(SampleFormat format, size_t size);
    ~audioBuffer();

    //for the writer only
    size_t availForPut() const;
    size_t writtenForGet() const;

    size_t Put(constSamplePtr buffer, SampleFormat format, size_t samples, size_t padding = 0);
    size_t unPut(size_t size);
    size_t Clear(SampleFormat format, size_t samples);
    std::pair<samplePtr, size_t> getUnflushed(unsigned iBlock);
    void Flush();

    //for the reader only
    size_t availForGet() const;
    size_t Get(samplePtr buffer, SampleFormat format, size_t samples);
    size_t discard(size_t samples);

private:
    size_t Filled(size_t start, size_t end) const;
    size_t Free(size_t start, size_t end) const;

};





#endif //AUDIOBUFFERS_H
