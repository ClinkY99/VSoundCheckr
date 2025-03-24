//
// Created by cline on 2025-03-21.
//

#include "PlaybackHandler.h"

#include <iostream>

using namespace std;

PlaybackHandler::~PlaybackHandler() {
    Pa_Terminate();
    AudioIO::DeInit();
}


//CMDL IO
//------------------------------------------------------------------------------------
void PlaybackHandler::waitForKeyPress() {
    cout << "Press any key to continue ....." << endl;
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

void PlaybackHandler::clrscr() {
// #if defined _WIN32
//     system("cls");
//     //clrscr(); // including header file : conio.h
// #elif defined (__LINUX__) || defined(__gnu_linux__) || defined(__linux__)
//     system("clear");
//     //std::cout<< u8"\033[2J\033[1;1H"; //Using ANSI Escape Sequences
// #elif defined (__APPLE__)
//     system("clear");
// #endif

    cin.sync();
}

int PlaybackHandler::inputTrackNum() {
    int trackNum;
    cout<<"Input Track Number: \n >>";
    cin>>trackNum;

    return std::clamp(trackNum, 1, (int) mTracks.size())-1;
}


//Menu Stuff
//------------------------------------------------------------------------------------
void PlaybackHandler::StartCApp() {
    AudioIO::Init();
    if (!saveFile && !AudioIO::sAudioDB) {
        createAudioTempDB();
    }
    mAudioIO = AudioIO::Get();
    Pa_Initialize();
    mHostApi = Pa_GetDefaultHostApi();
    mAudioInDev = Pa_GetDefaultInputDevice();
    mNumInputs = Pa_GetDeviceInfo(mAudioInDev)->maxInputChannels;
    mAudioOutDev = Pa_GetDefaultOutputDevice();
    mNumOutputs = Pa_GetDeviceInfo(mAudioOutDev)->maxOutputChannels;
    mRate = Pa_GetDeviceInfo(mAudioInDev)->defaultSampleRate;
    updateSRates();

    newTrack();

    clrscr();
    cout<<"Welcome to the <INSERT NAME HERE> recording software. \n At the moment the ui is purely console based, but we are workign on a physical UI"<<endl;

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
            case 0: {
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
                Record();
            }break;
            case 3: {

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
    while (loop) {
        clrscr();
        cout<<"(TMP) Recording: \n"
              "1 EndRecording \n"
              ">>";
        cin>>input;

        switch (input) {
            case 1: {
                endRecording();
                loop = false;
            } break;
            default: {
                cout<<"Invalid Input"<<endl;
                waitForKeyPress();
            }
        }
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
                cout<<"Amount of new tracks you would like to create: \n>>";
                cin>>newTracks;
                for (int i = 0; i < newTracks; ++i) {
                    newTrack();
                }
                waitForKeyPress();
            } break;
            case 3: {
                int TrackNdx = inputTrackNum();
                removeTrack(TrackNdx);
            } break;
            case 4: {
                 if (mTracks.size() > 0) {
                    int TrackNDX = inputTrackNum();
                    int channelNum;
                    cout<<"Enter the new input Channel: (Max: "<<mNumInputs<<") \n>>";
                    cin>>channelNum;
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
                changeAudioAPI();
            } break;
            case 2: {
                changeAudioInDev();
            } break;
            case 3: {
                changeAudioOutDev();
            } break;
            case 4: {
                changeSRate();
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

    string fileName = "../tmp/";
    mkdir(fileName.c_str());
    fileName += "Unsaved Session ";
    fileName += buf;
    fileName += ".audioUnsaved";

    return fileName;
}

void PlaybackHandler::createAudioTempDB() {
    AudioIO::sAudioDB = std::make_shared<DBConnection>();
    AudioIO::sAudioDB->open(buildFileName());


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


//PLAYBACK STUFF
//------------------------------------------------------------------------------------
void PlaybackHandler::Record() {
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

    if (mAudioIO->startStream(transports, 0,std::numeric_limits<double>::max(),options)) {
        RecordMenu();
    }
}

void PlaybackHandler::endRecording() {
    mAudioIO->stopStream();
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
    if (track->GetFirstChannelIN() +1 < mNumInputs && track->GetFirstChannelOut() +1 < mNumOutputs && type == AudioGraph::SterioChannel)  {
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




