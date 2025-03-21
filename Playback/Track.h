//
// Created by cline on 2025-01-29.
//

#ifndef TRACK_H
#define TRACK_H
#include <atomic>

#include "../Audio/AudioData/Sequence.h"
#include "Sequences/AudioIOSequences.h"


class Track
    : public RecordingSequence
    , public PlaybackSequence {

    int mInDevice;
    int mOutDevice;

    int mNumChannels;

    double mRate;
    SampleFormat mFormat;

    std::atomic<bool> mSolo;
    std::atomic<bool> mMute;

    std::vector<std::unique_ptr<Sequence>> mSequences;

public:
    Track();

    void changeInDevice(int) {};
    void changeOutDevice(int) {};
    void changeNumChannels(int) {};

    //overrides
    size_t NChannels() const override {return mNumChannels;}
    double GetRate() const override {return mRate;}

    //Recording specific Sequence Overrides
    SampleFormat GetSampleFormat() const override {return mFormat;}
    bool append(size_t channel, constSamplePtr buffer, SampleFormat format, size_t len, unsigned int stride, SampleFormat effectiveFormat) override;
    void Flush() override;

    //Playback Sequence specific Overrides
    bool doGet(size_t channel, samplePtr buffer, SampleFormat format, sampleCount start, size_t len, bool backwards) const override;

    bool isSolo() const override {return mSolo.load(std::memory_order_relaxed);}
    bool isMute() const override {return mMute.load(std::memory_order_relaxed);}

private:
    size_t GetGreatestAppendBufferLen() const;
};



#endif //TRACK_H
