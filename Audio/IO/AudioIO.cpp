//
// Created by cline on 2025-01-28.
//

#include "AudioIO.h"

#include "../../MemoryManagement/Math/float_cast.h"
#include <cstdio>
#include <iostream>
#include <wx/debug.h>

std::shared_ptr<DBConnection> AudioIOBase::sAudioDB;

AudioIOStream::AudioIOStream(PaError &err, PaStreamParameters inputParams, PaStreamParameters outputParams, int srate, PaStreamCallback CallbackFXN) {
    err = initializeAudioStream(inputParams,outputParams,srate,CallbackFXN);
}

PaError AudioIOStream::initializeAudioStream(PaStreamParameters inputParams, PaStreamParameters outputParams, int srate, PaStreamCallback CallbackFXN) {
    static testUserData data;

    PaError err = Pa_OpenStream(
        &mStream,
        &inputParams,
        &outputParams,
        srate,
        paFramesPerBufferUnspecified,
        paNoFlag,
        CallbackFXN,
        &data);

    // PaError err = Pa_OpenDefaultStream(
    //      &stream,
    //      numCaptureChannel,
    //      numPlaybackChannel,
    //      paFloat32,
    //      44100,
    //      256,
    //      CallbackFXN,
    //      &data
    //  );


    printf("Pa_OpenDefaultStream returned %d\n", err);
    if (err != paNoError) printf("Error opening audio stream\n");

    return err;
}


void AudioIOStream::startStream() {

    Pa_StartStream(mStream);
    printf("Stream started\n");
}

void AudioIOStream::endStream() {
    Pa_StopStream(mStream);
    printf("Stream ended\n");
}

int AudioCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData) {
    /* Cast data passed through stream to our structure. */
    auto *data = (testUserData *)userData;
    auto *out = (float *)outputBuffer;
    (void) inputBuffer; /* Prevent unused variable warning. */

    return 0;
}


void AudioIOBase::initAudio() {
    sAudioDB = std::make_shared<DBConnection>();
    sAudioDB->open("test.audio");
    createDB();
}

void AudioIOBase::createDB() {
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

    int rc = sqlite3_exec(sAudioDB->DB(), sql, nullptr, nullptr, &errmsg);
    if (rc) {
        std::cout<<errmsg<<std::endl;
        throw;
    }
}

AudioIO::AudioIO() {
    if (!initPortAudio()) {
        //REPLACE WITH WARNING POPUP
        printf("Error initializing audio stream\n");
    }
}

AudioIO::~AudioIO() {

    //Handle Exiting IO

    //Kill Stream & callback
    if (!mAudioStream->getStreamStillRunning()) {
        mAudioStream->endStream();
    }


    Pa_Terminate();
    mAudioThread.join();

}

bool AudioIO::initPortAudio() {
    PaError err;

    err = Pa_Initialize();

    return (err == paNoError);
}

void AudioIO::startAudioThread() {
    mAudioThread = std::thread(&AudioIO::audioThread, std::ref(mKillAudioThread));
}

int AudioIO::startStream() {
    return 0;
}
void AudioIO::startStreamCleanup(bool bClearBuffersOnly /*=false*/) {
    mPlaybackBuffers.clear();
    mCaptureBuffers.clear();
    if (!bClearBuffersOnly) {
        //HANDLE ISSUES WITH STREAM HERE
    }
}


bool AudioIO::AllocateBuffers() {

    BufferTime times = getBufferTimes();
    auto playbackTime = lrint(times.batchSize.count() * mRate)/mRate;

    mPlaybackSamplesToCopy = playbackTime*mRate;

    mPlaybackBufferSecs = times.RingBufferDelay.count();

    mCaptureBufferSecs = 4.5 + 0.5 * std::min(size_t(16), mNumCaptureChannels);
    mMinCaptureBufferSecsToCopy = 0.2 + 0.2*std::min(size_t(16), mNumCaptureChannels);

    bool done;
    do {
        done = true;
        try {
            if (mNumPlaybackChannels>0) {
                auto bufferLength = std::max((size_t)lrint(mRate*mPlaybackBufferSecs), mHardwarePlaybackLatency*2);

                //make buffer length a multiple of the sample rate
                bufferLength = mPlaybackSamplesToCopy * (bufferLength+mPlaybackSamplesToCopy-1)/mPlaybackSamplesToCopy;

                //If we cant afford 100 samples crash out
                if (bufferLength<100||mPlaybackSamplesToCopy<100) {
                    //SHOW ERROR MESSAGE
                    wxASSERT(false);
                    return false;
                }

                mPlaybackBufferSecs = bufferLength/mRate;

                mPlaybackBuffers.resize(0);
                mPlaybackBuffers.resize(mNumPlaybackChannels);

                //Generate Buffers
                std::generate(
                    mPlaybackBuffers.begin(),
                    mPlaybackBuffers.end(),
                    [=] {return std::make_unique<audioBuffer>(floatSample, bufferLength); }
                );

            }
            if (mNumCaptureChannels>0) {
                auto bufferLength = (size_t)lrint(mRate*mCaptureBufferSecs);

                //If we cant afford 100 samples crash out
                if (bufferLength<100) {
                    //SHOW ERROR MESSAGE
                    wxASSERT(false);
                    return false;
                }

                mCaptureBuffers.resize(0);
                mCaptureBuffers.resize(mNumCaptureChannels);

                std::generate(
                    mCaptureBuffers.begin(),
                    mCaptureBuffers.end(),
                    [&] {return std::make_unique<audioBuffer>(mCaptureFormat, bufferLength); }
                );
            }

        } catch (std::bad_alloc&) {
            //Handling Out of memery error, shouldn't happen, therefore just clean everything up and try again
            done = false;

            startStreamCleanup(true);

            mPlaybackSamplesToCopy /=2;
            mPlaybackBufferSecs /=2;
            mCaptureBufferSecs /=2;
            mMinCaptureBufferSecsToCopy /=2;

        }
    } while (!done);

    return true;
}


