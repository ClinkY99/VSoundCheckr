//
// Created by cline on 2025-03-21.
//

#ifndef PLAYBACKHANDLER_H
#define PLAYBACKHANDLER_H
#include <portaudio.h>

#include "../Audio/AudioData/SqliteSampleBlock.h"
#include "../Audio/IO/AudioIO.h"
#include "../Midi/Snapshots.h"
#include "../Saving/SaveFileDB.h"


class MidiIO;

class PlaybackHandler
    : public SaveBase
    ,public std::enable_shared_from_this<PlaybackHandler>{

    PaDeviceIndex mAudioInDev = 0;
    PaDeviceIndex mAudioOutDev = 0;
    PaHostApiIndex mHostApi = 0;

    FilePath mSaveFile;

    AudioIO *mAudioIO = AudioIO::Get();

    size_t mRate;
    size_t mNumInputs = 0;
    size_t mNumOutputs = 0;

    Tracks mTracks;

    std::atomic_bool mStopUiThread = false;
    std::thread mUiThread;

    bool mUnSaved = false;

    std::atomic_int midiAction = -1;

    std::atomic_bool mRecording = false;
    std::atomic_bool mPlaying = false;

    enum midiActions {
        mPlayPC = 0,
        mRecordPC = 1,
        mPausePC = 2,
        mStopPC = 3,
        mJumpF30Sec = 4,
        mJumpB30Sec = 5,
        mJumpF15Sec = 6,
        mJumpB15Sec = 7,
        mBackToSnapshot = 8
    };

    std::shared_ptr<MidiIO> mMidiIO;

    int snapshotNum = 0;
    std::shared_ptr<SnapshotHandler> mSnapshotHandler;

public:
    ~PlaybackHandler();

    void StartCApp();
    void newSave();
    void save() override;
    void newShow() override;
    void load(int = 0) override;

    //Playback
    bool Play();
    void stopPlayback();
    bool Record();
    void endRecording();

    //MIDI
    void setMidiAction(int action) {midiAction.store(action, std::memory_order_release);}
    void midiHandler();

    //Snapshot Stuff
    void snapshotAction(snapshotMidi md);
    void snapshotAction(int k);

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
    void RecordMidiUI();
    void PlayMenu();
    void PlayUIThread();
    void PlayMidiUI();
    void SaveMenu();
    void SnapshotMenu();
    void midiMenu();

    //Snapshot Stuff
    void viewSnapshots();
    bool goToSnapshot();
    void changeSnapshotsName();

    std::string buildFileName();
    void createAudioTempDB();
    void copyAudioTempDBToMainSave() const;

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
