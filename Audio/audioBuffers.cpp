//
// Created by cline on 2025-02-20.
//

#include "audioBuffers.h"

#include <format>
#include <wx/debug.h>

#include "Dither.h"

audioBuffer::audioBuffer(SampleFormat format, size_t size)
    : mBufferSize(std::max<size_t>(size, 64)), mFormat{format}, mBuffer{size, mFormat} {

}

audioBuffer::~audioBuffer() {

}

size_t audioBuffer::Filled(size_t start, size_t end) const {
    return (end + mBufferSize - start) % mBufferSize;
}

size_t audioBuffer::Free(size_t start, size_t end) const {
    return std::max<size_t>(mBufferSize - Filled(start, end), 4) - 4;
}



//Reader Only
size_t audioBuffer::availForGet() const {
    auto start = mStart.load(std::memory_order_relaxed);
    auto end = mEnd.load(std::memory_order_relaxed);
    return Filled(start, end);
}

size_t audioBuffer::Get(samplePtr buffer, SampleFormat format, size_t samples) {
    auto start = mStart.load(std::memory_order_relaxed);
    auto end = mEnd.load(std::memory_order_acquire);
    samples = std::min(samples, Filled(start, end));
    auto dest = buffer;
    size_t copied = 0;

    while (samples) {
        auto block = std::min(samples, mBufferSize-start);

        CopySamples(mBuffer.ptr() + start*SAMPLE_SIZE(format), mFormat, dest, format, block, DitherType::none);

        dest += block*SAMPLE_SIZE(format);
        start = (start + block) % mBufferSize;
        samples -= block;
        copied += block;
    }

    mStart.store(start, std::memory_order_release);

    return copied;
}

size_t audioBuffer::discard(size_t samples) {
    auto start = mStart.load(std::memory_order_relaxed);
    auto end = mEnd.load(std::memory_order_relaxed);
    samples = std::min(samples, Filled(start, end));

    mStart.store((start+samples)%mBufferSize, std::memory_order_release);

    return samples;
}

//Writer Only
size_t audioBuffer::availForPut() const {
    auto start = mStart.load(std::memory_order_relaxed);
    return Free(start, mWritten);
}

size_t audioBuffer::writtenForGet() const {
    auto start = mStart.load(std::memory_order_relaxed);
    return Filled(start, mWritten);
}

size_t audioBuffer::Put(constSamplePtr buffer, SampleFormat format, size_t samples, size_t padding) {
    mLastPadding = padding;
    auto start = mStart.load(std::memory_order_acquire);
    auto end = mWritten;
    const auto free = Free(start, end);
    samples = std::min(samples, free);
    padding = std::min(padding, free-samples);
    auto src = buffer;
    int copied = 0;
    auto pos = end;

    while (samples) {
        auto block = std::min(samples, mBufferSize - pos);

        CopySamples(src, format, mBuffer.ptr() + pos*SAMPLE_SIZE(format), format, block, DitherType::none);

        src += block*SAMPLE_SIZE(format);
        pos = (block+pos) % mBufferSize;
        copied+= block;
        samples-=block;
    }

    while (padding) {
        auto block = std::min(padding, mBufferSize - pos);

        ClearSamples(mBuffer.ptr(), format, pos, block);

        pos = (block+pos) % mBufferSize;
        copied += block;
        padding -=block;
    }

    mWritten = pos;
    return copied;
}

size_t audioBuffer::unPut(size_t size) {
    auto sampleSize = SAMPLE_SIZE(mFormat);
    auto buffer = mBuffer.ptr();

    auto end = mEnd.load(std::memory_order_relaxed);
    size = std::min(size, Filled(end, mWritten));
    const auto result = size;

    auto limit = end >= mWritten ? mWritten : mBufferSize;
    auto source = std::min(end+size, limit);
    auto count = limit-source;
    auto pDst = buffer + end * sampleSize;
    auto pSrc = buffer + source*sampleSize;

    //Destination, source, amount
    memmove(pDst, pSrc, count*sampleSize);

    size -= (source-end);

    if (end >= mWritten) {
        end+=count;
        pDst = buffer+end*sampleSize;
        pSrc = buffer+size*sampleSize;
        auto toMove = mWritten - size;
        auto toMove1 = std::min(toMove, end - mBufferSize);
        auto toMove2 = toMove - toMove1;

        memmove(pDst, pSrc, toMove1*sampleSize);
        memmove(pDst, pSrc + toMove1*sampleSize, toMove2*sampleSize);
    }

    mWritten = (mWritten + (mBufferSize-result)) % mBufferSize;

    mLastPadding = std::min(mLastPadding, Filled(end, mWritten));

    return result;

}

size_t audioBuffer::Clear(SampleFormat format, size_t samples) {
    auto start = mStart.load(std::memory_order_acquire);
    auto end = mWritten;
    samples = std::min(samples, Free(start, end));
    auto cleared = 0;

    auto pos = end;

    while (samples) {
        auto block = std::min(samples, mBufferSize - pos);

        ClearSamples(mBuffer.ptr(), format, pos, block);

        pos = (block+pos) % mBufferSize;
        cleared = block;
        samples -= block;
    }

    mWritten = cleared;

    return cleared;
}

std::pair<samplePtr, size_t> audioBuffer::getUnflushed(unsigned iBlock) {
    auto end = mEnd.load(std::memory_order_relaxed);
    auto size = Filled(end, mWritten) - mLastPadding;

    auto size0 = std::min(size, mBufferSize - end);
    auto size1 = size - size0;

    if (iBlock == 0)
        return {
            size0?
                mBuffer.ptr()+end*SAMPLE_SIZE(mFormat) : nullptr,
            size0};
    else
        return {
          size0 ?
              mBuffer.ptr(): nullptr, size0
        };

}


void audioBuffer::Flush() {
    mEnd.store(mWritten, std::memory_order_release);
    mLastPadding = 0;
}










