/*
 * This file is part of SoundCheckr
 * Copyright (C) 2025 Kieran Cline
 *
 * Licensed under the GNU General Public License v3.0
 * See LICENSE file for details.
 */

#ifndef SNAPSHOTS_H
#define SNAPSHOTS_H

#include <string>
#include <vector>

#include "../Saving/SaveFileDB.h"

struct snapshotMidi {
    int controller = -1;
    int change = -1;
};

inline bool operator==(snapshotMidi x, snapshotMidi y) {
    return x.controller == y.controller && x.change == y.change;
}

struct Snapshot {
    std::string name;
    int number = -1;
    snapshotMidi midiKey;


    double timestamp = -1.0;
};

using Snapshots = std::vector<Snapshot>;

class SnapshotHandler
    : public SaveBase {
    Snapshots mSnapshots;
    int mCurrentSnapshot = 0;

public:
    void Init();

    int newSnapshot(snapshotMidi key, double time);
    int newSnapshot(double time);

    Snapshot getSnapshot(snapshotMidi key);
    Snapshot getSnapshot(int key);

    int getCurrentSnapshot() {return mCurrentSnapshot;}
    void setCurrentSnapshot(int key){mCurrentSnapshot = key;}

    bool assignMidi(snapshotMidi data);
    bool assignMidi(int key, snapshotMidi data);

    void assignName(std::string name, int key);

    Snapshots getSnapshots(){return mSnapshots;}

    void save() override;
    void newShow() override;
    void load(int id = 0) override;
};



#endif //SNAPSHOTS_H
