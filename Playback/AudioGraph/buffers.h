/*
 * This file is part of VSoundCheckr
 * Copyright (C) 2025 Kieran Cline
 *
 * Licensed under the GNU General Public License v3.0
 * See LICENSE file for details.
 */

#ifndef BUFFERS_H
#define BUFFERS_H
#include <vector>

#include "../../Audio/SampleFormat.h"


namespace AudioGraph {
    class buffers {

        std::vector<std::vector<float>> mBuffers;
        std::vector<float *> mPositions;
        size_t mBuffersSize {0};
        size_t mBlockSize {0};

    public:
        explicit buffers(size_t blockSize = 512);
        buffers(size_t nChannels, size_t blockSize, size_t nBlocks, size_t padding = 0);

        unsigned channels() const {return mBuffers.size(); }
        size_t BufferSize() const {return mBuffersSize; }
        size_t BlockSize() const {return mBlockSize; }
        size_t Position() const {
            return mBuffers.empty() ? 0 : Positions()[0] - reinterpret_cast<const float*>(getReadPosition(0));
        }

        size_t Remaining() const {return BufferSize()-Position();}
        bool isRewound() const {return BufferSize()==Remaining();}

        float *const *Positions() const {return mPositions.data();}

        void reInit(size_t nChannels, size_t blockSize, size_t nBlocks, size_t padding = 0);

        constSamplePtr getReadPosition(unsigned channel) const;
        float &getWritePosition(unsigned channel);

        void ClearBuffer(unsigned channel, size_t n);


        //Discard, advance, rewind, rotate

        void Advance(size_t count);

        void Discard(size_t drop, size_t keep);

        void Rewind();

        size_t Rotate();
    };
}



#endif //BUFFERS_H
