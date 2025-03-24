//
// Created by cline on 2025-03-08.
//

#ifndef SEQUENCE_H
#define SEQUENCE_H
#include <atomic>
#include <deque>

#include "SampleBlock.h"
#include "../SampleCount.h"
#include "../SampleFormat.h"

class SeqBlock {
public:
    SampleBlockPtr sb;
    sampleCount start;

    SeqBlock(){};
    SeqBlock(SampleBlockPtr _sb, sampleCount _start) : sb(_sb), start(_start) {};


};

using BlockArray = std::deque<SeqBlock>;
using BlockPtrArray = std::deque<BlockArray*>;

using SampleBlockFactoryPtr = std::shared_ptr<SampleBlockFactory>;

class Sequence {

    SampleBlockFactoryPtr mpFactory;
    SampleFormats mSampleFormats;

    SampleBuffer mAppendBuffer;
    size_t mAppendBufferLen = 0;
    SampleFormat mAppendEffectiveFormat = floatSample;

    BlockArray mBlocks;
    std::atomic<size_t> mBlockCount;
    sampleCount mSampleCount;

    size_t mMinSamples = 0;
    size_t mMaxSamples = 0;

    // STATIC MEMBERS
    static size_t sHardDiskBlockSize;

public:

    Sequence(SampleBlockFactoryPtr _factory, SampleFormats _formats);

    bool Append(constSamplePtr src, SampleFormat srcFormat, size_t len, size_t stride, SampleFormat effectiveFormat);
    void Flush();

    size_t GetIdealAppendLength() const;
    size_t GetMaxBlockSize() const {return mMaxSamples;}
    size_t GetMinBlockSize() const {return mMinSamples;}

    int FindBlock(sampleCount pos);
    bool getSamples(samplePtr dst, SampleFormat dstFormat, sampleCount start, size_t nSamples);

    size_t GetAppendBufferLen() const {return mAppendBufferLen;}
    sampleCount GetSampleCount() const {return mSampleCount;}

private:
    void AppendBlocks(BlockArray& additionalBlocks, bool replaceLast, sampleCount numSamples);

    void DoAppend(constSamplePtr src, SampleFormat srcFormat, size_t len);
    bool DoGet(int b, samplePtr dst, SampleFormat dstFormat, sampleCount start, size_t len);

public:
    static void setHardDiskBlockSize(size_t bytes) {sHardDiskBlockSize = bytes;}

    static bool read(samplePtr buffer, SampleFormat format, const SeqBlock& seqBlock, size_t blockRelativeStart, size_t len);
};



#endif //SEQUENCE_H
