//
// Created by cline on 2025-03-08.
//

#include "AudioIOSequences.h"

#include <cmath>
#include <format>
#include <regex>

bool PlaybackSequence::GetFloats(size_t channel, size_t nBuffers, float *const buffers[], sampleCount count, size_t len, bool backwards) {
    auto castBuffers = reinterpret_cast<const samplePtr*>(buffers);
    auto result = doGet(channel, nBuffers, castBuffers, floatSample, count, len, backwards);

    //if get fails empty buffer.
    if (!result) while (nBuffers--) ClearSamples(castBuffers[nBuffers], floatSample, 0, len);

    return result;
}

double PlaybackSequence::LongSamplesToTime(sampleCount samples) const {
    return samples.as_double()/ GetRate();
}

sampleCount PlaybackSequence::TimeToLongSamples(double time) const {
    return sampleCount { floor(time*GetRate() +.5f) };
}

PlaybackSequence::~PlaybackSequence() =default;
RecordingSequence::~RecordingSequence() =default;



