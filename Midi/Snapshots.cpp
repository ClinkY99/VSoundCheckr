/*
 * This file is part of SoundCheckr
 * Copyright (C) 2025 Kieran Cline
 *
 * Licensed under the GNU General Public License v3.0
 * See LICENSE file for details.
 */

#include "Snapshots.h"

#include <iostream>
#include <utility>

void SnapshotHandler::Init() {
    Snapshot snapshot;

    snapshot.timestamp = 0;
    snapshot.number = 0;
    snapshot.midiKey = {16,0};
    snapshot.name = "First";

    mSnapshots.push_back(snapshot);
}


int SnapshotHandler::newSnapshot(double time) {
    Snapshot snapshot;
    snapshot.timestamp = time;
    snapshot.number = mSnapshots.size();

    mSnapshots.push_back(snapshot);

    return snapshot.number;
}

int SnapshotHandler::newSnapshot(snapshotMidi key, double time) {
    Snapshot snapshot;
    if ((snapshot = getSnapshot(key)).timestamp == -1) {
        snapshot.timestamp = time;
        snapshot.midiKey = key;
        snapshot.number = mSnapshots.size();

        mSnapshots.push_back(snapshot);

        return snapshot.number;
    }

    if (getCurrentSnapshot()<snapshot.number) {
        mSnapshots[snapshot.number].timestamp = time;
    }

    return snapshot.number;
}

bool SnapshotHandler::assignMidi(int key, snapshotMidi data) {
    if (mSnapshots.size()> key) {
        mSnapshots[key].midiKey = data;
        return true;
    }
    return false;
}


bool SnapshotHandler::assignMidi(snapshotMidi data) {
    for (auto snapshot: mSnapshots) {
        if (snapshot.midiKey.controller == -1) {
            snapshot.midiKey = data;
            return true;
        }
    }
    return false;
}

Snapshot SnapshotHandler::getSnapshot(int key) {
    if (mSnapshots.size() > key) {
        return mSnapshots[key];
    }
    return {};
}

Snapshot SnapshotHandler::getSnapshot(snapshotMidi key) {
    for (auto snapshot: mSnapshots) {
        if (snapshot.midiKey == key) {
            return snapshot;
        }
    }
    return {};
}

void SnapshotHandler::assignName(std::string name, int key) {
    mSnapshots[key].name = std::move(name);
}

void SnapshotHandler::save() {
    auto stmt = mSaveConn->Prepare("INSERT INTO snapshots (snapshotNum, time, controller, change, name) "
                                   "                    VALUES(?1, ?2, ?3, ?4, ?5)");

    for (auto s: mSnapshots) {
        sqlite3_bind_int(stmt, 1, s.number);
        sqlite3_bind_double(stmt, 2, s.timestamp);
        sqlite3_bind_int(stmt, 3, s.midiKey.controller);
        sqlite3_bind_int(stmt, 4, s.midiKey.change);
        sqlite3_bind_text(stmt, 5, s.name.c_str(), -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cerr<<sqlite3_errmsg(mSaveConn->DB())<<std::endl;;
            assert(false);
        }

        sqlite3_clear_bindings(stmt);
        sqlite3_reset(stmt);
    }

    sqlite3_finalize(stmt);

    std::cout<<"finished saving snapshots"<<std::endl;
}

void SnapshotHandler::newShow() {
    auto stmt = mSaveConn->Prepare("INSERT INTO snapshots (snapshotNum, time, controller, change, name) "
                                  "                    VALUES(?1, ?2, ?3, ?4, ?5)");

    auto s = mSnapshots[0];

    sqlite3_bind_int(stmt, 1, s.number);
    sqlite3_bind_double(stmt, 2, s.timestamp);
    sqlite3_bind_int(stmt, 3, s.midiKey.controller);
    sqlite3_bind_int(stmt, 4, s.midiKey.change);
    sqlite3_bind_text(stmt, 5, s.name.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr<<sqlite3_errmsg(mSaveConn->DB())<<std::endl;;
        assert(false);
    }

    sqlite3_finalize(stmt);

    std::cout<<"finished saving snapshots"<<std::endl;

    load();
}

void SnapshotHandler::load(int id) {
    mSnapshots.clear();
    auto stmt = mSaveConn->Prepare("SELECT snapshotNum, time, controller, change, name FROM snapshots");

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Snapshot newSnapshot;

        newSnapshot.number = sqlite3_column_int(stmt, 0);
        newSnapshot.timestamp = sqlite3_column_double(stmt, 1);
        newSnapshot.midiKey.controller = sqlite3_column_int(stmt, 2);
        newSnapshot.midiKey.change = sqlite3_column_int(stmt, 3);
        newSnapshot.name = (char*) sqlite3_column_text(stmt, 4);

        mSnapshots.push_back(newSnapshot);
    }

    std::cout<<"finished loading snapshots"<<std::endl;

    sqlite3_finalize(stmt);
}
