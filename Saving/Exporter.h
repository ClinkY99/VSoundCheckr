//
// Created by cline on 2025-04-22.
//

#ifndef EXPORTER_H
#define EXPORTER_H
#include "../Audio/SampleFormat.h"
#include "../Midi/Snapshots.h"
#include "../Playback/Track.h"

enum exportFormat {
    WavFormat = 0,
    Mp3Format = 1
};

struct wavHeader {
    char chunkID[4] = {'R', 'I', 'F', 'F'};
    uint32_t chunkSize;
    char format[4] = {'W', 'A', 'V', 'E'};
    char subchunk1ID[4] = {'f', 'm', 't', ' '};
    uint32_t subchunk1Size = 16; // PCM format
    uint16_t audioFormat = 1;   // PCM
    uint16_t numChannels;   // Mono
    uint32_t sampleRate = 44100;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample = 16;
    char subchunk2ID[4] = {'d', 'a', 't', 'a'};
    uint32_t subchunk2Size;

};

class Exporter {
    exportFormat mFormat;

    //Audio Data
    Tracks mTracks;
    Snapshots mSnapshots;

public:
    Exporter()
        : mFormat() {
    }

    Exporter(const Tracks* tracks, const Snapshots *snapshots)
        :mFormat(), mTracks(*tracks), mSnapshots(*snapshots){
        showExportUI();
    };
