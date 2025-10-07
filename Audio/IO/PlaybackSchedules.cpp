/*
 * This file is part of VSoundCheckr
 * Copyright (C) 2025 Kieran Cline
 *
 * Licensed under the GNU General Public License v3.0
 * See LICENSE file for details.
 */

#include "PlaybackSchedules.h"

#include <algorithm>
#include <cmath>

#include "../SampleCount.h"

//Recording Stuff
double RecordingSchedule::ToConsume() const
{
    return mDuration - Consumed();
}

double RecordingSchedule::Consumed() const
{
    return std::max( 0.0, mPosition + TotalCorrection() );
}

double RecordingSchedule::ToDiscard() const
{
    return std::max(0.0, -( mPosition + TotalCorrection() ) );
}

//Playback Stuff
void PlaybackPolicy::Initialize(double rate) {
    mRate = rate;
}

bool PlaybackPolicy::Done(PlaybackSchedule &schedule, unsigned long outputFrames) {
    auto diff = schedule.GetSequenceTime() -schedule.mT1;
    if (schedule.ReversedTime()) {
        diff *= -1;
    }

    return sampleCount(floor(diff * mRate + 0.5)) >= 0 && outputFrames == 0;
}

double PlaybackPolicy::OffsetSequenceTime(PlaybackSchedule &schedule, double offset) {
    auto time = schedule.GetSequenceTime() + offset;
    time = std::clamp(time, schedule.mT0, schedule.mT1);

    schedule.RealTimeInit(time);

    return time;
}

PlaybackSlice PlaybackPolicy::GetPlaybackSlice(PlaybackSchedule &schedule, size_t available) {
    const auto realTimeRemaining = std::max(0.0, schedule.RealTimeRemaining());
    auto frames = available;
    auto toProduce = frames;
    double deltaT = frames/mRate;

    if (deltaT > realTimeRemaining) {
        const double extraRealTime = (TimeQueueGrainSize +1)/mRate;
        auto extra = std::min(extraRealTime, deltaT-realTimeRemaining);
        auto realTime = realTimeRemaining + extra;

        frames = realTime * mRate +0.5;
        toProduce = realTimeRemaining * mRate +0.5;
        schedule.RealTimeAdvance(realTime);
    } else
        schedule.RealTimeAdvance(deltaT);

    return {available, frames, toProduce};
}

std::pair<double, double> PlaybackPolicy::AdvancedTrackTime(PlaybackSchedule &schedule, double trackTime, size_t nSamples) {
    auto realDuration = nSamples/mRate;

    if (schedule.ReversedTime()) {
        realDuration *= -1;
    }
    trackTime += realDuration;

    if (trackTime >= schedule.mT1)
        return {schedule.mT1, std::numeric_limits<double>::infinity()};

    return {trackTime, trackTime};
}


const PlaybackPolicy &PlaybackSchedule::GetPolicy() const {
    return const_cast<PlaybackSchedule&>(*this).GetPolicy();
}

void PlaybackSchedule::Init(double t0, double t1, const RecordingSchedule *pRecordingSchedule) {
    mpPlaybackPolicy.reset();
    mpPlaybackPolicy = std::make_unique<PlaybackPolicy>();

    mT0 = t0;

    mT1 = t1;
    if (pRecordingSchedule) {
        mT1 -= pRecordingSchedule->mLatencyCorrection;
    }

    SetSequenceTime(mT0);
}

void PlaybackSchedule::RealTimeInit(double trackTime) {
    mCurrentTime = RealDurationSigned(trackTime);
}

double PlaybackSchedule::RealDurationSigned(double trackTime1) const {
    return trackTime1 - mT0;
}

double PlaybackSchedule::RealDuration(double trackTime1) const {
    return fabs(RealDurationSigned(trackTime1));
}

double PlaybackSchedule::RealTimeRemaining() const {
    return mT1-mCurrentTime;
}

void PlaybackSchedule::RealTimeRestart() {
    mCurrentTime = 0;
}

void PlaybackSchedule::RealTimeAdvance(double increment) {
    mCurrentTime += increment;
    mCurrentTime = std::clamp(mCurrentTime, 0.0, mT1);
}

void PlaybackSchedule::TimeQueue::Init(size_t size) {
    auto node = std::make_unique<Node>();
    mProducerNode = mConsumerNode = node.get();

    mProducerNode->active.test_and_set();
    mProducerNode->records.resize(size);

    mNodePool.clear();
    mNodePool.emplace_back(std::move(node));
}

void PlaybackSchedule::TimeQueue::Clear() {
    mNodePool.clear();
    mProducerNode = nullptr;
    mConsumerNode = nullptr;
}

double PlaybackSchedule::TimeQueue::Consumer(size_t nSamples, double rate) {
    auto node = mConsumerNode;

    if (!node) {
        return (mLastTime += nSamples/rate);
    }

    auto head = node->head.load(std::memory_order_acquire);
    auto tail = node->tail.load(std::memory_order_relaxed);
    auto offset = node->offset;
    auto avail = TimeQueueGrainSize-offset;

    if (nSamples >= avail) {
        do {
            offset = 0;
            nSamples -= avail;

            if (head==tail) {
                //check if buffer was reallocated
                if (auto next = node->next.load()) {
                    node->offset = 0;
                    node ->active.clear();

                    mConsumerNode = node = next;

                    head = 0;
                    tail = node->tail.load(std::memory_order_relaxed);
                    avail = TimeQueueGrainSize;
                } else {
                    return node->records[head].timeValue;
                }
            } else {
                head = (head+1) % static_cast<int>(node->records.size());
                avail = TimeQueueGrainSize;
            }
        } while (nSamples >= avail);
        node->head.store(head, std::memory_order_release);
    }
    node->offset = offset + nSamples;
    return node->records[head].timeValue;
}

void PlaybackSchedule::TimeQueue::Producer(PlaybackSchedule &schedule, PlaybackSlice slice) {
    auto &policy = schedule.GetPolicy();
    auto node = mProducerNode;

    if (!node) {
        return;
    }

    auto written = node->written;
    auto tail = node->tail.load(std::memory_order_acquire);
    auto head = node->head.load(std::memory_order_relaxed);
    auto time = mLastTime;

    auto frames = slice.toProduce;

    auto advanceTail = [&](double time) {
        auto newTail = (tail+1) % static_cast<int>(node->records.size());
        if ((newTail > head && static_cast<size_t>(newTail-head)==node->records.size()-1) ||
            (newTail < head && static_cast<size_t>(head-newTail)==node->records.size()-1)) {
            try {
                Node* next = nullptr;
                for (auto &p : mNodePool) {
                    if (p.get() == node || p->active.test_and_set())
                        continue;

                    next = p.get();
                    next->next.store(nullptr);
                    next->head.store(0);
                    next->tail.store(0);
                    break;
                }
                if (next == nullptr) {
                    mNodePool.emplace_back(std::make_unique<Node>());
                    next = mNodePool.back().get();
                }

                //previous node had too low capacity to fit all slices,
                //try enlarge capacity to avoid more reallocaitons
                next->records.resize(node->records.size() * 2);
                next->records[0].timeValue = time;

                node->next.store(next);//make it visible to the consumer
                mProducerNode = node = next;
                head = 0;
                newTail = 0;
            } catch (...) {
                newTail = tail;
            }
        } else
            node->records[newTail].timeValue = time;

        tail = newTail;

        node->written = 0;
    };

    auto space = TimeQueueGrainSize - written;

    while (frames >= space) {
        const auto times = policy.AdvancedTrackTime(schedule, time, space);

        time = times.second;
        if (!std::isfinite(time))
            time = times.first;

        advanceTail(time);

        written = 0;
        frames-=space;
        space = TimeQueueGrainSize;
    }

    if (frames >0) {
        const auto times = policy.AdvancedTrackTime(schedule, time, frames);

        time = times.second;
        if (!std::isfinite(time))
            time = times.first;

        written += frames;
        space -= frames;
    }

    frames = slice.nFrames - slice.toProduce;
    while (frames >0 && frames >= space) {
        advanceTail(time);

        frames -= space;
        written = 0;
        space = TimeQueueGrainSize;
    }

    mLastTime = time;
    node->written = written+frames;
    node->tail.store(tail, std::memory_order_release);
}

void PlaybackSchedule::TimeQueue::Prime(double time) {
    mLastTime = time;
    if(mProducerNode != nullptr)
    {
        mConsumerNode = mProducerNode;
        mConsumerNode->next.store(nullptr);
        mConsumerNode->head.store(0);
        mConsumerNode->tail.store(0);
        mConsumerNode->written = 0;
        mConsumerNode->offset = 0;
        mConsumerNode->records[0].timeValue = time;
    }
}








