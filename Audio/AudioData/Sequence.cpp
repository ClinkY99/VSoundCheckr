//
// Created by cline on 2025-03-08.
//

#include "Sequence.h"

#include <cassert>
#include <iostream>
#include <wx/cpp.h>
#include <wx/debug.h>
#include <wx/types.h>

#include "../Dither.h"

size_t Sequence::sHardDiskBlockSize = 1048576;

inline bool Overflows(size_t numSampels) {
    return numSampels > wxLL(9223372036854775807);
}

Sequence::Sequence(SampleBlockFactoryPtr _factory, SampleFormats _formats) :
    mpFactory(_factory), mSampleFormats{ _formats }, mMinSamples(sHardDiskBlockSize/SAMPLE_SIZE(mSampleFormats.getEffective())/2), mMaxSamples(mMinSamples*2) {
}

void Sequence::Flush() {
    if (mAppendBufferLen >0) {
        DoAppend(mAppendBuffer.ptr(), mSampleFormats.getStored(), mAppendBufferLen);
        mSampleFormats.updateEffective(mAppendEffectiveFormat);

        mAppendBufferLen = 0;
        mAppendBuffer.Free();
        mAppendEffectiveFormat =narrowestSampleFormat;
    }


}


bool Sequence::Append(constSamplePtr src, SampleFormat srcFormat, size_t len, size_t stride, SampleFormat effectiveFormat) {
    effectiveFormat = std::min(effectiveFormat, srcFormat);
    const auto seqFormat = mSampleFormats.getStored();
    if (!mAppendBuffer.ptr()) {
        mAppendBuffer.Allocate(mMaxSamples, seqFormat);
    }

    bool result = false;
    auto blockSize = GetIdealAppendLength();

    while (true) {
        if (mAppendBufferLen >= blockSize) {
            //Append the buffer len
            DoAppend(mAppendBuffer.ptr(), seqFormat, blockSize);

            //Change the effective format now that do append succeeded.
            mSampleFormats.updateEffective(mAppendEffectiveFormat);

            //Finish up the append
            result = true;
            memmove(mAppendBuffer.ptr(), mAppendBuffer.ptr()+blockSize*SAMPLE_SIZE(seqFormat), (mAppendBufferLen-blockSize)*SAMPLE_SIZE(seqFormat));

            mAppendBufferLen -= blockSize;
            blockSize = GetIdealAppendLength();
        }

        if (len==0) {
            break;
        }

        assert(mAppendBufferLen <= mMaxSamples);
        auto toCopy = std::min(len, mMaxSamples - mAppendBufferLen);

        CopySamples(src, srcFormat, mAppendBuffer.ptr()+mAppendBufferLen*SAMPLE_SIZE(seqFormat), seqFormat, toCopy, seqFormat<effectiveFormat ? gHighQualDither : DitherType::none, stride);
        mAppendEffectiveFormat = std::max(effectiveFormat, mAppendEffectiveFormat);
        mAppendBufferLen += toCopy;

        src += toCopy*SAMPLE_SIZE(srcFormat)*stride;

        len-=toCopy;
    }
    return result;
}

void Sequence::DoAppend(constSamplePtr src, SampleFormat srcFormat, size_t len) {
    if (len == 0) {
        return;
    }

    auto &factory = mpFactory;

    //Ensure it doesnt overflow int limit
    if (Overflows(mSampleCount.as_double()+(double) len)) wxASSERT(false);

    BlockArray newBlocks;
    sampleCount newNumSamples = mSampleCount;

    int numBlocks = mBlocks.size();
    SeqBlock *pLastBlock;
    decltype(pLastBlock->sb->getSampleCount()) length;
    size_t bufferSize = mMaxSamples;
    const auto dstFormat = mSampleFormats.getStored();
    SampleBuffer buffer(bufferSize, dstFormat);
    bool replaceLast = false;

    std::vector<float> test;
    test.resize(len);

    CopySamples(src, floatSample, (samplePtr)test.data(), floatSample, len, none);

    //If the last block isnt already full fill it
    if (numBlocks > 0 &&
        (length = (pLastBlock = &mBlocks.back())->sb->getSampleCount()) < mMinSamples) {
        const SeqBlock &lastBlock = *pLastBlock;
        const auto addLen = std::min(mMaxSamples-length, len);

        read(buffer.ptr(), dstFormat, lastBlock, 0, length);

        CopySamples(src, srcFormat, buffer.ptr()+length*SAMPLE_SIZE(dstFormat), dstFormat, addLen, DitherType::none);

        const auto newLastBlockLen = length + addLen;
        SampleBlockPtr pBlock = factory->Create(buffer.ptr(), dstFormat, newLastBlockLen);
        SeqBlock newLastBlock(pBlock, lastBlock.start);

        newBlocks.push_back(newLastBlock);

        len-=addLen;
        newNumSamples += addLen;
        src += addLen*SAMPLE_SIZE(srcFormat);

        replaceLast = true;
    }

    //Append the rest as new blocks
    while (len) {
        const auto idealBlockSize = mMaxSamples;
        const auto addedSamples = std::min(idealBlockSize, len);
        SampleBlockPtr pBlock;

        if (srcFormat == dstFormat) {
            pBlock = factory->Create(src, dstFormat, addedSamples);
        } else {
            CopySamples(src,srcFormat, buffer.ptr(), dstFormat, addedSamples, DitherType::none);

            pBlock = factory->Create(buffer.ptr(), dstFormat, addedSamples);
        }

        newBlocks.push_back(SeqBlock(pBlock, newNumSamples));

        len-=addedSamples;
        newNumSamples += addedSamples;
        src+=addedSamples*SAMPLE_SIZE(srcFormat);
    }

    AppendBlocks(newBlocks, replaceLast, newNumSamples);
}

bool Sequence::read(samplePtr buffer, SampleFormat format, const SeqBlock &seqBlock, size_t blockRelativeStart, size_t len) {
    auto &sb = seqBlock.sb;

    assert(blockRelativeStart+len <= sb->getSampleCount());

    auto result = sb->GetSamples(buffer, format, blockRelativeStart, len);

    if (result != len) {
        //EXPECTED A DIFFERENT AMOUNT OF SAMPLES
        wxASSERT(false);
        return false;
    }

    return true;
}


void Sequence::AppendBlocks(BlockArray &additionalBlocks, bool replaceLast, sampleCount numSamples) {
    if (additionalBlocks.empty()) {
        return;
    }

    if (replaceLast && !mBlocks.empty()) {
        mBlockCount.store(mBlocks.size(), std::memory_order::relaxed);
        mBlocks.pop_back();
    }
    std::copy(additionalBlocks.begin(), additionalBlocks.end(), std::back_inserter(mBlocks));

    mSampleCount = numSamples;

    mBlockCount.store(mBlocks.size(), std::memory_order::relaxed);
}

size_t Sequence::GetIdealAppendLength() const {
    const size_t numBlocks = mBlockCount.load(std::memory_order_relaxed);

    const auto max = GetMaxBlockSize();

    if (numBlocks == 0) {
        return max;
    }

    const auto lastBlockSize = mBlocks.back().sb->getSampleCount();

    if (lastBlockSize >= max) {
        return max;
    }
    return max-lastBlockSize;
}


bool Sequence::getSamples(samplePtr dst, SampleFormat dstFormat, sampleCount start, size_t nSamples) {
    const auto sampleSize = SAMPLE_SIZE(dstFormat);
    bool bOutOfBounds = false;

    if (start <0) {
        const auto fillLen = LimitSampleBufferSize(nSamples, -start);
        ClearSamples(dst, dstFormat, 0, fillLen);
        //entire request OOB
        if (nSamples = fillLen) {
            return false;
        }
        start = 0;
        dst += fillLen*sampleSize;
        nSamples -= fillLen;
        bOutOfBounds = true;
    }
    if (start >= mSampleCount) {
        ClearSamples(dst, dstFormat, 0, nSamples);
        return false;
    }

    if (start+nSamples > mSampleCount) {
        const auto excess = (start + nSamples - mSampleCount).as_size_t();
        ClearSamples(dst, dstFormat, nSamples-excess, excess);

        nSamples-=excess;
        bOutOfBounds = true;
    }

    int b = FindBlock(start);

    return DoGet(b, dst, dstFormat, start, nSamples) && !bOutOfBounds;
}

bool Sequence::DoGet(int b, samplePtr dst, SampleFormat dstFormat, sampleCount start, size_t len) {
    bool result = true;
    while (len) {
        const SeqBlock &block = mBlocks[b];

        const size_t bStart = (start-block.start).as_size_t();

        const auto bLen = std::min(len, block.sb->getSampleCount()-bStart);

        if (!read(dst,dstFormat, block, bStart, bLen)) {
            result = false;
        }

        len -= bLen;
        dst+=bLen*SAMPLE_SIZE(dstFormat);
        b++;
        start +=bLen;
    }
    return result;
}


int Sequence::FindBlock(sampleCount pos) {

    if (pos ==0) {
        return 0;
    }

    const size_t numBlocks = mBlockCount.load(std::memory_order_relaxed);

    size_t low = 0,hi =numBlocks, guess;
    sampleCount lowSamples = 0, hiSamples = mSampleCount;
    //Perform dictionary search
    while (true) {
        const double frac = (pos-lowSamples).as_double()/(hiSamples-lowSamples).as_double();
        guess = std::min(hi-1, size_t(frac*(hi-low)));
        const SeqBlock &block = mBlocks[guess];

        if (pos < block.start) {
            assert(guess != low);

            hi = guess;
            hiSamples = block.start;
        } else {
            const sampleCount nextStart = block.start + block.sb->getSampleCount();

            if (pos < nextStart) {
                break;
            }

            low = guess +1;
            lowSamples = nextStart;
        }
    }

    const int rval = guess;
    assert(rval >= 0 && rval < numBlocks &&
             pos >= mBlocks[rval].start &&
             pos < mBlocks[rval].start + mBlocks[rval].sb->getSampleCount());

    return rval;
}

void Sequence::loadBlockFromID(int id){
    auto newBlock = mpFactory->CreateFromID(floatSample,id);

    mBlocks.push_back(SeqBlock(newBlock, mSampleCount));
    mSampleCount+= newBlock->getSampleCount();

    mBlockCount.store(mBlocks.size(), std::memory_order_relaxed);
}
