//
// Created by cline on 2025-03-17.
//

#ifndef PLAYBACKSCHEDULES_H
#define PLAYBACKSCHEDULES_H
#include <algorithm>
#include <atomic>
#include <chrono>

struct audioIoStreamOptions;

constexpr size_t TimeQueueGrainSize = 2000;

struct RecordingSchedule {
    double mPreRoll{};
    double mLatencyCorrection{}; // negative value usually
    double mDuration{};
    double mPosition{};
    bool mLatencyCorrected{};

    double TotalCorrection() const { return mLatencyCorrection - mPreRoll; }
    double ToConsume() const;
    double Consumed() const;
    double ToDiscard() const;
};

struct PlaybackSchedule;

struct PlaybackSlice {
    const size_t nFrames;
    const size_t toProduce;

    PlaybackSlice(
      size_t available, size_t frames_, size_t toProduce_)
      : nFrames{ std::min(available, frames_) }
    , toProduce{ std::min(toProduce_, nFrames) }
    {}
};

class PlaybackPolicy {
protected:
    double mRate = 0;

public:
    using Duration = std::chrono::duration<double>;

    virtual void Initialize(double rate);

    struct BufferTimes {
        Duration batchSize;
        Duration latency;
        Duration RingBufferDelay;
    };

    virtual BufferTimes SuggestedBufferTimes() {
        using namespace std::chrono;
        return { 0.05s, 0.05s, 0.25s };
    }

    virtual bool AllowSeek() { return true;}

    virtual bool Done( PlaybackSchedule &schedule,
       unsigned long outputFrames
    );

    virtual double OffsetSequenceTime( PlaybackSchedule &schedule, double offset );

    virtual std::chrono::milliseconds SleepInterval( PlaybackSchedule &schedule ) {
        using namespace std::chrono;
        return 10ms;
    }

    virtual std::pair<double, double>
      AdvancedTrackTime( PlaybackSchedule &schedule,
         double trackTime, size_t nSamples );


    virtual PlaybackSlice GetPlaybackSlice( PlaybackSchedule &schedule,
       size_t available
    );
};

struct PlaybackSchedule {
    double mT0;
    double mT1;
    double mCurrentTime;
    std::atomic<double> mTime;



    std::unique_ptr<PlaybackPolicy> mpPlaybackPolicy;

    class TimeQueue {
        double mLastTime {};

        struct Node final
        {
            struct Record final {
                double timeValue;
                // More fields to come
            };

            std::vector<Record> records;
            std::atomic<int> head { 0 };
            std::atomic<int> tail { 0 };
            std::atomic<Node*> next{};

            std::atomic_flag active { ATOMIC_FLAG_INIT };

            size_t offset { 0 };
            size_t written { 0 };

        };

        Node* mConsumerNode {};
        Node* mProducerNode {};

        std::vector<std::unique_ptr<Node>> mNodePool;

    public:
        TimeQueue() {};
        TimeQueue(const TimeQueue&) = delete;
        TimeQueue& operator=(const TimeQueue&) = delete;

        void Clear();
        void Init(size_t size);

        void Producer( PlaybackSchedule &schedule, PlaybackSlice slice );

        double GetLastTime() const { return mLastTime; }

        void SetLastTime(double time) {mLastTime = time; }

        double Consumer( size_t nSamples, double rate );

        void Prime( double time );
    } mTimeQueue;

    PlaybackPolicy &GetPolicy() {
        return *mpPlaybackPolicy;
    };
    const PlaybackPolicy &GetPolicy() const;

    void Init(
       double t0, double t1,
       const RecordingSchedule *pRecordingSchedule );

    bool ReversedTime() const
    {
        return mT1 < mT0;
    }

    double GetSequenceTime() const
    { return mTime.load(std::memory_order_relaxed); }

    void SetSequenceTime( double time )
    { mTime.store(time, std::memory_order_relaxed); }

    // Convert time between mT0 and argument to real duration, according to
    // time track if one is given; result is always nonnegative
    double RealDuration(double trackTime1) const;

    // Convert time between mT0 and argument to real duration, according to
    // time track if one is given; may be negative
    double RealDurationSigned(double trackTime1) const;

    // How much real time left?
    double RealTimeRemaining() const;

    // Advance the real time position
    void RealTimeAdvance( double increment );

    // Determine starting duration within the first pass -- sometimes not
    // zero
    void RealTimeInit( double trackTime );

    void RealTimeRestart();
};



#endif //PLAYBACKSCHEDULES_H
