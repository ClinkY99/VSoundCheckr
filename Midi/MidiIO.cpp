/*
 * This file is part of VSC+
 * Copyright (C) 2025 Kieran Cline
 *
 * Licensed under the GNU General Public License v3.0
 * See LICENSE file for details.
 */

#include "MidiIO.h"

#include <iostream>
#include <libremidi/cmidi2.hpp>

MidiIO::~MidiIO() {
    assert(!mMidiStream->is_port_open());
}



void MidiIO::midiCallback(midi1Data message) {
    if (message.status == message_type::PROGRAM_CHANGE) {
        int PC = message.byte1;
        mPlaybackHandler->setMidiAction(PC);
    } else if (message.status == message_type::CONTROL_CHANGE) {
        int controller = message.byte1;
        int change = message.byte2;
        mPlaybackHandler->snapshotAction({controller, change});
    }
}

void MidiIO::startMidiMonitering() {
    auto callback = [&](midi1Data message) {
        midiCallback(message);
    };

    mMidiStream = new midi_in(input_configuration{.on_message = callback});
    if (mInDev.has_value()) {
        mMidiStream->open_port(mInDev.value());
    }

    mIsMonitering = mMidiStream->is_port_open();
}

void MidiIO::exitMidiMonitering() {
    mMidiStream->close_port();
    mIsMonitering = mMidiStream->is_port_open();
}

void MidiIO::testMidiConn() {
    auto callback = [](midi1Data message) {
        std::cout<<"Received Midi Command: "<<std::endl;
        std::cout<<message<<std::endl;
    };
    mMidiStream = new midi_in(input_configuration{.on_message = callback});
    if (mInDev.has_value()) {
        mMidiStream->open_port(mInDev.value());
    }
    mIsTesting = mMidiStream->is_port_open();
}

void MidiIO::endTest() {
    mMidiStream->close_port();
    mIsTesting = mMidiStream->is_port_open();
}



void MidiIO::midiInputSelector() {
    observer observer;
    using namespace std;

    if (mInDev.has_value()) {
        cout<<"Current In Device: "<< mInDev.value().device_name<<endl;
    }

    cout<<"Available In devices: "<<endl;
    if (observer.get_input_ports().size() == 0) {
        cout<<"No available input devices"<<endl;
        return;
    }

    for (int i = 0; i < observer.get_input_ports().size(); ++i) {
        auto inDev = observer.get_input_ports()[i];
        if (mInDev.has_value()) {
            if (inDev.device_name == mInDev.value().device_name) {
                cout<<">>  ";
            } else {
                cout<<"    ";
            }
        } else {
            cout<<"    ";
        }

        cout<<i+1<<" "<<inDev.port_name<<endl;

    }
    cout<<">>";
    int input;
    cin>>input;
    input--;

    if (input < observer.get_input_ports().size()) {
        mInDev = observer.get_input_ports()[input];
    } else {
        cout<<input+1<<" out of range"<<endl;
    }
}

