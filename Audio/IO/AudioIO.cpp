//
// Created by cline on 2025-01-28.
//

#include "AudioIO.h"

#include "../../MemoryManagement/Math/float_cast.h"
#include <cstdio>
#include <iostream>
#include <numeric>
#include <wx/debug.h>
#include <wx/wxcrtvararg.h>

std::shared_ptr<DBConnection> AudioIOBase::sAudioDB;
std::unique_ptr<AudioIOBase> AudioIOBase::ugAudioIO;

#define stackAllocate(T, count) static_cast<T*>(alloca(count * sizeof(T)))


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

void ClampBuffer(float *pBuffer, unsigned long len) {
    for (unsigned long i = 0; i < len; i++) {
        pBuffer[i] = std::clamp(pBuffer[i], -1.0f, 1.0f);
    }
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

int AudioIoCallback::AudioCallback(constSamplePtr inputBuffer, float* outputBuffer,  unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData) {
    mbHasSoloSequences = CountSoloSequences();
    mCallbackReturn = paContinue;

    const auto PlaybackChannels = mNumPlaybackChannels;
    const auto CaptureChannels = mNumCaptureChannels;
    const auto tempFloats = stackAllocate(float, framesPerBuffer*std::max(PlaybackChannels,CaptureChannels));

    if (isPaused())
        return mCallbackReturn;

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


void AudioIoCallback::UpdateTimePosition(unsigned long framesPerBuffer) {
    mPlaybackShchedule.SetSequenceTime(mPlaybackShchedule.mTimeQueue.Consumer(framesPerBuffer, mRate));
}

void AudioIoCallback::FillOutputBuffers(float* outputFloats, unsigned long framesPerBuffer) {
    const auto numPlaybackSequences = mPlayableSequences.size();
    const auto numPlaybackChannels = mNumPlaybackChannels;

    mMaxFramesOutput = 0;

    if (!outputFloats  || numPlaybackSequences <=0) {
        mMaxFramesOutput = framesPerBuffer;
        return;
    }

    const auto toGet = std::min<size_t>(framesPerBuffer, CommonlyReadyPlayback());

    //--------- MEMORY ALLOCATIONS -----------
    const auto tempBufs = stackAllocate(float*, numPlaybackChannels);

    for (int i = 0; i < numPlaybackChannels; ++i) {
        tempBufs[i] = stackAllocate(float, framesPerBuffer);
    }
    //-------- END OF MEMORY ALLOCATIONS ---------


    for (int i = 0; i < numPlaybackChannels; ++i) {
        decltype(framesPerBuffer) len = mPlaybackBuffers[i]->Get(reinterpret_cast<samplePtr>(tempBufs[i]) , floatSample, toGet);

        if (len < framesPerBuffer) {
            //Capturing edge case where cpu is sometimes behind playback

            memset(&tempBufs[i][len], 0, (framesPerBuffer - len)*sizeof(float));
        }

        mMaxFramesOutput = std::max(mMaxFramesOutput, len);

        len = mMaxFramesOutput;

        if (len>0) {
            for (int f = 0; f < len; ++f) {
                outputFloats[numPlaybackChannels*f+i] += tempBufs[i][f];
            }
        }

        CallbackCompletion(mCallbackReturn, len);
    }

    ClampBuffer(outputFloats, framesPerBuffer*numPlaybackChannels);

}
void AudioIoCallback::DrainInputBuffers(constSamplePtr inputBuffer, unsigned long framesPerBuffer, float* tempFloats) {
    const auto numCaptureChannels = mNumCaptureChannels;

    if (!inputBuffer || numCaptureChannels) {
        return;
    }

    //If there is no playback sequence this wont get checked so do it here
    if (mPlaybackShchedule.GetPolicy().Done(mPlaybackShchedule, 0)) {
        mCallbackReturn = paContinue;
    }

    size_t len = framesPerBuffer;

    for (unsigned i = 0; i < numCaptureChannels; ++i) {
        len = std::min(len, mCaptureBuffers[i]->availForPut());
    }

    if (len<framesPerBuffer) {
        mLostSamples += framesPerBuffer-len;
        wxPrintf("lost %d samples \n", (int) (framesPerBuffer-len));
    }

    if (len<= 0) {
        return;
    }

    for (unsigned n = 0; n < numCaptureChannels; ++n) {

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

        mCaptureBuffers[n]->Put((samplePtr) tempFloats, mCaptureFormat, len);
        mCaptureBuffers[n]->Flush();
    }
}

size_t AudioIoCallback::CommonlyReadyPlayback() {
    return MinValues(mPlaybackBuffers, &audioBuffer::availForGet);
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

void AudioIO::Init() {
    auto pAudioIO = new AudioIO();
    ugAudioIO.reset(pAudioIO);
    pAudioIO->startAudioThread();
}

void AudioIO::DeInit() {
    ugAudioIO.reset();
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
    return MinValues(mPlaybackBuffers, &audioBuffer::availForGet);
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
            if (lastState != State::eLoopRunning) {
                //Notify Main Thread that we started
            }

            lastState = State::eLoopRunning;

            gAudioIO->sequenceBufferExchange();
        } else {
            if (lastState == State::eLoopRunning) {
                //Notify Main thread that we stopped
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

            auto iter = mRecordingSequences.begin();
            auto width = (*iter)->NChannels();
            size_t iChannel = 0;

            for (size_t i = 0; i < mNumCaptureChannels; i++) {
                Finally Do{[&] {
                    if (++iChannel == width) {
                        iter++;
                        iChannel = 0;
                        if (iter != mRecordingSequences.end()) {
                            width = (*iter)->NChannels();
                        }
                    }
                }};

                size_t discarded = 0;

                if (!mRecordingSchedule.mLatencyCorrected) {
                    const auto correction = mRecordingSchedule.TotalCorrection();

                    if (correction >= 0) {
                        size_t size = floor(correction*mRate*mFactor);

                        SampleBuffer temp(size, mCaptureFormat);
                        ClearSamples(temp.ptr(), mCaptureFormat, 0, size);

                        (*iter)->append(iChannel, temp.ptr(), mCaptureFormat, size, 1, narrowestSampleFormat);
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

                newBlocks = ((*iter) -> append(iChannel, temp.ptr(), format, size, 1, narrowestSampleFormat)) || newBlocks;
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
    if (nAvail> mPlaybackSamplesToCopy) {
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

sampleCount AudioIO::getWritePos(){
    return ((size_t) (mPlaybackShchedule.mTimeQueue.GetLastTime()*mRate));
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

        auto pos = getWritePos();

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

                    pSeq->GetFloats(i, (samplePtr) buffer.data(), pos, toProduce, mPlaybackShchedule.ReversedTime());
                }

                iBuffer+=nChannels;
            }
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

    //Cleanup for processing buffers
    auto cleanup = finally([&] {
       for (auto buffer : mProcessingBuffers) {
           buffer.clear();
       }
    });

    {
        int iBuffer = 0;

        for (auto buffer : mProcessingBuffers) {
            const auto offset = processingBufferOffsets[iBuffer];
            mPlaybackBuffers[iBuffer++]->Put(
                reinterpret_cast<constSamplePtr>(buffer.data()) + offset*sizeof(float),
                floatSample,
                avail);
        }
    }

    return progress;
}








