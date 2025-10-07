/*
 * This file is part of SoundCheckr
 * Copyright (C) 2025 Kieran Cline
 *
 * Licensed under the GNU General Public License v3.0
 * See LICENSE file for details.
 */

#include "AudioIOSequences.h"

#include <cmath>
#include <format>
#include <regex>

bool PlaybackSequence::GetFloats(size_t channel, samplePtr buffer, sampleCount start, size_t len, bool backwards) const{
    auto result = doGet(channel, buffer, floatSample, start, len, backwards);

    //if get fails empty buffer.
    if (!result) ClearSamples(buffer, floatSample, 0, len);

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



