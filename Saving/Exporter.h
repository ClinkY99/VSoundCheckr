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
    //Mp3Format = 1
};

class Exporter
{
    exportFormat mFormat;

    //Audio Data
    Tracks mTracks;
    Snapshots mSnapshots;

    size_t mBlockSize = 1048576;

    size_t mSamplesPerBlock;

public:
    Exporter()
        : mFormat() {
    }

    Exporter(const Tracks* tracks, Snapshots snapshots)
        :mFormat(), mTracks(*tracks), mSnapshots(snapshots){
        showExportUI();
    }

private:
    void showExportUI();

    void exportSnapshotsUI();
    void exportTimestampsUI();

    Tracks getTracksToExport();

    size_t parseTime(std::string time);

    std::pair<int,int> get2Ints();
    std::pair<double, double> get2Times();

    void exportWavSamples(FilePath path, Tracks tracks, sampleCount startLocation, sampleCount endLocation);
    void exportWavSnapshot(FilePath path, Tracks tracks, int startSnapshot, int endSnapshot);

    void exportWav(FilePath path, std::shared_ptr<Track> track, sampleCount startLocation, sampleCount endLocation);
};
#endif