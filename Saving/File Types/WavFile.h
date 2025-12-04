//
// Created by cline on 2025-12-02.
//

#ifndef VSOUNDCHECKR_WAVFILE_H
#define VSOUNDCHECKR_WAVFILE_H
#include <cstdint>
#include <iostream>
#include <fstream>

#include "../DBConnection.h"
#include "../../Audio/audioBuffers.h"

struct wavHeader {
    char chunkID[4] = {'R', 'I', 'F', 'F'};
    uint32_t chunkSize = 36;
    char format[4] = {'W', 'A', 'V', 'E'};
    char subchunk1ID[4] = {'f', 'm', 't', ' '};
    uint32_t subchunk1Size = 16; // PCM format
    uint16_t audioFormat = 1;   // PCM
    uint16_t numChannels = 1;   // Mono
    uint32_t sampleRate = 44100;
    uint32_t byteRate = 176400; // (Sample Rate * BitsPerSample * Channels) / 8
    uint16_t blockAlign = 4; // Channels * BitsPerSample/8
    uint16_t bitsPerSample = 32; //SAMPLE_SIZE(sampletype)
    char subchunk2ID[4] = {'d', 'a', 't', 'a'};
    uint32_t subchunk2Size;
};

using namespace std;

class WavFile
{
    wavHeader mHeader;

    SampleFormat mFormat;

    ofstream mWavFile;
    bool mConnected = false; //is it connected to a wavFile object

    size_t mNumSamples;
    size_t mWrittenSamples;

    FilePath mWavFilePath;

public:
    WavFile(size_t sampleRate, size_t numChannels, SampleFormat sampleFormat, size_t numSamples)
    {
        mHeader.numChannels = numChannels;
        mHeader.audioFormat = (sampleFormat == floatSample ) ? 3:1;
        mHeader.sampleRate = sampleRate;
        mHeader.bitsPerSample = SAMPLE_SIZE(sampleFormat)*8;
        mHeader.byteRate = (sampleRate * mHeader.bitsPerSample * numChannels)/8;
        mHeader.blockAlign = numChannels * mHeader.bitsPerSample / 8;
        mHeader.subchunk2Size = numSamples*mHeader.blockAlign;
        mHeader.chunkSize = 36 + mHeader.subchunk2Size; //36 is number of bytes in header - 8

        mFormat = sampleFormat;
        mNumSamples = numSamples;
        mWrittenSamples = 0;
    }

    void openWavFile(FilePath wavFilePath);
    void closeWavFile();

    void writeSamples(constSamplePtr src, size_t numSamples);


private:
    void writeHeader();
};


#endif //VSOUNDCHECKR_WAVFILE_H