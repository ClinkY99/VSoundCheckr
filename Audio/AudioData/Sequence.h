//
// Created by cline on 2025-03-08.
//

#ifndef SEQUENCE_H
#define SEQUENCE_H
#include <deque>

#include "../SampleFormat.h"

class SeqBlock {
    SampleBuffer mBuffer;


public:
    SeqBlock();

};

class BlockArray : std::deque<SeqBlock> {};
using BlockPtrArray = std::deque<BlockArray*>;

class Sequence {
    BlockArray mLoadedBlocks;

};



#endif //SEQUENCE_H
