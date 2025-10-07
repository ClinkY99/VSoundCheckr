/*
 * This file is part of VSC+
 * Copyright (C) 2025 Kieran Cline
 *
 * Licensed under the GNU General Public License v3.0
 * See LICENSE file for details.
 */

#ifndef TRACK_H
#define TRACK_H
#include <atomic>

#include "../Audio/AudioData/Sequence.h"
#include "../Saving/SaveFileDB.h"
#include "Sequences/AudioIOSequences.h"


class Track
    : public RecordingSequence
    , public PlaybackSequence
    , public SaveBase {

    int mFirstChannelNumIn = -1;
    int mFirstChannelNumOut = -1;

    int mNumChannels = 1;

    double mRate;
    SampleFormat mFormat;

    bool mAppendSecondNext = false;

    std::atomic<bool> mSolo;
    std::atomic<bool> mMute;

    std::vector<std::unique_ptr<Sequence>> mSequences;

public:
    Track(double rate, SampleFormat format)
        :mRate(rate), mFormat(format), mSolo(false), mMute(false) {
        updateSequences();
    }

    void changeInChannel(int channelNum) {mFirstChannelNumIn = channelNum;}
    void changeOutChannel(int channelNum) {mFirstChannelNumOut = channelNum;}

    void changeTrackType(AudioGraph::ChannelType newType) {mNumChannels = newType+1; updateSequences();}

    void setRate(double rate) {mRate = rate;} ;

    double getLengthS(){return mSequences[0]->GetSampleCount().as_double()/mRate;}

    //saving
    void save() override;
    void newShow() override;
    void load(int id) override;

    //overrides
    size_t NChannels() const override {return mNumChannels;}
    bool appendSecond() const override {return mAppendSecondNext;}
    void toggleAppendSecond() override {mAppendSecondNext = !mAppendSecondNext;}
    AudioGraph::ChannelType getChannelType() const override {return mNumChannels>1 ? AudioGraph::SterioChannel : AudioGraph::MonoChannel;}
    int GetFirstChannelIN() const override {return mFirstChannelNumIn;}
    int GetFirstChannelOut() const override {return mFirstChannelNumOut;}
    double GetRate() const override {return mRate;}
    bool isValid() const override {return mFirstChannelNumIn >=0 && mFirstChannelNumOut>=0;}

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
    void updateSequences();
};

using Tracks = std::vector<std::shared_ptr<Track>>;



#endif //TRACK_H
