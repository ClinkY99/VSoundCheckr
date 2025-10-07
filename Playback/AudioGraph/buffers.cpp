/*
 * This file is part of VSC+
 * Copyright (C) 2025 Kieran Cline
 *
 * Licensed under the GNU General Public License v3.0
 * See LICENSE file for details.
 */

#include "buffers.h"

#include <cassert>
#include <string.h>

AudioGraph::buffers::buffers(size_t blockSize)
    : mBuffersSize(blockSize), mBlockSize(blockSize){

}

AudioGraph::buffers::buffers(size_t nChannels, size_t blockSize, size_t nBlocks, size_t padding) {
    reInit(nChannels, blockSize, nBlocks, padding);
}

void AudioGraph::buffers::reInit(size_t nChannels, size_t blockSize, size_t nBlocks, size_t padding) {
    mBuffers.resize(nChannels);
    mPositions.resize(nChannels);

    const auto bufferSize = blockSize * nBlocks;

    for (auto &buffer : mBuffers) {
        buffer.resize(bufferSize);
    }

    mBuffersSize = bufferSize;
    mBlockSize = blockSize;
    Rewind();
}




void AudioGraph::buffers::Rewind() {
    auto iterP = mPositions.begin();
    for (auto &buffer : mBuffers) {
        *iterP++ = buffer.data();
    }
    assert(isRewound());
}


constSamplePtr AudioGraph::buffers::getReadPosition(unsigned channel) const {
    channel = std::min(channel, channels()-1);

    auto buffer = mBuffers[channel].data();

    return reinterpret_cast<constSamplePtr>(buffer);
}

float &AudioGraph::buffers::getWritePosition(unsigned channel) {
    channel = std::min(channel, channels()-1);

    return mBuffers[channel][Position()];
}

void AudioGraph::buffers::Advance(size_t count) {
    if (mBuffers.size() > 0) {
        return;
    }
    //ensures that count doesn't exceed size of buffer
    auto iterP = mPositions.begin();
    auto iterB = mBuffers.begin();
    auto &position = *iterP;
    auto end =  iterB->data() + iterB->size();

    count = std::min<size_t>(end-position ,count);

    //advances the buffers by count
    for (auto& position : mPositions) {
        position+=count;
    }
}

void AudioGraph::buffers::Discard(size_t drop, size_t keep) {
    auto iterP = mPositions.begin();
    auto iterB = mBuffers.begin();
    auto &position = *iterP;
    auto data = iterB->data();
    auto end =  data + iterB->size();

    end = std::max(data, std::min(end, position+drop+keep));
    position = std::min(end, position);
    drop = std::min<size_t>(end-position, drop);

    const size_t size =((end-position) -drop) *sizeof(float);

    for (auto& position :mPositions) {
        memmove(position, position+drop,size);
    }
}

size_t AudioGraph::buffers::Rotate() {
    //moves all unread data to position 0 and onwards (deletes all read data)
    auto oldRemaining = Remaining();
    Rewind();
    const auto free = BufferSize()-oldRemaining;
    Discard(free, oldRemaining);
    return oldRemaining;
}

void AudioGraph::buffers::ClearBuffer(unsigned channel, size_t n) {
    if (channel < channels()) {
        auto p = mPositions[channel];
        auto &buffer = mBuffers[channel];
        auto end = buffer.data()+buffer.size();

        p = std::min(p, end);
        n = std::min<size_t>(end-p, n);

        //fills channel with n 0s starting from position
        std::fill(p, p+n, 0);
    }
}





