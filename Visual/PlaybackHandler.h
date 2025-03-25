//
// Created by cline on 2025-03-21.
//

#ifndef PLAYBACKHANDLER_H
#define PLAYBACKHANDLER_H
#include <portaudio.h>

#include "../Audio/AudioData/SqliteSampleBlock.h"
#include "../Audio/IO/AudioIO.h"


class PlaybackHandler {
    PaDeviceIndex mAudioInDev = 0;
    PaDeviceIndex mAudioOutDev = 0;
    PaHostApiIndex mHostApi = 0;

    FilePath saveFile;

    AudioIO *mAudioIO = AudioIO::Get();

    size_t mRate;
    size_t mNumInputs;
    size_t mNumOutputs;

    Tracks mTracks;

    std::atomic_bool mStopUiThread = false;
    std::thread mUiThread;

public:
    ~PlaybackHandler();

    void StartCApp();
    void Save(){}

    //Playback
    void Play();
    void stopPlayback();
    void Record();
    void endRecording();

    //Port Audio Stuff
    PaDeviceIndex GetAudioInDevice() const {return mAudioInDev;}
    PaDeviceIndex GetAudioOutDevice() const {return mAudioOutDev;}

    //Track Stuff
    void ViewTracks();
    bool newTrack();
    void removeTrack(int trackNdx);
    bool changeTrackType(int trackNdx, AudioGraph::ChannelType type);
    bool changeInChannels(int trackNdx, int channelNum);
    bool changeOutChannels(int trackNdx, int channelNum);

    //Settings Stuff
    void changeAudioInDev();
    void changeAudioOutDev();
    void changeAudioAPI();
    void changeSRate();

private:
    void MenuAPP();
    void TracksMenu();
    void AudioSettingsMenu();
    void PlaybackMenu();
    void RecordMenu();
    void RecordUIThread();
    void PlayMenu();
    void PlayUIThread(bool &loop);

    std::string buildFileName();
    void createAudioTempDB();

    bool attemptPopulateAutoIOTrack(int trackIndex, bool fullReset = false);
    bool attemptPopulateIn(int trackNdx);
    bool attemptPopulateOut(int trackNdx, bool fullReset);
    int propagateDevChange(int trackIndex = 0);

    //Settings Stuff
    void sRateDefault();
    void getSupportedRates (std::vector<size_t>& rates);
    void updateSRates();

    //CMDL IO stuff
    void waitForKeyPress();
    bool YNConfirm();
    void clrscr();
    int inputTrackNum();
};



#endif //PLAYBACKHANDLER_H
