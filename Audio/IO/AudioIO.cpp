/*
 * This file is part of VSoundCheckr
 * Copyright (C) 2025 Kieran Cline
 *
 * Licensed under the GNU General Public License v3.0
 * See LICENSE file for details.
 */

#include "AudioIO.h"

#include "../../MemoryManagement/Math/float_cast.h"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <numeric>
#include <wx/debug.h>
#include <wx/wxcrtvararg.h>

#include "../Dither.h"

#ifdef __WXMSW__
    #include <pa_win_wasapi.h>
#endif


std::shared_ptr<DBConnection> AudioIOBase::sAudioDB;
std::unique_ptr<AudioIOBase> AudioIOBase::ugAudioIO;


#define stackAllocate(T, count) static_cast<T*>(alloca(count * sizeof(T)))


AudioIOStream::AudioIOStream(PaError &err, PaStreamParameters *inputParams, PaStreamParameters *outputParams, int srate, PaStreamCallback CallbackFXN) {
    err = initializeAudioStream(inputParams,outputParams,srate,CallbackFXN);
}

AudioIOStream::~AudioIOStream() {
    Pa_CloseStream(mStream);
    mStream = nullptr;
}


PaError AudioIOStream::initializeAudioStream(PaStreamParameters *inputParams, PaStreamParameters *outputParams, int srate, PaStreamCallback CallbackFXN) {
    static testUserData data;

    PaError err = Pa_OpenStream(
        &mStream,
        inputParams,
        outputParams,
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

    if (err != paNoError) std::cout<<"Error opening audio stream: "<< Pa_GetErrorText(err)<<std::endl; ;

    return err;
}


PaError AudioIOStream::startStream() {
    return Pa_StartStream(mStream);
}

void AudioIOStream::endStream() {
    Pa_StopStream(mStream);
    printf("Stream ended\n");
}

void AudioIOStream::abortStream() {
    Pa_AbortStream(mStream);
}

void AudioIOStream::PrintSFormat() {
    auto streamInfo = Pa_GetStreamInfo(mStream);

    std::cout<<streamInfo->sampleRate<<std::endl;
}


int AudioCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData) {
    auto gAudioIO = AudioIO::Get();
    return gAudioIO->AudioIOCallback((constSamplePtr)inputBuffer, (float*) outputBuffer, framesPerBuffer, timeInfo, statusFlags, userData);
}

void ClampBuffer(float *pBuffer, unsigned long len) {
    for (unsigned long i = 0; i < len; i++) {
        pBuffer[i] = std::clamp(pBuffer[i], -1.0f, 1.0f);
    }
}

size_t AudioIoCallback::MinValues(const audioBuffers &buffers, size_t (audioBuffer::*pmf)() const) {
    return std::accumulate(buffers.begin(), buffers.end(),std::numeric_limits<size_t>::max(), [pmf](auto value, auto &pBuffer) {
        return std::min(value, (pBuffer.get()->*pmf)());
    });
}

unsigned AudioIoCallback::CountSoloSequences() {
    unsigned numSolo = 0;
    for (auto seq: mPlayableSequences) {
        if (seq->isSolo())
            numSolo++;
    }

    return numSolo;
}

bool AudioIoCallback::SequenceShouldBeSilent(const PlaybackSequence &ps) {
    return !ps.isSolo() && (mbHasSoloSequences || ps.isMute());
}

int AudioIoCallback::AudioIOCallback(constSamplePtr inputBuffer, float* outputBuffer,  unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData) {
    mbHasSoloSequences = CountSoloSequences();
    mCallbackReturn = paContinue;

    const auto PlaybackChannels = mNumCaptureChannels;
    const auto CaptureChannels = mNumCaptureChannels;
    const auto tempFloats = stackAllocate(float, framesPerBuffer*std::max(PlaybackChannels,CaptureChannels));

    if (isPaused()) {
        memset(outputBuffer, 0, framesPerBuffer*SAMPLE_SIZE(floatSample)*mMaxPLaybackChannels);
        return mCallbackReturn;
    }

    FillOutputBuffers(outputBuffer, framesPerBuffer);

    UpdateTimePosition(framesPerBuffer);

    DrainInputBuffers(inputBuffer, framesPerBuffer, tempFloats);

    return mCallbackReturn;
}

void AudioIoCallback::CallbackCompletion(int &callbackReturn, unsigned long len) {
    if (isPaused()) {
        return;
    }
    auto done = mPlaybackShchedule.GetPolicy().Done(mPlaybackShchedule, len);
    if (!done) {
        return;
    }

    callbackReturn = paComplete;
}

int AudioIoCallback::CallbackDoSeek() {
    mAudioThreadSequenceBufferExchangeLoopRunning.store(false);
    waitForAudioThreadStopped();

    const auto time = mPlaybackShchedule.GetPolicy().OffsetSequenceTime(mPlaybackShchedule, mSeek);

    mPlaybackShchedule.SetSequenceTime(time);
    mSeek = 0;

    mSamplePos = (sampleCount)(time*mRate);

    for (auto &buffer : mPlaybackBuffers) {
        const auto toDiscard = buffer->availForGet();
        buffer->discard( toDiscard );
    }

    mPlaybackShchedule.mTimeQueue.Prime(time);

    processOnceAndWait();

    mAudioThreadSequenceBufferExchangeLoopRunning.store(true);
    waitForAudioThreadStarted();

    return paContinue;
}

int AudioIoCallback::CallbackJumpToTime(){
    mAudioThreadSequenceBufferExchangeLoopRunning.store(false);
    waitForAudioThreadStopped();

    mPlaybackShchedule.SetSequenceTime(mNewTime);
    mSamplePos =(sampleCount) (mNewTime*mRate);

    for (auto &buffer : mPlaybackBuffers) {
        const auto toDiscard = buffer->availForGet();
        buffer->discard( toDiscard );
    }

    mPlaybackShchedule.mTimeQueue.Prime(mNewTime);

    mNewTime = -1.0;

    processOnceAndWait();

    mAudioThreadSequenceBufferExchangeLoopRunning.store(true);

    waitForAudioThreadStarted();

    return paContinue;
}



void AudioIoCallback::UpdateTimePosition(unsigned long framesPerBuffer) {
    mPlaybackShchedule.SetSequenceTime(mPlaybackShchedule.mTimeQueue.Consumer(framesPerBuffer, mRate));
}

void AudioIoCallback::FillOutputBuffers(float* outputFloats, unsigned long framesPerBuffer) {
    const auto numPlaybackSequences = mPlayableSequences.size();
    const auto numPlaybackChannels = mNumPlaybackChannels;
    const auto numMaxPlaybackChannels = mMaxPLaybackChannels;

    mMaxFramesOutput = 0;

    if (!outputFloats  || numPlaybackSequences <=0) {
        mMaxFramesOutput = framesPerBuffer;
        return;
    }

    if (mSeek) {
        //SOLUTION TO FIX AUDIO BUFFERING WHILE SEEKING WITH A LARGE NUMBER OF TRACKS, LOOK AT LATER TO MAYBE MAKE SEEKING THREADED?
        if (!isPaused()&&mSeeking<3) {
            memset(outputFloats, 0, framesPerBuffer*SAMPLE_SIZE(floatSample)*mMaxPLaybackChannels);
            mSeeking++;
            return;
        }
        mCallbackReturn = CallbackDoSeek();
        mSeeking = 0;
    }
    if (mNewTime != -1) {
        //SOLUTION TO FIX AUDIO BUFFERING WHILE SEEKING WITH A LARGE NUMBER OF TRACKS, LOOK AT LATER TO MAYBE MAKE SEEKING THREADED?
        if (!isPaused()&&mSeeking<3) {
            memset(outputFloats, 0, framesPerBuffer*SAMPLE_SIZE(floatSample)*mMaxPLaybackChannels);
            mSeeking++;
            return;
        }
        mCallbackReturn = CallbackJumpToTime();
        mSeeking = 0;
    }

    const auto toGet = std::min<size_t>(framesPerBuffer, CommonlyReadyPlayback());

    //--------- MEMORY ALLOCATIONS -----------
    const auto tempBufs = stackAllocate(float*, numPlaybackChannels);

    for (int i = 0; i < numPlaybackChannels; ++i) {
        tempBufs[i] = stackAllocate(float, framesPerBuffer);
    }
    //-------- END OF MEMORY ALLOCATIONS ---------

    int i = 0;
    for (int x = 0; x < numMaxPlaybackChannels; ++x) {
        if (mPlaybackMap[x]) {
            decltype(framesPerBuffer) len = mPlaybackBuffers[i]->Get(reinterpret_cast<samplePtr>(tempBufs[i]) , floatSample, toGet);

            if (len < framesPerBuffer) {
                //Capturing edge case where cpu is sometimes behind playback

                memset(&tempBufs[i][len], 0, (framesPerBuffer - len)*sizeof(float));
            }

            mMaxFramesOutput = std::max(mMaxFramesOutput, len);

            len = mMaxFramesOutput;

            if (len>0) {
                for (int f = 0; f < len; ++f) {
                    outputFloats[numMaxPlaybackChannels*f+x] = tempBufs[i][f];
                }
            }

            CallbackCompletion(mCallbackReturn, len);
            i++;
        } else {
            for (int f = 0; f < framesPerBuffer; ++f) {
                outputFloats[numMaxPlaybackChannels*f+x] = 0.0f;
            }
        }
    }

    ClampBuffer(outputFloats, framesPerBuffer*numPlaybackChannels);

}
void AudioIoCallback::DrainInputBuffers(constSamplePtr inputBuffer, unsigned long framesPerBuffer, float* tempFloats) {
    const auto numCaptureChannels = mMaxNumCaptureChannels;

    if (!inputBuffer || numCaptureChannels == 0) {
        return;
    }

    //Note: this shouldnt be needed because uncapped recordings?
    // If there is no playback sequence this wont get checked so do it here
    // if (mPlaybackShchedule.GetPolicy().Done(mPlaybackShchedule, 0)) {
    //     mCallbackReturn = paComplete;
    // }

    size_t len = framesPerBuffer;

    for (unsigned i = 0; i < mCaptureBuffers.size(); ++i) {
        len = std::min(len, mCaptureBuffers[i]->availForPut());
    }

    if (len<framesPerBuffer) {
        mLostSamples += framesPerBuffer-len;
        wxPrintf("lost %d samples \n", (int) (framesPerBuffer-len));
    }

    if (len<= 0) {
        return;
    }
    int buffer = 0;
    for (unsigned n = 0; n < numCaptureChannels; ++n) {
        //Only save data from that input channel if it is needed
        if (!mCaptureMap[n].empty()) {
            for (int x = 0; x < mCaptureMap[n].size(); ++x) {
                switch (mCaptureFormat) {
                    case floatSample: {
                        auto inputFloats = (const float*) inputBuffer;

                        for (unsigned i = 0; i < len; ++i) {
                            tempFloats[i] = inputFloats[numCaptureChannels*i + n];
                        }
                    } break;

                    case int24Sample: {
                        //IN THEORY SHOULD NEVER GET HERE
                        assert(false);
                    } break;

                    case int16Sample: {
                        auto inputShorts = (const short*) inputBuffer;
                        short* tempShorts = (short* ) tempFloats;

                        for (unsigned i = 0; i < len; ++i) {
                            float tmp = inputShorts[numCaptureChannels*i + n];
                            tmp = std::clamp(tmp, -32768.0f, 32767.0f);
                            tempShorts[i] = (short)tmp;
                        }
                    } break;
                }
                mCaptureBuffers[buffer]->Put((samplePtr) tempFloats, mCaptureFormat, len);
                mCaptureBuffers[buffer]->Flush();
                buffer++;
            }
        }
    }
}

bool AudioIoCallback::BuildMaps() {
    bool result = false;
    mCaptureMap.clear();
    mPlaybackMap.clear();
    if (mNumCaptureChannels > 0) {
        mCaptureMap.resize(mMaxNumCaptureChannels);
        for (const auto& pSeq: mRecordingSequences) {
            if (pSeq->isValid()) {
                mCaptureMap[pSeq->GetFirstChannelIN()].push_back(pSeq);
                if (pSeq->NChannels() ==2) {
                    mCaptureMap[pSeq->GetFirstChannelIN()+pSeq->NChannels()-1].push_back(pSeq);
                }
            }
        }
        result = true;
    }
    if (mNumPlaybackChannels>0) {
        mPlaybackMap.resize(mMaxPLaybackChannels);
        for (auto pSeq : mPlayableSequences) {
            if (pSeq->isValid()) {
                mPlaybackMap[pSeq->GetFirstChannelOut()] = pSeq;
                if (pSeq->NChannels() ==2) {
                    mPlaybackMap[pSeq->GetFirstChannelOut()+pSeq->NChannels()-1] = pSeq;
                }
            }
        }
    }
    return result;
}

size_t AudioIoCallback::CommonlyReadyPlayback() {
    return MinValues(mPlaybackBuffers, &audioBuffer::availForGet);
}

AudioIO::AudioIO() {

}

AudioIO::~AudioIO() {

    //Handle Exiting IO

    //Kill Stream & callback
    if (mAudioStream) {
        if (!mAudioStream->getStreamStillRunning()) {
            mAudioStream->endStream();
        }
    }


    Pa_Terminate();
    mAudioThread.join();

}

void AudioIO::Init() {
    sAudioDB = std::make_shared<DBConnection>();
    auto pAudioIO = new AudioIO();
    ugAudioIO.reset(pAudioIO);
    pAudioIO->startThread();
}

void AudioIO::DeInit() {
    ugAudioIO.reset();
    sAudioDB->close();
    sAudioDB.reset();
}

void AudioIO::startThread() {
    mAudioThread = std::thread(&AudioIO::audioThread, std::ref(mKillAudioThread));
}
void AudioIO::togglePause() {
    mPaused = !mPaused;
}


void AudioIoCallback::startAudioThread() {
    mAudioThreadSequenceBufferExchangeLoopRunning.store(true, std::memory_order_release);
}
void AudioIoCallback::stopAudioThread() {
    mAudioThreadSequenceBufferExchangeLoopRunning.store(false, std::memory_order_release);
}

void AudioIoCallback::waitForAudioThreadStarted() {
    while (mAudioThreadAcknowledge.load(std::memory_order_acquire) != eStart) {
        using namespace std::chrono;
        std::this_thread::sleep_for(50ms);
    }
    mAudioThreadAcknowledge.store(eNone, std::memory_order_release);
}

void AudioIoCallback::waitForAudioThreadStopped() {
    while (mAudioThreadAcknowledge.load(std::memory_order_acquire) != eStop) {
        using namespace std::chrono;
        std::this_thread::sleep_for(50ms);
    }
    mAudioThreadAcknowledge.store(eNone, std::memory_order_release);
}

void AudioIoCallback:: processOnceAndWait() {
    mAudioThreadShouldSequenceBufferExchangeOnce.store(true, std::memory_order_release);

    while (mAudioThreadShouldSequenceBufferExchangeOnce.load(std::memory_order_acquire))
    {
        using namespace std::chrono;
        std::this_thread::sleep_for(50ms);
    }
}



int AudioIO::startStream(const TransportSequence &sequences, double t0, double t1, const audioIoStreamOptions &options) {
    const auto &startTime = options.mStartTime;

    mRecordingSchedule = {};
    mRecordingSchedule.mLatencyCorrection = -130/1000.0;
    mRecordingSchedule.mDuration = t1-t0;
    mRate = options.mSampleRate;

    mRecordingSequences = sequences.captureSequences;
    mPlayableSequences = sequences.playableSequences;

    bool commit = false;
    auto cleanupSequences = finally([&] {
        if (!commit) {
            mRecordingSequences.clear();
            mPlayableSequences.clear();
        }
    });

    mPlaybackBuffers.clear();
    mCaptureBuffers.clear();
    mResample.clear();

    mPlaybackShchedule.mTimeQueue.Clear();

    mPlaybackShchedule.Init(t0, t1, mRecordingSequences.empty()?nullptr:&mRecordingSchedule);

    bool successAudio = createPortAudioStream(options);

    mPlaybackShchedule.GetPolicy().Initialize(mRate);

    if (!successAudio)
        return 0;

    if (!AllocateBuffers(mRate))
        return 0;

    BuildMaps();

    mSamplePos = sampleCount(t0*mRate);

    if (startTime) {
        auto time = *startTime;

        mPlaybackShchedule.SetSequenceTime(time);
        mPlaybackShchedule.GetPolicy().OffsetSequenceTime(mPlaybackShchedule, 0);
        mSamplePos = sampleCount(time*mRate);
    }

    mPlaybackShchedule.mTimeQueue.Prime(mPlaybackShchedule.GetSequenceTime());

    //Trigger the audio thread to sequence buffers so the output buffers have data in them once the stream gets started
    mAudioThreadShouldSequenceBufferExchangeOnce.store(true, std::memory_order_release);

    while (mAudioThreadShouldSequenceBufferExchangeOnce.load(std::memory_order_acquire)) {
        using namespace std::chrono;
        std::this_thread::sleep_for(50ms);
    }

    startAudioThread();

    PaError err = mAudioStream->startStream();

    if (err != paNoError) {
        std::cout<<"Error Starting the audio stream \n";

        stopAudioThread();

        startStreamCleanup();

        return 0;
    }

    if (isPaused()) {
        mPaused = false;
    }

    commit = true;
    waitForAudioThreadStarted();

    return 1;
}

bool AudioIO::createPortAudioStream(const audioIoStreamOptions &options) {
    mNumPlaybackChannels = options.mPlaybackChannels;
    mNumCaptureChannels = options.mCaptureChannels;
    mMaxNumCaptureChannels = Pa_GetDeviceInfo(options.InDev)->maxInputChannels;
    mMaxPLaybackChannels = Pa_GetDeviceInfo(options.OutDev)->maxOutputChannels;

    bool usePlayback = false, useCapture = false;
    PaStreamParameters outputParameters {};
    PaStreamParameters inputParameters {};

#ifdef __WXMSW__
    PaWasapiStreamInfo wasapiStreamInfo {};
#endif

    auto latencyDuration = 100.0;

    if (mNumPlaybackChannels > 0) {
        usePlayback = true;

        outputParameters.device = options.OutDev;

        const PaDeviceInfo *playbackDeviceInfo = Pa_GetDeviceInfo(outputParameters.device );

        if( playbackDeviceInfo == NULL )
            return false;

        outputParameters.sampleFormat = paFloat32;
        outputParameters.hostApiSpecificStreamInfo = NULL;
        outputParameters.channelCount = mMaxPLaybackChannels;

        const PaHostApiInfo* hostInfo = Pa_GetHostApiInfo(playbackDeviceInfo->hostApi);
        bool isWASAPI = (hostInfo && hostInfo->type == paWASAPI);

        outputParameters.suggestedLatency = isWASAPI ? 0.0 : latencyDuration/1000;
    }
    if (mNumCaptureChannels > 0) {
        useCapture = true;

        inputParameters.device = options.InDev;

        const PaDeviceInfo *captureDeviceInfo = Pa_GetDeviceInfo(inputParameters.device);
        if( captureDeviceInfo == NULL )
            return false;

        inputParameters.sampleFormat = paFloat32;
        inputParameters.hostApiSpecificStreamInfo = NULL;
        inputParameters.channelCount = mMaxNumCaptureChannels;

        inputParameters.suggestedLatency = latencyDuration/1000;
    }

    PaError err = paNoError;

    mAudioStream = std::make_unique<AudioIOStream>(err,
        useCapture? &inputParameters : nullptr,
        usePlayback? &outputParameters: nullptr,
        mRate, AudioCallback);

    if (!err) {
        mHardwarePlaybackLatency = lrint(mAudioStream->getOutputLatency()*mRate);
    }

    return err == paNoError;
}

void AudioIO::stopStream() {

    if (mAudioStream == nullptr)
        return;
    stopAudioThread();

    if (mAudioStream->getStreamStillRunning()) {
        mAudioStream->abortStream();
    }
    mAudioStream = nullptr;

    waitForAudioThreadStopped();

    //Ensure no data is lost from the buffers and dont make it into the recordable sequences
    processOnceAndWait();

    mPlaybackBuffers.clear();
    mPlaybackShchedule.mTimeQueue.Clear();

    {
        if (mRecordingSequences.size() >0) {
            mCaptureBuffers.clear();
            mResample.clear();

            for (auto seq : mRecordingSequences) {
                //Safe call this to ensure that it doesnt mess with other processes
                //could through errors when disk size is not adequeate

                try {
                    seq->Flush();
                } catch (...) {
                    printf("Error saving sequence");
                }
            }
        }
    }

    mNumCaptureChannels = 0;
    mMaxNumCaptureChannels = 0;
    mNumPlaybackChannels = 0;
    mMaxPLaybackChannels = 0;
    mCaptureMap.clear();
    mPlaybackMap.clear();


}


void AudioIO::startStreamCleanup(bool bClearBuffersOnly /*=false*/) {
    mPlaybackBuffers.clear();
    mCaptureBuffers.clear();
    if (!bClearBuffersOnly) {
        mAudioStream->abortStream();
        mAudioStream = nullptr;
    }
}


bool AudioIO::AllocateBuffers(double sampleRate) {
    auto policy = mPlaybackShchedule.GetPolicy();

    PlaybackPolicy::BufferTimes times = policy.SuggestedBufferTimes();
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
                mProcessingBuffers.resize(0);

                mPlaybackBuffers.resize(mNumPlaybackChannels);
                mProcessingBuffers.resize(mNumPlaybackChannels);

                for(auto& buffer : mProcessingBuffers)
                    buffer.reserve(bufferLength);

                //Generate Buffers
                std::generate(
                    mPlaybackBuffers.begin(),
                    mPlaybackBuffers.end(),
                    [=] {return std::make_unique<audioBuffer>(floatSample, bufferLength); }
                );

                mPlaybackQueueMinimum = lrint(mRate *times.latency.count());
                mPlaybackQueueMinimum = std::min(mPlaybackQueueMinimum, bufferLength);

                mPlaybackQueueMinimum = std::max(mPlaybackQueueMinimum, mHardwarePlaybackLatency);

                //Make the playback q min a multiple of playbacksamples
                mPlaybackQueueMinimum = mPlaybackSamplesToCopy * ((mPlaybackQueueMinimum + mPlaybackSamplesToCopy -1)/mPlaybackSamplesToCopy);

                const auto timeQueueSize = 1+(bufferLength+TimeQueueGrainSize-1)/TimeQueueGrainSize;
                mPlaybackShchedule.mTimeQueue.Init(timeQueueSize);
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
                mResample.resize(0);
                mResample.resize(mNumCaptureChannels);
                mFactor = sampleRate /mRate;


                std::generate(
                    mCaptureBuffers.begin(),
                    mCaptureBuffers.end(),
                    [&] {return std::make_unique<audioBuffer>(mCaptureFormat, bufferLength); }
                );

                std::generate(
                    mResample.begin(),
                    mResample.end(),
                    [&] {return std::make_unique<Resample>(true, mFactor, mFactor);});
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

size_t AudioIO::GetCommonlyAvailCapture() {
    return MinValues(mCaptureBuffers, &audioBuffer::availForGet);
}

size_t AudioIO::GetCommonlyFreePlayback() {
    return MinValues(mPlaybackBuffers, &audioBuffer::availForPut);
}

size_t AudioIO::GetCommonlyWrittenForPlayback() {
    return MinValues(mPlaybackBuffers, &audioBuffer::writtenForGet);
}

//Audio Thread Stuff
void AudioIO::audioThread(std::atomic<bool> &finish) {
    enum class State { eUndefined, eOnce, eLoopRunning, eDoNothing, eMonitoring } lastState = State::eUndefined;
    AudioIO* const gAudioIO = AudioIO::Get();
    while (!finish.load(std::memory_order_acquire)) {
        using Clock = std::chrono::steady_clock;
        auto loopPassStart = Clock::now();
        auto &schedule = gAudioIO->mPlaybackShchedule;
        const auto interval = schedule.GetPolicy().SleepInterval(schedule);

        if (gAudioIO->mAudioThreadShouldSequenceBufferExchangeOnce.load(std::memory_order_acquire)) {
            gAudioIO->sequenceBufferExchange();
            gAudioIO->mAudioThreadShouldSequenceBufferExchangeOnce.store(false, std::memory_order_release);

            lastState = State::eOnce;
        } else if (gAudioIO->mAudioThreadSequenceBufferExchangeLoopRunning.load(std::memory_order_relaxed)) {
            gAudioIO->mAudioThreadSequenceBufferExchangeActive
                .store(true, std::memory_order_relaxed);
            if (lastState != State::eLoopRunning) {
                gAudioIO->mAudioThreadAcknowledge.store(eStart, std::memory_order_release);
            }

            lastState = State::eLoopRunning;

            gAudioIO->sequenceBufferExchange();
        } else {
            if (lastState == State::eLoopRunning) {
                gAudioIO->mAudioThreadAcknowledge.store(eStop, std::memory_order_release);
            }
            lastState = State::eDoNothing;
            if (gAudioIO->isMonitoring()) {
                lastState = State::eMonitoring;
            }
        }

        gAudioIO->mAudioThreadSequenceBufferExchangeActive
        .store(false, std::memory_order_relaxed);

        std::this_thread::sleep_until( loopPassStart + interval );
    }

}


//Buffer Exchange
void AudioIO::sequenceBufferExchange() {
    FillPlayBuffers();
    DrainRecordBuffers();
}

void AudioIO::DrainRecordBuffers() {
    if (mRecordingSequences.empty()) {
        return;
    }
    //Might Need A Try Catch?
    {
        const auto avail = GetCommonlyAvailCapture();
        const auto remainingTime = std::max(0.0,  mRecordingSchedule.ToConsume());
        const auto remainingSamples = remainingTime * mRate;
        bool latencyCorrected = true;

        auto deltaT = avail/mRate;

        if (mAudioThreadShouldSequenceBufferExchangeOnce.load(std::memory_order_relaxed)|| deltaT >= mMinCaptureBufferSecsToCopy ) {
            bool newBlocks = false;

            size_t iChannel =0;
            size_t currChannelNum = 0;

            for (size_t i = 0; i < mCaptureBuffers.size(); i++) {
                for (auto pSeq: mCaptureMap[currChannelNum]) {
                    size_t discarded = 0;

                    if (!mRecordingSchedule.mLatencyCorrected) {
                        const auto correction = mRecordingSchedule.TotalCorrection();

                        if (correction >= 0) {
                            size_t size = floor(correction*mRate*mFactor);

                            SampleBuffer temp(size, mCaptureFormat);
                            ClearSamples(temp.ptr(), mCaptureFormat, 0, size);

                            (pSeq)->append(iChannel, temp.ptr(), mCaptureFormat, size, 1, narrowestSampleFormat);
                        } else {
                            size_t size = floor(mRecordingSchedule.ToDiscard() * mRate);

                            discarded = mCaptureBuffers[i] ->discard(std::min(avail, size));

                            if (discarded < size) {
                                latencyCorrected = false;
                            }
                        }
                    }

                    wxASSERT(discarded <= avail);
                    size_t toGet = avail - discarded;
                    SampleBuffer temp;
                    size_t size;
                    SampleFormat format;

                    if (mFactor == 1) {
                        size = toGet;
                        format = mCaptureFormat;

                        temp.Allocate(size, format);

                        const auto Got = mCaptureBuffers[i]->Get(temp.ptr(), format, size);

                        wxUnusedVar(Got);

                        if (double (size) > remainingSamples) {
                            size = floor(remainingSamples);
                        }
                    } else {
                        size = lrint(toGet * mFactor);
                        format = floatSample;
                        SampleBuffer temp1(toGet, floatSample);
                        temp.Allocate(size, format);

                        if (toGet > 0 ) {
                            if (double(toGet) > remainingSamples)
                                toGet = floor(remainingSamples);
                            const auto results =
                            mResample[i]->Process(mFactor, (float *)temp1.ptr(), toGet,
                                                  !isStreamRunning(), (float *)temp.ptr(), size);
                            size = results.second;
                        }
                    }
                    if (size > 0 ) {
                        newBlocks = ((pSeq) -> append(iChannel, temp.ptr(), format, size, 1, narrowestSampleFormat)) || newBlocks;
                    }
                    if (pSeq->NChannels() == 2) {
                        pSeq->toggleAppendSecond();
                    }
                }
                currChannelNum++;
            }
            mRecordingSchedule.mPosition += avail/mRate;
            mRecordingSchedule.mLatencyCorrected = latencyCorrected;

        }
    }
}

void AudioIO::FillPlayBuffers() {
    if (mNumPlaybackChannels == 0) {
        return;
    }

    auto nAvail = GetCommonlyFreePlayback();

    //Dont Waste cpu processing if not enough sampels to cpy
    if (nAvail< mPlaybackSamplesToCopy) {
        return;
    }

    auto GetNeeded = [&] {
        auto nReady = GetCommonlyWrittenForPlayback();
        return mPlaybackQueueMinimum - std::min(nReady, mPlaybackQueueMinimum);
    };

    auto nNeeded = GetNeeded();

    auto Flush = [&] {
        for (auto &pBuffer : mPlaybackBuffers) {
            pBuffer->Flush();
        }
    };

    while (true) {
        auto avail = std::min(nAvail, std::max(nNeeded, mPlaybackSamplesToCopy));

        Finally Do {Flush};

        //If not making any progress give up
        //no point in wasting resources.
        if (!ProcessPlaybackSlices(avail)) {
            break;
        }

        nNeeded = GetNeeded();
        if (nNeeded == 0) {
            break;
        }

        nAvail = GetCommonlyFreePlayback();
    }
}

bool AudioIO::ProcessPlaybackSlices(size_t avail) {
    auto policy = mPlaybackShchedule.GetPolicy();

    bool done = false;
    bool progress = false;



    const auto processingBufferOffsets = stackAllocate(size_t, mProcessingBuffers.size());
    for(unsigned n = 0; n < mProcessingBuffers.size(); ++n)
        processingBufferOffsets[n] = mProcessingBuffers[n].size();
    do {
        const auto slice = policy.GetPlaybackSlice(mPlaybackShchedule, avail);
        const auto &[frames, toProduce] = slice;
        progress = progress || toProduce >0;

        auto pos = mSamplePos;

        mPlaybackShchedule.mTimeQueue.Producer(mPlaybackShchedule, slice);

        auto iBuffer = 0;

        for (auto& pSeq : mPlayableSequences) {
            if (frames >0) {
                const auto nChannels = pSeq->NChannels();
                const auto appendPos = mProcessingBuffers[iBuffer].size();

                for (int i = 0; i < nChannels; ++i) {
                    auto& buffer = mProcessingBuffers[iBuffer+i];

                    //ensuring enough space in the buffer
                    buffer.resize(buffer.size() + frames, 0);

                    pSeq->GetFloats(i, (samplePtr) buffer.data()+appendPos, pos, toProduce, mPlaybackShchedule.ReversedTime());
                }

                iBuffer+=nChannels;
            }
            mSamplePos = pos+toProduce;
        }
        avail-=frames;


    } while (avail);

    //processing fx (mute/solo)
    {
        int iBuffer = 0;
        for (auto pSeq: mPlayableSequences) {
            if (!pSeq)
                continue;

            const auto offset = processingBufferOffsets[iBuffer];

            const auto len = mProcessingBuffers[iBuffer].size() - offset;

            if (len >0) {
                const auto silenced = SequenceShouldBeSilent(*pSeq);

                for (int i = 0; i < pSeq->NChannels(); ++i) {
                    auto buffer = mProcessingBuffers[iBuffer+i];

                    if (silenced) {
                        std::fill_n(buffer.data()+offset, len,0);
                    }
                }
            }
            iBuffer+=pSeq->NChannels();
        }
    }

    auto samplesAvailable = std::min_element(
      mProcessingBuffers.begin(),
      mProcessingBuffers.end(),
      [](auto& first, auto& second) { return first.size() < second.size(); }
    )->size();

    //Cleanup for processing buffers
    auto cleanup = finally([&] {
       for (auto &buffer : mProcessingBuffers) {
           buffer.erase(buffer.begin(), buffer.begin()+samplesAvailable);
       }
    });

    {
        int iBuffer = 0;

        for (auto buffer : mProcessingBuffers) {
            mPlaybackBuffers[iBuffer++]->Put(
                reinterpret_cast<constSamplePtr>(buffer.data()),
                floatSample,
                samplesAvailable);
        }
    }

    return progress;
}