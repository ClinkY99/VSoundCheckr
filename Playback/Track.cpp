//
// Created by cline on 2025-01-29.
//

#include "Track.h"

#include <wx/debug.h>


bool Track::append(size_t channel, constSamplePtr buffer, SampleFormat format, size_t len, unsigned int stride, SampleFormat effectiveFormat) {
    wxASSERT(channel < NChannels());
    bool appended = false;
    appended = mSequences[channel]->Append(buffer, format, len, stride, effectiveFormat);

    return appended;
}

void Track::Flush() {
    if (GetGreatestAppendBufferLen() > 0) {
        for (auto&
            seq: mSequences) {
            seq->Flush();
        }
    }
}

size_t Track::GetGreatestAppendBufferLen() const {
    size_t result = 0;
    for (int i = 0; i < NChannels(); ++i) {
        result = std::max(result, mSequences[i]->GetAppendBufferLen());
    }

    return result;
}

bool Track::doGet(size_t channel, samplePtr buffer, SampleFormat format, sampleCount start, size_t len, bool backwards) const {
    if (backwards) {
        start -= len;
    }
    auto samplesToCopy = std::min((sampleCount)len, mSequences[channel]->GetSampleCount());

    bool result = mSequences[channel]->getSamples(buffer, format, start, samplesToCopy.as_size_t());

    if (result && backwards) {
        ReverseSamples(buffer, format, 0, len);
    }

    return result;
}


