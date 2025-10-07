/*
 * This file is part of SoundCheckr
 * Copyright (C) 2025 Kieran Cline
 *
 * Licensed under the GNU General Public License v3.0
 * See LICENSE file for details.
 */

#include "Track.h"

#include <iostream>
#include <wx/debug.h>

#include "../Visual/PlaybackHandler.h"


bool Track::append(size_t channel, constSamplePtr buffer, SampleFormat format, size_t len, unsigned int stride, SampleFormat effectiveFormat) {
    wxASSERT(channel < NChannels());
    bool appended = false;
    appended = mSequences[channel]->Append(buffer, format, len, stride, effectiveFormat);

    return appended;
}

void Track::Flush() {
    if (GetGreatestAppendBufferLen() > 0) {
        for (auto&
            seq: mSequences) {
            seq->Flush();
        }
    }
}

size_t Track::GetGreatestAppendBufferLen() const {
    size_t result = 0;
    for (int i = 0; i < NChannels(); ++i) {
        result = std::max(result, mSequences[i]->GetAppendBufferLen());
    }

    return result;
}

bool Track::doGet(size_t channel, samplePtr buffer, SampleFormat format, sampleCount start, size_t len, bool backwards) const {
    if (backwards) {
        start -= len;
    }
    auto samplesToCopy = std::min((sampleCount)len, mSequences[channel]->GetSampleCount());

    bool result = mSequences[channel]->getSamples(buffer, format, start, samplesToCopy.as_size_t());

    if (result && backwards) {
        ReverseSamples(buffer, format, 0, len);
    }

    return result;
}

void Track::updateSequences() {
    mSequences.clear();
    mSequences.resize(NChannels());
    for (int i = 0; i < NChannels(); ++i) {
        mSequences[i] = std::make_unique<Sequence>(std::make_unique<SqliteSampleBlockFactory>(), SampleFormats(floatSample, floatSample));
    }
}

//Saving

void Track::save() {
    auto stmt = mSaveConn->Prepare("INSERT INTO tracks (trackType, firstChannelIn, firstChannelOut,"
                                   "                        sampleRate, blocks)"
                                   "                VALUES(?1, ?2, ?3, ?4, ?5);");


    std::vector<int> blocks;
    //Using number of blocks for the first one, as both sequences have the same number of blocks
    for (int i = 0; i < mSequences[0]->getBlockCount(); ++i) {
        blocks.push_back(mSequences[0]->getBlockIDAtIndex(i));
        if (NChannels()>1) {
            blocks.push_back(mSequences[1]->getBlockIDAtIndex(i));
        }
    }

    size_t blocksBytes = sizeof(int)*blocks.size();

    if (sqlite3_bind_int(stmt, 1, mNumChannels) ||
        sqlite3_bind_int(stmt, 2, mFirstChannelNumIn) ||
        sqlite3_bind_int(stmt, 3, mFirstChannelNumOut) ||
        sqlite3_bind_double(stmt, 4, mRate) ||
        sqlite3_bind_blob(stmt, 5, blocks.data(), blocksBytes, SQLITE_STATIC)) {
        wxASSERT(false);
    }

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        //STEP FAILED (replace with log)
        wxASSERT(false);
    }

    sqlite3_finalize(stmt);
}

void Track::newShow() {
    auto stmt = mSaveConn->Prepare("INSERT INTO tracks (trackType, firstChannelIn, firstChannelOut,"
                                   "                        sampleRate)"
                                   "                VALUES(?1, ?2, ?3, ?4);");

    if (sqlite3_bind_int(stmt, 1, mNumChannels) ||
        sqlite3_bind_int(stmt, 2, mFirstChannelNumIn) ||
        sqlite3_bind_int(stmt, 3, mFirstChannelNumOut) ||
        sqlite3_bind_double(stmt, 4, mRate)) {
        wxASSERT(false);
        }

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        //STEP FAILED (replace with log)
        wxASSERT(false);
    }

    mSequences.clear();
    updateSequences();

    sqlite3_finalize(stmt);
}


void Track::load(int id) {
    std::cout<<"loading Track"<<id<<std::endl;
    auto stmt = mSaveConn->Prepare("SELECT trackType, firstChannelIn, firstChannelOut, sampleRate, blocks "
                                   "FROM tracks WHERE trackNum = ?1;");

    if (sqlite3_bind_int(stmt, 1, id)) {
        wxASSERT(false);
    }
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        wxASSERT(false);
    }


    mNumChannels = sqlite3_column_int(stmt, 0);
    mFirstChannelNumIn = sqlite3_column_int(stmt, 1);
    mFirstChannelNumOut = sqlite3_column_int(stmt, 2);
    mRate = sqlite3_column_double(stmt, 3);
    std::vector<int> blockIDs;
    auto bytes = sqlite3_column_bytes(stmt, 4);
    blockIDs.resize(bytes/sizeof(int));
    memcpy(blockIDs.data(), sqlite3_column_blob(stmt, 4), bytes);

    updateSequences();

    for (int i = 0; i < NChannels(); ++i) {
        for (int x = 0; x < blockIDs.size()/NChannels(); ++x) {
            mSequences[i]->loadBlockFromID(blockIDs[x*NChannels()+i]);
        }
    }

    sqlite3_finalize(stmt);
    std::cout<<"finished loading track num: "<<id<<std::endl;
}
