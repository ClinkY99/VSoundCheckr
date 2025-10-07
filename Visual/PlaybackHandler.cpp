/*
 * This file is part of SoundCheckr
 * Copyright (C) 2025 Kieran Cline
 *
 * Licensed under the GNU General Public License v3.0
 * See LICENSE file for details.
 */

#include "PlaybackHandler.h"

#include <iostream>
#include <wx/filedlg.h>
#include <wx/translation.h>
#include <wx/msw/filedlg.h>

#include "AppBase.h"
#include "../Midi/MidiIO.h"

using namespace std;

PlaybackHandler::~PlaybackHandler() {
    Pa_Terminate();
    AudioIO::DeInit();
    mSaveConn->close();
}


//CMDL IO
//------------------------------------------------------------------------------------
void PlaybackHandler::waitForKeyPress() {
    cout << "<Press any key to continue>" << endl;
    cin.sync();
    cin.get();
}

bool PlaybackHandler::YNConfirm() {
    char answer;
    cout << "(Y/N)" << endl;
    cout<< ">> ";
    cin>>answer;

    if (answer == 'Y' || answer == 'y') {
        return true;
    }

    return false;
}

void clrscrSafe() {
#ifdef NDEBUG
    #if defined _WIN32
        system("cls");
        //clrscr(); // including header file : conio.h
    #elif defined (__LINUX__) || defined(__gnu_linux__) || defined(__linux__)
        system("clear");
        //std::cout<< u8"\033[2J\033[1;1H"; //Using ANSI Escape Sequences
    #elif defined (__APPLE__)
        system("clear");
    #endif
#endif
}

void PlaybackHandler::clrscr() {
    clrscrSafe();

    cin.sync();
}


int PlaybackHandler::inputTrackNum() {
    int trackNum;
    cout<<"Input Track Number: \n >>";
    cin>>trackNum;

    return std::clamp(trackNum, 1, (int) mTracks.size())-1;
}

string makeTime(size_t seconds) {
    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    int secondsConv = seconds % 60;

    return to_string(hours) + "H " + to_string(minutes) + "M " + to_string(secondsConv)+"s";
}


//Menu Stuff
//------------------------------------------------------------------------------------
void PlaybackHandler::StartCApp() {
    AudioIO::Init();
    mSaveConn = make_shared<SaveFileDB>();
    mSnapshotHandler = make_shared<SnapshotHandler>();
    mSnapshotHandler->mSaveConn = mSaveConn;
    mSnapshotHandler->Init();
    mMidiIO = make_shared<MidiIO>(shared_from_this());

    mAudioIO = AudioIO::Get();
    Pa_Initialize();
    mHostApi = Pa_GetDefaultHostApi();
    mAudioInDev = Pa_GetDefaultInputDevice();
    mNumInputs = Pa_GetDeviceInfo(mAudioInDev)->maxInputChannels;
    mAudioOutDev = Pa_GetDefaultOutputDevice();
    mNumOutputs = Pa_GetDeviceInfo(mAudioOutDev)->maxOutputChannels;
    mRate = Pa_GetDeviceInfo(mAudioInDev)->defaultSampleRate;
    updateSRates();

    clrscr();
    cout<<"Welcome to the SoundCheckr recording software. \n At the moment the ui is purely console based, but we are working on a physical UI"<<endl;

    waitForKeyPress();

    MenuAPP();
}

void PlaybackHandler::MenuAPP() {
    int input;
    bool loop = true;
    while (loop) {
        clrscr();
        cout<<"MENU: \n"
              "1 Playback \n"
              "2 Track modifications \n"
              "3 Audio Settings \n"
              "4 Save \n"
              "5 Snapshots \n"
              "6 Midi \n"
              "0 Exit \n"
              ">>";

        cin>>input;

        switch (input) {
            case 1: {
                PlaybackMenu();
            } break;
            case 2: {
                TracksMenu();
            } break;
            case 3: {
                AudioSettingsMenu();
            } break;
            case 4: {
                SaveMenu();
            } break;
            case 5: {
                SnapshotMenu();
            } break;
            case 6: {
                midiMenu();
            } break;
            case 0: {
                if (mUnSaved) {
                    cout<<"Current session is unsaved\n Would you like to save session? ";
                    if (YNConfirm()) {
                        save();
                    }
                }
                cout<<"Are you sure you want to exit? ";
                if (YNConfirm()) {
                    loop = false;
                }
            } break;
            default: {
                cout<<"Not supported ATM"<<endl;
                waitForKeyPress();
            } break;
        }
    }
}

void PlaybackHandler::PlaybackMenu() {
    int input;
    bool loop = true;
    while (loop) {
        clrscr();
        cout<<"Playback MENU: \n"
              "1 View Tracks \n"
              "2 Record \n"
              "3 Playback \n"
              "0 Back \n"
              ">>";
        cin>>input;

        switch (input) {
            case 1: {
                ViewTracks();
            } break;
            case 2: {
                if (Record()) {
                    RecordMenu();
                    string recordingStr = makeTime(mTracks[0]->getLengthS());
                    cout<<"successfully recorded "<<recordingStr<<endl;
                    waitForKeyPress();
                }
            }break;
            case 3: {
                if (Play()) {
                    PlayMenu();
                }
            } break;
            case 0: {
                loop = false;
            } break;
            default: {
                cout<<"Not supported ATM"<<endl;
                waitForKeyPress();
            } break;
        }
    }
}

void PlaybackHandler::RecordMenu() {
    int input;
    bool loop = true;
    mStopUiThread.store(false, memory_order::release);
    mUiThread = std::thread([this] { RecordUIThread(); });
    while (loop) {
        cin.sync();
        cin>>input;
        switch (input) {
            case 1: {
                mAudioIO->togglePause();
            } break;
            case 2: {
                mStopUiThread.store(true, memory_order::relaxed);
                endRecording();
                loop = false;
                mSnapshotHandler->setCurrentSnapshot(0);
            } break;
            case 3: {
                snapshotAction(mSnapshotHandler->getCurrentSnapshot()+1);
            } break;
            default: {
                cout<<"Invalid Input"<<endl;
            }
        }
    }
}

void PlaybackHandler::RecordUIThread() {
    while (!mStopUiThread.load(memory_order::acquire)) {
        clrscrSafe();
        if (mAudioIO->isPaused()) {
            cout<<"Recording Paused (" << makeTime(mAudioIO->getRecordingTime()) << ")\n"
                "1 UnPause Recording \n"
                "2 End Recording \n"
                "3 Create Snapshot \n"
                ">>";

        } else {
            cout<<"Recording (" << makeTime(mAudioIO->getRecordingTime()) << ")\n"
                "1 Pause Recording \n"
                "2 End Recording \n"
                "3 Create Snapshot \n"
                ">>";
        }

        using namespace chrono;
        std::this_thread::sleep_for(1s);
    }
}
void PlaybackHandler::RecordMidiUI() {
    while (!mStopUiThread.load(memory_order::acquire)) {
        clrscrSafe();
        if (mAudioIO->isPaused()) {
            cout<<"Recording Paused (" << makeTime(mAudioIO->getRecordingTime()) << ")\n";
            cout<<"Snapshot: "<<mSnapshotHandler->getCurrentSnapshot()<<endl;
        } else {
            cout<<"Recording (" << makeTime(mAudioIO->getRecordingTime()) << ")\n";
            cout<<"Snapshot: "<<mSnapshotHandler->getCurrentSnapshot()<<endl;
        }

        using namespace chrono;
        std::this_thread::sleep_for(1s);
    }
}


void PlaybackHandler::PlayMenu() {
    int input;
    bool loop = true;
    mStopUiThread.store(false, memory_order::release);
    mUiThread = std::thread([this] { PlayUIThread(); });
    while (loop) {
        cin.sync();
        cin>>input;

        switch (input) {
            case 1: {
                mAudioIO->togglePause();
            } break;
            case 2: {
                mStopUiThread.store(true, memory_order::release);
                stopPlayback();
                loop = false;
            } break;

            case 3: {
                if (mAudioIO->isPaused()) {
                    if (goToSnapshot()) {
                        waitForKeyPress();
                        mAudioIO->togglePause();
                    } else {
                        waitForKeyPress();
                    }
                }
            }break;

            case 10:
            case 15:
            case 30:
            case -10:
            case -15:
            case -30:{
                mAudioIO->doSeek(input);
            } break;
            default: {
                cout<<"Invalid Input"<<endl;
            }
        }
    }
}

void PlaybackHandler::PlayUIThread() {
    while (!mStopUiThread.load(memory_order::acquire)) {
        clrscrSafe();
        if (mAudioIO->isPaused()) {
            cout<<"Playback Paused ( " << makeTime(mAudioIO->getCurrentPlaybackTime())<<" \\ " << makeTime(mTracks[0]->getLengthS()) << ")\n"
                "1 UnPause Playback \n"
                "2 Stop Playback \n"
                "3 Go to Snapshot\n"
                "(-/+) (10,15,30) move playback by inputted distance\n"
                ">>";

        } else {
            cout<<"Playing Back Audio ( "<< makeTime(mAudioIO->getCurrentPlaybackTime())<<" \\ " << makeTime(mTracks[0]->getLengthS()) << ")\n"
                "1 Pause Playback \n"
                "2 Stop Playback \n"
                "(-/+) (10,15,30) move playback by inputted distance \n"
                ">>";
        }

        using namespace chrono;
        std::this_thread::sleep_for(1s);
    }
}

void PlaybackHandler::PlayMidiUI() {
    while (!mStopUiThread.load(memory_order::acquire)) {
        clrscrSafe();
        if (mAudioIO->isPaused()) {
            cout<<"Playback Paused ( " << makeTime(mAudioIO->getCurrentPlaybackTime())<<" \\ " << makeTime(mTracks[0]->getLengthS()) << ")\n";
            cout<<"Snapshot: "<<mSnapshotHandler->getCurrentSnapshot()<<endl;
        } else {
            cout<<"Playing Back Audio ( "<< makeTime(mAudioIO->getCurrentPlaybackTime())<<" \\ " << makeTime(mTracks[0]->getLengthS()) << ")\n";
            cout<<"Snapshot: "<<mSnapshotHandler->getCurrentSnapshot()<<endl;
        }

        using namespace chrono;
        std::this_thread::sleep_for(1s);
    }
}



void PlaybackHandler::TracksMenu() {
    int input;
    bool loop = true;
    while (loop) {
        clrscr();
        cout<<"Tracks MENU: \n"
              "1 View Tracks \n"
              "2 Create A New Track \n"
              "3 Remove A Track \n"
              "4 Change Input Channels on a Track \n"
              "5 Change Output Channels on a Track \n"
              "6 Change Track Type \n"
              "0 Back \n"
              ">>";
        cin>>input;

        switch (input) {
            case 1: {
                ViewTracks();
            } break;
            case 2: {
                int newTracks;
                cout<<"Amount of new tracks you would like to create: \n(Current audio device supports "<< std::min(mNumInputs, mNumOutputs) <<" IO Channels)\n>>";
                cin>>newTracks;
                mUnSaved = true;
                for (int i = 0; i < newTracks; ++i) {
                    newTrack();
                }
                waitForKeyPress();
            } break;
            case 3: {
                int TrackNdx = inputTrackNum();
                mUnSaved = true;
                removeTrack(TrackNdx);
            } break;
            case 4: {
                 if (mTracks.size() > 0) {
                    int TrackNDX = inputTrackNum();
                    int channelNum;
                    cout<<"Enter the new input Channel: (Max: "<<mNumInputs<<") \n>>";
                    cin>>channelNum;
                     mUnSaved = true;
                    changeInChannels(TrackNDX, channelNum-1);
                } else {
                    cout<<"No Tracks have been created, please create a track first"<<endl;
                }
            } break;
            case 5: {
                if (mTracks.size() > 0) {
                    int TrackNDX = inputTrackNum();
                    int channelNum;
                    cout<<"Enter the new output Channel: (Max: "<<mNumOutputs<<") \n>>";
                    cin>>channelNum;
                    mUnSaved = true;
                    changeOutChannels(TrackNDX, channelNum-1);
                }
                else {
                    cout<<"No Tracks have been created, please create a track first"<<endl;
                }
            } break;
            case 6: {
                int TrackNDX = inputTrackNum();
                int type;
                AudioGraph::ChannelType trackType;
                cout<<"Enter the New track type: \n"
                      "1 Mono \n"
                      "2 Sterio\n"
                      ">>";
                cin>>type;
                if (type ==1) {
                    trackType = AudioGraph::MonoChannel;
                    changeTrackType(TrackNDX, trackType);
                } else if (type == 2){
                    trackType = AudioGraph::SterioChannel;
                    changeTrackType(TrackNDX, trackType);
                } else {
                    cout<<"Invalid track type"<<endl;
                    waitForKeyPress();
                }
            } break;
            case 0: {
                loop = false;
            } break;
            default: {
                cout<<"Not supported ATM"<<endl;
                waitForKeyPress();
            } break;
        }
    }
}

void PlaybackHandler::AudioSettingsMenu() {
    int input;
    bool loop = true;
    while (loop) {
        clrscr();
        cout<<"Audio Settings: \n"
              "1 Change Host API \n"
              "2 Change Input Device \n"
              "3 Change Output Device \n"
              "4 Change Sample Rate \n"
              "0 Back \n"
              ">>";
        cin>>input;

        switch (input) {
            case 1: {
                mUnSaved = true;
                changeAudioAPI();
            } break;
            case 2: {
                mUnSaved = true;
                changeAudioInDev();
            } break;
            case 3: {
                mUnSaved = true;
                changeAudioOutDev();
            } break;
            case 4: {
                if (!mTracks.empty()) {
                    if (mTracks[0]->getLengthS() > 0) {
                        cout<<"Unable to change sample rate of project after data has been recorded"<<endl;
                        break;
                    }
                }
                mUnSaved = true;
                changeSRate();
                waitForKeyPress();
            } break;
            case 0: {
                loop = false;
            } break;
            default: {
                cout<<"Not supported ATM"<<endl;
                waitForKeyPress();
            } break;
        }
    }
}

void PlaybackHandler::midiMenu() {
    int input;
    bool loop = true;
    while (loop) {
        clrscr();
        cout<<"MIDI Settings: \n"
              "1 Change Input Device \n"
              "2 Test MIDI \n"
              "3 Enter Board Control \n"
              "0 Back \n"
              ">>";
        cin>>input;

        switch (input) {
            case 1: {
                mMidiIO->midiInputSelector();
                waitForKeyPress();
            } break;
            case 2: {
               mMidiIO->testMidiConn();
                waitForKeyPress();
                mMidiIO->endTest();
            } break;
            case 3: {
                mMidiIO->startMidiMonitering();
                midiHandler();
                mMidiIO->exitMidiMonitering();
            } break;
            case 0: {
                loop = false;
            } break;
            default: {
                cout<<"Not supported ATM"<<endl;
                waitForKeyPress();
            } break;
        }
    }
}


void PlaybackHandler::SaveMenu() {
    int input;
    bool loop = true;
    while (loop) {
        clrscr();
        cout<<"Save Menu: \n"
              "1 Save \n"
              "2 New Session \n"
              "3 Load Session \n"
              "4 New Show \n"
              "0 Back \n"
              ">>";
        cin>>input;

        switch (input) {
            case 1: {
                if (mSaveConn->DB()) {
                    save();
                    waitForKeyPress();
                } else {
                    newSave();
                }
            } break;
            case 2: {
                newSave();
            } break;
            case 3: {
                if (mUnSaved) {
                    cout<<"Current session is unsaved\n Would you like to save session? ";
                    if (YNConfirm()) {
                        save();
                    }
                }
                load();
            } break;
            case 4: {
                if (mUnSaved) {
                    cout<<"Current session is unsaved\n Would you like to save session? ";
                    if (YNConfirm()) {
                        save();
                    }
                }
                newShow();
            }
            case 0: {
                loop = false;
            } break;
            default: {
                cout<<"Not supported ATM"<<endl;
                waitForKeyPress();
            } break;
        }
    }
}

void PlaybackHandler::SnapshotMenu() {
    int input;
    bool loop = true;
    while (loop) {
        clrscr();
        cout<<"Snapshot Menu: \n"
              "1 View Snapshots \n"
              "2 change snapshots name \n"
              "0 Back \n"
              ">>";
        cin>>input;

        switch (input) {
            case 1: {
                viewSnapshots();
                waitForKeyPress();
            } break;
            case 2: {
                changeSnapshotsName();
                waitForKeyPress();
            } break;
            case 0: {
                loop = false;
            } break;
            default: {
                cout<<"Not supported ATM"<<endl;
                waitForKeyPress();
            } break;
        }
    }
}



//SAVING STUFF
//------------------------------------------------------------------------------------

std::string PlaybackHandler::buildFileName() {
    auto now = std::time(0);
    tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);

    strftime(buf, sizeof(buf), "%Y-%m-%d.%H-%M", &tstruct);

    string fileName = "./tmp/";
    mkdir(fileName.c_str());
    fileName += "Unsaved Session ";
    fileName += buf;
    fileName += ".SCRUnsaved";

    return fileName;
}

void PlaybackHandler::createAudioTempDB() {
    AudioIO::sAudioDB->open(buildFileName(), true);
    AudioIO::sAudioDB->setTemp(true);


    const char * sql = "CREATE TABLE IF NOT EXISTS sampleBlocks ( "
                       "blockID INTEGER PRIMARY KEY, "
                       "sampleformat INTEGER, "
                       "summin REAL, "
                       "summax REAL, "
                       "sumrms REAL, "
                       "samples BLOB,"
                       "summary256 BLOB,"
                       "summary64k BLOB);";

    char* errmsg = nullptr;

    int rc = sqlite3_exec(AudioIO::sAudioDB->DB(), sql, nullptr, nullptr, &errmsg);
    if (rc) {
        std::cout<<errmsg<<std::endl;
        throw;
    }
}

void PlaybackHandler::newSave() {
    wxFileDialog saveFileDialog(nullptr, _("Choose Save Location"), "","", "<insert name> session files (*.SCR) | *.SCR", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    if (saveFileDialog.ShowModal() == wxID_CANCEL) {
        cout<<"Canceled New Save"<<endl;
        waitForKeyPress();
        return;
    }
    if (mSaveConn->DB())
        mSaveConn->close();

    if (!mSaveConn->newSave(saveFileDialog.GetPath().c_str())) {
        cout<<"Failed to create a new save file in specified destination"<<endl;
    }


    if (AudioIO::sAudioDB->DB()) {
        copyAudioTempDBToMainSave();
    }
    AudioIO::sAudioDB->open(mSaveConn->GetSavePath());

    save();
}


void PlaybackHandler::save() {
    const char* sql = "DELETE FROM tracks;";
    if (sqlite3_exec(mSaveConn->DB(), sql, nullptr, nullptr, nullptr)!=SQLITE_OK) {
        std::cerr<<"Failed to truncate audio DB, "<<sqlite3_errmsg(mSaveConn->DB())<<std::endl;
        assert(false);
    }

    sql = "DELETE FROM snapshots;";
    if (sqlite3_exec(mSaveConn->DB(), sql, nullptr, nullptr, nullptr)!=SQLITE_OK) {
        std::cerr<<"Failed to truncate audio DB, "<<sqlite3_errmsg(mSaveConn->DB())<<std::endl;
        assert(false);
    }

    for (const auto& track : mTracks) {
        track->mSaveConn = mSaveConn;
        track->save();
    }
    mSnapshotHandler->save();

    sql = "DELETE FROM settings;";
    if (sqlite3_exec(mSaveConn->DB(), sql, nullptr, nullptr, nullptr)!=SQLITE_OK) {
        cerr<<"Failed to truncate audio DB, "<<sqlite3_errmsg(mSaveConn->DB())<<endl;
        assert(false);
    }

    auto stmt = mSaveConn->Prepare("INSERT INTO settings (hostAPI, inDev, outDev, sRate)"
                                       "                           VALUES(?1, ?2, ?3, ?4);");
    if (sqlite3_bind_int(stmt, 1, mHostApi) ||
        sqlite3_bind_int(stmt, 2, mAudioInDev) ||
        sqlite3_bind_int(stmt, 3, mAudioOutDev) ||
        sqlite3_bind_double(stmt, 4, mRate)) {
        wxASSERT(false);
        }

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        wxASSERT(false);
    }

    sqlite3_finalize(stmt);
    mUnSaved = false;

    cout<<"Saving complete"<<endl;
}

void PlaybackHandler::newShow() {
    if (mSaveConn->DB()) {
        wxFileDialog saveFileDialog(nullptr, _("Choose Save Location"), "","", "<insert name> session files (*.SCR) | *.SCR", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (saveFileDialog.ShowModal() == wxID_CANCEL) {
            cout<<"Canceled New Save"<<endl;
            waitForKeyPress();
            return;
        }
        if (mSaveConn->DB())
            mSaveConn->close();

        if (!mSaveConn->newSave(saveFileDialog.GetPath().c_str())) {
            cout<<"Failed to create a new save file in specified destination"<<endl;
        }

        AudioIO::sAudioDB->close();
        AudioIO::sAudioDB->open(mSaveConn->GetSavePath());

        for (auto track: mTracks) {
            track->mSaveConn = mSaveConn;
            track->newShow();
        }

        mSnapshotHandler->mSaveConn = mSaveConn;
        mSnapshotHandler->newShow();

        auto stmt = mSaveConn->Prepare("INSERT INTO settings (hostAPI, inDev, outDev, sRate)"
                                      "                           VALUES(?1, ?2, ?3, ?4);");
        if (sqlite3_bind_int(stmt, 1, mHostApi) ||
            sqlite3_bind_int(stmt, 2, mAudioInDev) ||
            sqlite3_bind_int(stmt, 3, mAudioOutDev) ||
            sqlite3_bind_double(stmt, 4, mRate)) {
            wxASSERT(false);
            }

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            wxASSERT(false);
        }

        sqlite3_finalize(stmt);
        mUnSaved = false;
    } else {
        cout<<"Unable to make a new show without an active save file, please save your current session first"<<endl;
    }
}


void PlaybackHandler::load(int) {
    wxFileDialog saveFileDialog(nullptr, _("Choose Session File"), "","", "<insert name> session files (*.SCR) | *.SCR", wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    if (saveFileDialog.ShowModal() == wxID_CANCEL) {
        cout<<"Canceled opening Save"<<endl;
        waitForKeyPress();
        return;
    }

    if (mSaveConn->DB())
        mSaveConn->close();
    if (AudioIO::sAudioDB->DB()) {
        AudioIO::sAudioDB->close();
    }

    if (mSaveConn->open(saveFileDialog.GetPath().c_str(), false)) {
        cout<<"Failed to open save file in specified destination"<<endl;
        return;
    }
    AudioIO::sAudioDB->open(mSaveConn->GetSavePath(), false);

    auto stmt = mSaveConn->Prepare("SELECT hostAPI, inDev, outDev, sRate FROM settings WHERE _ = 1;");

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        cerr<<"Failed to execute stmt"<<endl;
    }

    mHostApi = sqlite3_column_int(stmt, 0);
    mAudioInDev = sqlite3_column_int(stmt, 1);
    mAudioOutDev = sqlite3_column_int(stmt, 2);
    mRate = sqlite3_column_double(stmt, 3);

    sqlite3_finalize(stmt);

    stmt = mSaveConn->Prepare("SELECT Count(*) FROM tracks;");

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        cerr<<"Failed to execute stmt, err: "<<sqlite3_errmsg(mSaveConn->DB())<<endl;
    }

    int numTracks = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    mTracks.clear();
    mTracks.resize(numTracks);
    for (int i = 0; i < numTracks; ++i) {
        mTracks[i] = make_shared<Track>(mRate, floatSample);
        mTracks[i]->mSaveConn = mSaveConn;
        mTracks[i]->load(i+1);
    }
    mSnapshotHandler = make_shared<SnapshotHandler>();
    mSnapshotHandler->mSaveConn = mSaveConn;
    mSnapshotHandler->load();

    mUnSaved = false;

    cout<<"loading complete"<<endl;
    waitForKeyPress();


}


void PlaybackHandler::copyAudioTempDBToMainSave() const {
    auto tmpDB = AudioIO::sAudioDB->DB();
    auto saveDB = mSaveConn->DB();

    const char* sql = "SELECT sampleformat, summin, summax, sumrms,"
        "                              samples, length(samples), summary256, length(summary256), summary64k, length(summary64k) FROM sampleBlocks;";
    sqlite3_stmt* stmt;


    if (sqlite3_prepare_v2(tmpDB, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        cerr<<"Failed to prepare statement 2, err: "<< sqlite3_errmsg(tmpDB)<<endl;
    }

    string insertQuery = "INSERT INTO sampleBlocks (sampleformat, summin, summax, sumrms,"
        "                              samples, summary256, summary64k)"
        "                              VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7);";

    sqlite3_stmt* insertStmt;

    if (sqlite3_prepare_v2(saveDB, insertQuery.c_str(), -1, &insertStmt, nullptr) != SQLITE_OK) {
        cerr<<"Failed to prepare insert statement, err: "<< sqlite3_errmsg(saveDB)<<endl;
        return;
    }

    while (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_bind_int(insertStmt, 1, sqlite3_column_int(stmt, 0));
        sqlite3_bind_double(insertStmt, 2, sqlite3_column_double(stmt, 1));
        sqlite3_bind_double(insertStmt, 3, sqlite3_column_double(stmt, 2));
        sqlite3_bind_double(insertStmt, 4, sqlite3_column_double(stmt, 3));
        sqlite3_bind_blob(insertStmt, 5, sqlite3_column_blob(stmt, 4), sqlite3_column_int(stmt, 5), SQLITE_TRANSIENT);
        sqlite3_bind_blob(insertStmt, 6, sqlite3_column_blob(stmt, 6), sqlite3_column_int(stmt, 7), SQLITE_TRANSIENT);
        sqlite3_bind_blob(insertStmt, 7, sqlite3_column_blob(stmt, 8), sqlite3_column_int(stmt, 9), SQLITE_TRANSIENT);

        if (sqlite3_step(insertStmt) != SQLITE_DONE) {
            cerr<<"Failed to execute insert statement, err: "<< sqlite3_errmsg(saveDB)<<endl;
            return;
        }

        sqlite3_clear_bindings(insertStmt);
        sqlite3_reset(insertStmt);
    }

    sqlite3_finalize(stmt);
    sqlite3_finalize(insertStmt);

    std::cout << "Table copied successfully!" << std::endl;

    AudioIO::sAudioDB->close();
}

//PLAYBACK STUFF
//------------------------------------------------------------------------------------
bool PlaybackHandler::Record() {
    if (mTracks.empty()) {
        cerr<<"No tracks availble, please create a track before trying to record"<<endl;
        waitForKeyPress();
        return false;
    }

    if (!mSaveFile && !AudioIO::sAudioDB->DB()) {
        createAudioTempDB();
    }
    recordingSequences captureSequences;
    captureSequences.assign(mTracks.begin(), mTracks.end());

    TransportSequence transports = {captureSequences, constPlayableSequences{}};
    audioIoStreamOptions options;
    options.InDev = mAudioInDev;
    options.mSampleRate = mRate;
    options.mCaptureChannels = 0;
    options.mPlaybackChannels = 0;

    for (int i = 0; i <mTracks.size(); ++i) {
        if (mTracks[i]->isValid()) {
            auto testVar = (unsigned int)(mTracks[i]->GetFirstChannelIN()+mTracks[i]->NChannels());
            options.mCaptureChannels = std::max(options.mCaptureChannels, testVar);
        }
    }

    bool done = mAudioIO->startStream(transports, 0,std::numeric_limits<double>::max(),options);
    mRecording.store(done, std::memory_order_release);
    return done;
}

void PlaybackHandler::endRecording() {
    mAudioIO->stopStream();
    if (mSaveConn->DB()) {
        save();
    }

    mStopUiThread.store(true, memory_order::release);
    mUiThread.detach();
    mRecording.store(false, std::memory_order_release);
}

bool PlaybackHandler::Play() {
    if (!mSaveFile && !AudioIO::sAudioDB->DB() || mTracks.empty() || mTracks[0]->getLengthS() == 0) {
        cout<<"Nothing to playback :("<<endl;
        waitForKeyPress();
        return false;
    }

    constPlayableSequences playbackSequences;
    playbackSequences.assign(mTracks.begin(), mTracks.end());

    TransportSequence transports = {recordingSequences{}, playbackSequences};
    audioIoStreamOptions options;
    options.InDev = mAudioInDev;
    options.OutDev = mAudioOutDev;
    options.mSampleRate = mRate;
    options.mCaptureChannels = 0;
    options.mPlaybackChannels = 0;
    options.mStartTime = 0;

    for (int i = 0; i <mTracks.size(); ++i) {
        if (mTracks[i]->isValid()) {
            auto testVar = (unsigned int)(mTracks[i]->GetFirstChannelOut()+mTracks[i]->NChannels());
            options.mPlaybackChannels = std::max(options.mPlaybackChannels, testVar);
        }
    }

    auto snapshot = mSnapshotHandler->getSnapshot(mSnapshotHandler->getCurrentSnapshot());
    options.mStartTime = snapshot.timestamp;

    bool done =  mAudioIO->startStream(transports, 0, mTracks[0]->getLengthS(),options);
    mPlaying.store(done, std::memory_order_release);

    return done;
}

void PlaybackHandler::stopPlayback() {
    mAudioIO->stopStream();

    mStopUiThread.store(true, memory_order::release);
    mUiThread.detach();
    mPlaying.store(false, std::memory_order_release);
}

void PlaybackHandler::midiHandler() {
    bool loop = true;
    clrscr();
    cout<<"Entered Midi Mode"<<endl;
    while (loop) {
        bool recording = mRecording.load(std::memory_order_relaxed);
        bool playback = mPlaying.load(std::memory_order_relaxed);
        int action = midiAction.load(std::memory_order_acquire);
        switch (action) {
            case mPlayPC:
                if (!playback) {
                    if (!recording) {
                        playback = Play();

                        if (playback) {
                            mStopUiThread.store(false, memory_order::release);
                            mUiThread = std::thread([this] { PlayMidiUI(); });
                        }
                    }
                } else {
                    mAudioIO->togglePause();
                }
            break;
            case mRecordPC:
                if (!recording) {
                    if (playback) {
                        stopPlayback();
                        using namespace chrono;
                        std::this_thread::sleep_for(1s);
                    }
                    recording = Record();

                    if (recording) {
                        mStopUiThread.store(false, memory_order::release);
                        mUiThread = std::thread([this] { RecordMidiUI(); });
                    }
                } else {
                    mAudioIO->togglePause();
                }
            break;

            case mPausePC:
                mAudioIO->togglePause();
            break;

            case mStopPC:
                if (playback) {
                    stopPlayback();
                    cout<<"Finished Playback"<<endl;
                } else if (recording) {
                    endRecording();
                    cout<<"Finished Recording"<<endl;
                } else {
                    loop = false;
                }
            break;

            case mJumpF30Sec:
                mAudioIO->doSeek(30);
            break;

            case mJumpB30Sec:
                mAudioIO->doSeek(-30);
            break;

            case mJumpF15Sec:
                mAudioIO->doSeek(15);
             break;

            case mJumpB15Sec:
                mAudioIO->doSeek(-15);
            break;
            case mBackToSnapshot:
                snapshotAction(mSnapshotHandler->getCurrentSnapshot());
            break;
        }
        if (action>=0) {
            midiAction.store(-1, memory_order::release);
        }
    }
}

void PlaybackHandler::snapshotAction(snapshotMidi md) {
    bool recording = mRecording.load(std::memory_order_acquire);
    bool playback = mPlaying.load(std::memory_order_acquire);
    bool pause = mAudioIO->isPaused();

    if (recording) {
        mSnapshotHandler->setCurrentSnapshot(mSnapshotHandler->newSnapshot(md, mTracks[0]->getLengthS()));
    } else if (playback) {
        auto s = mSnapshotHandler->getSnapshot(md);
        if (s.number != -1) {
            mAudioIO->jumpToTime(s.timestamp);
            mSnapshotHandler->setCurrentSnapshot(s.number);
        }
    } else {
        auto s = mSnapshotHandler->getSnapshot(md);
        if (s.number != -1) {
            mSnapshotHandler->setCurrentSnapshot(s.number);
        }
    }
}

void PlaybackHandler::snapshotAction(int k) {
    bool recording = mRecording.load(std::memory_order_acquire);
    bool playback = mPlaying.load(std::memory_order_acquire);
    bool pause = mAudioIO->isPaused();

    if (recording) {
        mSnapshotHandler->newSnapshot(mAudioIO->getRecordingTime());
        mSnapshotHandler->setCurrentSnapshot(k);
    } else if (playback) {
        auto s = mSnapshotHandler->getSnapshot(k);
        mAudioIO->jumpToTime(s.timestamp);
        mSnapshotHandler->setCurrentSnapshot(s.number);
    }
}

//Snapshot Stuff
void PlaybackHandler::viewSnapshots() {
    auto snapshots = mSnapshotHandler->getSnapshots();

    if (snapshots.size() >0) {
        cout<<"snapshots: "<<endl;
        for (auto snapshot: snapshots) {
            cout<<snapshot.number<<" - "<<snapshot.name<<"     "<<makeTime(snapshot.timestamp)<<endl;
        }
    } else {
        cout<<"no created snapshots"<<endl;
    }
}

void PlaybackHandler::changeSnapshotsName() {
    auto snapshotsSize = mSnapshotHandler->getSnapshots().size();
    if (snapshotsSize > 1) {
        int snapshotNum;
        cout<<"Input the snapshot number: \n>>";
        cin>>snapshotNum;

        if (snapshotNum<snapshotsSize) {
            string name;
            cout<<"Input the name of the new snapshot: \n>>";
            cin>>name;

            mSnapshotHandler->assignName(name, snapshotNum);
        } else {
            cout<<"Snapshot Num Out of range"<<endl;
        }
    } else {
        cout<<"No snapshots to rename"<<endl;
    }
}

bool PlaybackHandler::goToSnapshot() {
    auto snapshotsSize = mSnapshotHandler->getSnapshots().size();
    if (snapshotsSize > 0) {
        viewSnapshots();

        cout<<"Which snapshot would you like to go to? \n >>";
        int snapshotNum;
        cin>>snapshotNum;
        if (snapshotNum<snapshotsSize) {
            snapshotAction(snapshotNum);
        } else {
            cout<<"Snapshot Num Out of range"<<endl;
            return false;
        }
    } else {
        cout<<"No snapshots to go to"<<endl;
        return false;
    }
    return true;
}


//Audio IO Dev stuff
//------------------------------------------------------------------------------------
void PlaybackHandler::changeAudioInDev() {
    auto currentDevInfo = Pa_GetDeviceInfo(mAudioInDev);
    cout<<"Current Audio Device: "<<currentDevInfo->name<<endl;

    std::vector<PaDeviceIndex> inDevs = {};

    cout<<endl<<"Available Devices: "<<endl;
    for (int i = 0; i < Pa_GetDeviceCount(); ++i) {
        auto dev = Pa_GetDeviceInfo(i);
        if (dev->hostApi == mHostApi && dev->maxInputChannels>0) {
            inDevs.push_back(i);
        }
    }
    for (int i = 0; i < inDevs.size(); ++i) {
        auto dev = Pa_GetDeviceInfo(inDevs[i]);

        if (inDevs[i]== mAudioInDev) {
            cout<<">>  ";
        } else {
            cout<<"    ";
        }
        cout<<i+1<<"   "<<dev->name<<endl;
    }

    cout<<">> ";

    int newDevNdx;
    cin>>newDevNdx;
    newDevNdx--;
    if (newDevNdx < inDevs.size() && newDevNdx>=0) {
        mAudioInDev = inDevs[newDevNdx];
        cout<< "Audio Device has been changed to "<<Pa_GetDeviceInfo(mAudioInDev)->name<<endl;
        mNumInputs = Pa_GetDeviceInfo(mAudioInDev)->maxInputChannels;
        propagateDevChange();
        sRateDefault();
    } else {
        cout<< "Error setting audio device "<< newDevNdx <<" is out of range"<<endl;
    }

    waitForKeyPress();
}
void PlaybackHandler::changeAudioOutDev() {
    auto currentDevInfo = Pa_GetDeviceInfo(mAudioOutDev);
    cout<<"Current Audio Device: "<<currentDevInfo->name<<endl;

    std::vector<PaDeviceIndex> outDevs = {};

    cout<<endl<<"Available Devices: "<<endl;
    for (int i = 0; i < Pa_GetDeviceCount(); ++i) {
        auto dev = Pa_GetDeviceInfo(i);
        if (dev->hostApi == mHostApi && dev->maxOutputChannels>0) {
            outDevs.push_back(i);
        }
    }
    for (int i = 0; i < outDevs.size(); ++i) {
        auto dev = Pa_GetDeviceInfo(outDevs[i]);

        if (outDevs[i]== mAudioOutDev) {
            cout<<">>  ";
        } else {
            cout<<"    ";
        }
        cout<<i+1<<"   "<<dev->name<<endl;
    }

    cout<<">> ";

    int newDevNdx;
    cin>>newDevNdx;
    newDevNdx--;
    if (newDevNdx < outDevs.size() && newDevNdx>=0) {
        mAudioOutDev = outDevs[newDevNdx];
        mNumOutputs = Pa_GetDeviceInfo(mAudioOutDev)->maxOutputChannels;
        cout<< "Audio Device has been changed to "<<Pa_GetDeviceInfo(mAudioOutDev)->name<<endl;
        propagateDevChange();
        sRateDefault();
    } else {
        cout<< "Error setting audio device "<< newDevNdx <<" is out of range"<<endl;
    }

    waitForKeyPress();
}


void PlaybackHandler::changeAudioAPI() {
    cout<<"Current Audio API: "<< Pa_GetHostApiInfo(mHostApi)->name;

    cout<<endl<<"Available APIs: "<<endl;
    for (int i = 0; i < Pa_GetHostApiCount(); ++i) {
        auto api = Pa_GetHostApiInfo(i);

        if (i == mHostApi) {
            cout<<">>  ";
        } else {
            cout<<"    ";
        }
        cout<<i+1<<"   "<<api->name<<endl;
    }
    cout<<">>";

    int newDevNdx;
    cin>>newDevNdx;
    newDevNdx--;
    if (newDevNdx < Pa_GetDeviceCount() && newDevNdx>=0) {
        mHostApi = newDevNdx;
        cout<< "Audio API has been changed to "<<Pa_GetHostApiInfo(mHostApi)->name<<endl;
        propagateDevChange();
        sRateDefault();
    } else {
        cout<< "Error setting audio API, "<< newDevNdx <<" is out of range"<<endl;
    }

    waitForKeyPress();
}


void PlaybackHandler::changeSRate() {
    cout<< "Current SRate: "<<mRate<<endl;

    std::vector<size_t> supportedRates;
    getSupportedRates(supportedRates);

    if (!supportedRates.size()) {
        cout<<"No Available Sample Rates Found"<<endl;
        waitForKeyPress();
    }

    cout<<endl<<"Available rates: "<<endl;
    for (int i = 0; i < supportedRates.size(); ++i) {
        auto rate = supportedRates[i];
        if (mRate == rate) {
            cout<<">>  ";
        } else {
            cout<<"    ";
        }
        cout<<i+1<< "   " <<rate<<endl;
    }

    cout<<">>";
    int rateNdx;
    cin>>rateNdx;
    rateNdx--;

    if (rateNdx < supportedRates.size()) {
        mRate = supportedRates[rateNdx];
        cout<<"Sample rate has been set to "<<supportedRates[rateNdx]<<endl;
        updateSRates();
    } else {
        cout<< "Error setting SRate " <<rateNdx<< " is out of range";
    }

    waitForKeyPress();
}
void PlaybackHandler::getSupportedRates(std::vector<size_t> &rates) {
    std::vector<size_t> possibleRates = {32000, 44100, 48000, 88200, 96000, 176400, 192000};

    PaStreamParameters outPars;

    outPars.device = mAudioOutDev;
    outPars.channelCount = 1;
    outPars.sampleFormat = paFloat32;
    outPars.suggestedLatency = Pa_GetDeviceInfo(mAudioOutDev)->defaultHighOutputLatency;
    outPars.hostApiSpecificStreamInfo = NULL;

    PaStreamParameters inPars;

    inPars.device = mAudioInDev;
    inPars.channelCount = mNumInputs;
    inPars.sampleFormat = paFloat32;
    inPars.suggestedLatency = Pa_GetDeviceInfo(mAudioInDev)->defaultHighInputLatency;
    inPars.hostApiSpecificStreamInfo = NULL;

    for (auto rate: possibleRates) {
        if (Pa_IsFormatSupported(&inPars, &outPars, rate) == 0) {
            rates.push_back(rate);
        }
    }
}

void PlaybackHandler::updateSRates() {
    for (auto track: mTracks) {
        track->setRate(mRate);
    }
}


void PlaybackHandler::sRateDefault() {
    auto dev = Pa_GetDeviceInfo(mAudioInDev);

    mRate = dev->defaultSampleRate;

    updateSRates();
}



//Track Stuff
//------------------------------------------------------------------------------------
void PlaybackHandler::ViewTracks() {
    clrscr();
    if (mTracks.size() > 0) {
        cout<<"Tracks: "<<endl;
        cout<<"Track#   InChannel   OutChannel   Track Type   Valid Track"<<endl;
        for (int i = 0; i <  mTracks.size(); ++i) {
            auto track = mTracks[i];
            cout<<"#"<<i+1<<"      "<<track->GetFirstChannelIN()+1<<"          "<<track->GetFirstChannelOut()+1<<"           "<<(track->getChannelType() ? "Stereo" :" Mono ")
                << "         "<<(track->isValid()? "Valid": "Invalid")<<endl;
        }
    } else {
        cout<<"No Tracks have been created"<<endl;
    }

    waitForKeyPress();
}

bool PlaybackHandler::newTrack() {
    auto newTrack = std::make_shared<Track>(mRate, floatSample);
    auto newTrackNdx = mTracks.size();
    mTracks.resize(mTracks.size()+1);
    mTracks[newTrackNdx] = newTrack;

    if (!attemptPopulateAutoIOTrack(newTrackNdx)) {
        cout<<"Input Output channels not set automatically for track number: " << newTrackNdx+1<<", please set manually"<< endl;

        return false;
    }

    cout<<"successfully Created Track"<<endl;

    return true;
}

void PlaybackHandler::removeTrack(int trackNdx) {
    auto trackToBeRemoved = mTracks[trackNdx];
    cout<<"Are you sure you want to delete track # "<<trackNdx+1;
    if (YNConfirm()) {
        mTracks.erase(mTracks.begin()+trackNdx);
        cout<<"Track successfully deleted ";
    }

    waitForKeyPress();
}


bool PlaybackHandler::changeTrackType(int trackNdx, AudioGraph::ChannelType type) {
    auto track = mTracks[trackNdx];
    if ((track->GetFirstChannelIN() +1 < mNumInputs && track->GetFirstChannelOut() +1 < mNumOutputs && type == AudioGraph::SterioChannel)||type == AudioGraph::MonoChannel)  {
        mUnSaved = true;

        track->changeTrackType(type);

        cout<<"Track has been changed to have "<<type+1<< " channels"<<endl;

        cout<<"Do you want to automatically reassign IO channels for all tracks after? ";

        if (YNConfirm()) {
            cout<<"Attempting to repopulate all tracks after track num "<<trackNdx+1 << endl;
            propagateDevChange(trackNdx);
        } else {
            for (int i = 0; i < mTracks.size(); ++i) {
                if (i!= trackNdx) {
                    auto t= mTracks[i];
                    int channelNum = track->GetFirstChannelOut();
                    if ((t->GetFirstChannelOut() == channelNum || t->GetFirstChannelOut()+t->NChannels()-1 == channelNum)||t->GetFirstChannelOut() == channelNum+ track->NChannels()-1) {
                        t->changeOutChannel(-1);
                        cout<<"Output channel overlaps with that of track # " << i+1 << " please manually reassign the output channel for that track"<<endl;
                    }
                }
            }
        }
    } else {
        cout<<"Unable to make this track stereo, there are not enough channels available on selected audio device"<<endl;
    }

    waitForKeyPress();
    return true;
}

bool PlaybackHandler::changeInChannels(int trackNdx, int channelNum) {
    auto track = mTracks[trackNdx];

    auto maxChannels = mNumInputs;
    if (channelNum < maxChannels && channelNum >=0) {
        track->changeInChannel(channelNum);

        cout<<"Track In channel num has been changed to "<< channelNum+1<<endl;
        if (!attemptPopulateOut(trackNdx, false)) {
            cout<<"Automatic setting of output failed, please set manually"<<endl;
        }

        cout<<"Do you want to automatically reassign IO channels for all tracks after? ";

        if (YNConfirm()) {
            cout<<"Attempting to repopulate all tracks after track num "<<trackNdx+1 << endl;
            propagateDevChange(trackNdx);
        }
    } else {
        cout<<"Channel num out of range for current audio device, please select a different audio device or a lower channel num"<<endl;
        cout<<"Max Input channels for current device: "<<maxChannels<<endl;
        waitForKeyPress();
        return false;
    }

    waitForKeyPress();
    return true;
}

bool PlaybackHandler::changeOutChannels(int trackNdx, int channelNum) {
    auto track = mTracks[trackNdx];

    auto maxChannels = mNumOutputs;

    if (channelNum >=0 && channelNum < maxChannels) {
        for (int i = 0; i < trackNdx; ++i) {
            if (mTracks[i]->GetFirstChannelOut() == channelNum || mTracks[i]->GetFirstChannelOut()+mTracks[i]->NChannels()-1 == channelNum) {
                cout<<"The output channel " <<channelNum<< "is already in use, please reassign Track Num " << i +1 << "before changing to this output channel"<<endl;

                waitForKeyPress();
                return false;
            }
        }

        track->changeOutChannel(channelNum);
        cout<<"Track Out channel num has been changed to "<< channelNum+1 <<endl;

        cout<<"Do you want to automatically reassign IO channels for all tracks after? ";
        if (YNConfirm()) {
            cout<<"Attempting to repopulate all tracks after track num "<<trackNdx+1 << endl;
            propagateDevChange(trackNdx);
        } else {
            for (int i = trackNdx +1; i < mTracks.size(); ++i) {
                auto t = mTracks[i];
                if ((t->GetFirstChannelOut() == channelNum || t->GetFirstChannelOut()+t->NChannels()-1 == channelNum)||t->GetFirstChannelOut() == channelNum+ track->NChannels()-1) {
                    t->changeOutChannel(-1);
                    cout<<"Output channel overlaps with that of track # " << i+1 << " please manually reassign the output channel for that track"<<endl;
                }
            }
        }
    } else {
        cout<<"Channel num out of range for current audio device, please select a different audio device or a lower channel num"<<endl;
        cout<<"Max Output channels for current device: "<<maxChannels<<endl;
        waitForKeyPress();
        return false;
    }
    waitForKeyPress();

    return true;
}

int PlaybackHandler::propagateDevChange(int trackIndex) {
    for (int i = trackIndex+1; i < mTracks.size(); ++i) {
        if (!attemptPopulateAutoIOTrack(i, true)) {
            cout<<"Automatically populated tracks up to num "<<i+1<< "Please set the rest manually"<<endl;
            return i;
        }
    }
    cout<< "Successfully Populated tracks"<<endl;
    return -1;
}

bool PlaybackHandler::attemptPopulateAutoIOTrack(int trackIndex, bool fullReset) {
    if (attemptPopulateOut(trackIndex, fullReset))
        if (attemptPopulateIn(trackIndex))
            return true;
    return false;
}

bool PlaybackHandler::attemptPopulateIn(int trackNdx) {
    auto newTrack = mTracks[trackNdx];
    auto maxChannels = mNumInputs;

    size_t channelNum = 0;

    if (trackNdx > 0) {
        auto lastTrack = mTracks[trackNdx-1];
        channelNum = lastTrack->GetFirstChannelOut() + lastTrack->NChannels();
    }

    if (channelNum < maxChannels) {
        newTrack->changeInChannel(channelNum);
        return true;
    }
    return false;
}

bool PlaybackHandler::attemptPopulateOut(int trackNdx, bool fullReset) {
    auto newTrack = mTracks[trackNdx];
    auto maxChannels = mNumOutputs;

    size_t channelNum = 0;

    if (trackNdx > 0) {
        auto lastTrack = mTracks[trackNdx-1];
        channelNum = lastTrack->GetFirstChannelOut() + lastTrack->NChannels();
    }
    if (channelNum < maxChannels) {
        for (int i = 0; i < trackNdx-1; ++i) {
            auto track = mTracks[i];
            if (track->GetFirstChannelOut() == channelNum || track->GetFirstChannelOut()+track->NChannels()-1 == channelNum) {
                newTrack->changeOutChannel(-1);
                newTrack->changeInChannel(-1);
                return false;
            }
        }
        newTrack->changeOutChannel(channelNum);

        for (int i = trackNdx +1; i < mTracks.size(); ++i) {
            auto t = mTracks[i];
            if (((t->GetFirstChannelOut() == channelNum || t->GetFirstChannelOut()+t->NChannels()-1 == channelNum)||t->GetFirstChannelOut() == channelNum+ newTrack->NChannels()-1) && !fullReset) {
                t->changeOutChannel(-1);
                cout<<"Output channel overlaps with that of track # " << i+1 << " please manually reassign the output channel for that track"<<endl;
            }
        }
    } else {
        newTrack->changeInChannel(-1);
        newTrack->changeOutChannel(-1);
        return false;
    }

    return true;
}