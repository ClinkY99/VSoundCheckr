//
// Created by cline on 2025-03-08.
//

#ifndef AUDIOIOSEQUENCES_H
#define AUDIOIOSEQUENCES_H
#include <memory>
#include <vector>

#include "../../Audio/SampleCount.h"
#include "../../Audio/SampleFormat.h"
#include "../AudioGraph/Channel.h"


class PlaybackSequence : public AudioGraph::Channel {
    ~PlaybackSequence() override;

    virtual bool doGet(size_t channel, size_t nBuffers, const samplePtr buffers[], SampleFormat format, sampleCount count, size_t len, bool backwards = false) = 0;
    bool GetFloats(size_t channel, size_t nBuffers, float *const buffers[], sampleCount count, size_t len, bool backwards = false);

    virtual size_t NChannels() = 0;

    virtual double GetStartTime() const= 0;
    virtual double GetEndTime() const= 0;
    virtual double GetRate() const= 0;

    virtual bool isSolo() const = 0;
    virtual bool isMute() const = 0;

    double LongSamplesToTime(sampleCount samples) const;
    sampleCount TimeToLongSamples(double time) const;
};

using constPlayableSequences = std::vector<std::shared_ptr<const PlaybackSequence>>;

class RecordingSequence {
    virtual ~RecordingSequence();

    virtual SampleFormat GetSampleFormat() const = 0;
    virtual double GetRate() const = 0;

    virtual size_t NChannels() const = 0;

    virtual void append(size_t channel, constSamplePtr buffer, SampleFormat format, size_t len, unsigned int stride, SampleFormat effectiveFormat) = 0;

    virtual void Flush() = 0;

    virtual void InsertSilence(double t, double len) = 0;
};

using recordingSequences = std::vector<std::shared_ptr<RecordingSequence>>;

#endif //AUDIOIOSEQUENCES_H
