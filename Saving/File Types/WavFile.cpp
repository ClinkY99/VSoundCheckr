//
// Created by cline on 2025-12-02.
//

#include "WavFile.h"

void WavFile::openWavFile(FilePath wavFilePath)
{
    mWavFilePath = wavFilePath;

    mWavFile = ofstream(std::string(wavFilePath), ios::binary);

    mConnected = true;
    mWrittenSamples = 0;

    writeHeader();
}

void WavFile::closeWavFile()
{
    assert(mConnected && mNumSamples == mWrittenSamples);
    mConnected = false;
    mWavFile.close();
}

void WavFile::writeSamples(constSamplePtr src, size_t numSamples)
{
    assert(mConnected);

    mWavFile.write(src, numSamples*mHeader.blockAlign); //Pray this works :), note: originally didnt work :(

    mWrittenSamples += numSamples;
}

void WavFile::writeHeader()
{
    mWavFile.write(reinterpret_cast<char*>(&mHeader),sizeof(mHeader));
}
