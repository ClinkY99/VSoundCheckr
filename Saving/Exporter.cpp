//
// Created by cline on 2025-04-22.
//

#include "Exporter.h"

#include <stack>
#include <wx/filedlg.h>
#include <wx/translation.h>
#include <wx/msw/filedlg.h>

#include "../Visual/PlaybackHandler.h"
#include "File Types/WavFile.h"

#define stackAllocate(T, count) static_cast<T*>(alloca(count * sizeof(T)))

using namespace std;

string makeTime(size_t seconds) {
    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    int secondsConv = seconds % 60;

    return to_string(hours) + "H " + to_string(minutes) + "M " + to_string(secondsConv)+"s";
}

void Exporter::showExportUI()
{
    int input;
    bool loop = true;

    while (loop)
    {
        PlaybackHandler::clrscr();
        cout<< "EXPORT MENU:\n"
               "1 export wav (timestamp mode) \n"
               "2 export wav (snapshot mode) \n"
               "0 return\n"
               ">> ";

        cin>>input;
        switch (input)
        {
            case 1: {
                exportTimestampsUI();
            } break;
            case 2:{
                exportSnapshotsUI();
            } break;
            case 0 :{
                loop = false;
            }break;
            default:{
                cout<<"Not supported ATM"<<endl;
                PlaybackHandler::waitForKeyPress();
            }
        }
    }
}

Tracks Exporter::getTracksToExport(){
    Tracks tracksToExport;

    PlaybackHandler::clrscr();
    cout<<"Which tracks do you want to export (-1 for all): \n>> ";

     auto range = get2Ints();

    if (range.first != -1)
    {
        int num1 = max(range.first,0);
        int num2 = min(range.second,(int) mTracks.size()-1);

        for (int i = num1; i < num2+1; ++i)
        {
            tracksToExport.push_back(mTracks[i]);
        }
    } else
    {
        tracksToExport = mTracks;
    }
    return tracksToExport;
}

void Exporter::exportSnapshotsUI()
{
    Tracks tracksToExport = getTracksToExport();

    PlaybackHandler::clrscr();

    cout<<"Please enter the range of snapshots that you would like to export (valid input from 1-"<< (const char) mSnapshots.size() << "):\nenter -1 for full track or enter a single number for one snapshot \n>> ";

    auto snapshotRange = get2Ints();

    sampleCount startLocation,endLocation;
    if (snapshotRange.first != -1)
    {
        int num1 = max(snapshotRange.first-1,0);
        int num2 = min(snapshotRange.second,(int) mSnapshots.size());

        startLocation = sampleCount(mSnapshots[num1].timestamp*tracksToExport[0]->GetRate());
        if (num2 == mSnapshots.size())
        {
            endLocation  = sampleCount(tracksToExport[0]->getLengthS()*tracksToExport[0]->GetRate());
        } else
        {
            endLocation = sampleCount(mSnapshots[num2].timestamp*tracksToExport[0]->GetRate());
        }
    }

    FilePath path;

    wxFileDialog exportFileDialog(nullptr, _("Choose Export Location"), "","", "Wav file (*.wav) | *.wav", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    if (exportFileDialog.ShowModal() == wxID_CANCEL) {
        cout<<"Canceled Export"<<endl;
        PlaybackHandler::waitForKeyPress();
        return;
    }

    if (tracksToExport.size() > 1)
    {
        path = exportFileDialog.GetPath();
        path.erase(path.size()-4);
    } else
    {
        path = exportFileDialog.GetPath();
    }

    exportWavSamples(path, tracksToExport, startLocation, endLocation);

}

void Exporter::exportTimestampsUI()
{
    Tracks tracksToExport = getTracksToExport();

    PlaybackHandler::clrscr();
    cout<<"Please enter the time range for exporting (track length: "+makeTime(mTracks[0]->getLengthS())+")\nenter -1 for full track length\n>>";

    auto timeRange = get2Times();
    sampleCount startLocation, endLocation;
    if (timeRange.first != -1){
        startLocation = sampleCount(max(0.0, timeRange.first*mTracks[0]->GetRate()));
        endLocation = sampleCount(min(timeRange.second*mTracks[0]->GetRate(), mTracks[0]->GetRate()*mTracks[0]->getLengthS()));
    } else {
        startLocation = 0;
        endLocation = sampleCount(mTracks[0]->GetRate()*mTracks[0]->getLengthS());
    }

    FilePath path;

    wxFileDialog exportFileDialog(nullptr, _("Choose Export Location"), "","", "Wav file (*.wav) | *.wav", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    if (exportFileDialog.ShowModal() == wxID_CANCEL) {
        cout<<"Canceled Export"<<endl;
        PlaybackHandler::waitForKeyPress();
        return;
    }

    if (tracksToExport.size() > 1)
    {
        path = exportFileDialog.GetPath();
        path.erase(path.size()-4);
    } else
    {
        path = exportFileDialog.GetPath();
    }

    exportWavSamples(path, tracksToExport, startLocation, endLocation);
}

std::pair<int, int> Exporter::get2Ints()
{
    string input;
    int num1, num2;
    char dash = '\0';

    cin>>input;

    stringstream ss(input);

    if (input.find('-')!=string::npos && input != "-1")
    {
        ss >> num1 >> dash >> num2;
    } else
    {
        ss >> num1;
        num2 = num1;
    }

    return {num1, num2};
}

std::pair<double, double> Exporter::get2Times()
{
    string input;
    double num1, num2;

    cin>>input;

    size_t dashPos;

    if ((dashPos = input.find("-")) != string::npos && input != "-1")
    {
        num1 = parseTime(input.substr(0, dashPos));
        num2 = parseTime(input.substr(dashPos+1, string::npos));
    } else
    {
        num1 = num2 = -1;
    }

    return {num1, num2};
}

size_t Exporter::parseTime(std::string time)
{
    int h =0, m = 0, s = 0;
    char colon;

    stringstream ss(time);

    int colonCount = count(time.begin(), time.end(), ':');

    if (colonCount == 2)
    {
        ss >> h >> colon >> m >> colon >> s;
    } else if (colonCount == 1)
    {
        ss >> m >> colon >> s;
    } else
    {
        ss >> s;
    }

    return h*3600 + m*60 + s;
}

void Exporter::exportWavSamples(FilePath path, Tracks tracks, sampleCount startLocation, sampleCount endLocation)
{
    if (tracks.size() == 1)
    {
        exportWav(path, tracks[0], startLocation, endLocation);
    }
    else
    {
        for (auto track : tracks)
        {
            FilePath newPath = path+" - ";
            newPath += track->getTrackNum();
            newPath += ".wav";

            exportWav(newPath, track, startLocation, endLocation);
        }
    }
}

void Exporter::exportWav(FilePath path, std::shared_ptr<Track> track, sampleCount startLocation, sampleCount endLocation)
{
    size_t samplesRemaining = (endLocation-startLocation).as_size_t();
    sampleCount location = startLocation;

    WavFile wavFile(track->GetRate(), track->NChannels(), track->GetSampleFormat(), (endLocation-startLocation).as_size_t());

    wavFile.openWavFile(path);

    size_t samplesPerBlock = mBlockSize/SAMPLE_SIZE(track->GetSampleFormat());

    //** MEMORY ALLOCATIONS **
    std::vector<std::vector<float>>tempBufs (track->NChannels(), std::vector<float>(samplesPerBlock));

    std::vector<float> outputBufs;
    outputBufs.resize(track->NChannels()*samplesPerBlock);
    //** END OF MEMORY ALLOCATIONS **

    while (samplesRemaining>0)
    {
        size_t samples = std::min(samplesRemaining, samplesPerBlock);

        for (size_t i=0; i<track->NChannels(); i++)
        {
            track->GetFloats(i, (samplePtr) tempBufs[i].data(), location, samples);

            for (size_t j=0; j<samples; j++)
            {
                outputBufs[track->NChannels()*j + i] = tempBufs[i][j];
            }
        }

        wavFile.writeSamples((constSamplePtr) outputBufs.data(), samples);

        samplesRemaining -= samples;
        location += samples;
    }

    wavFile.closeWavFile();
}