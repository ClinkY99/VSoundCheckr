/*
 * This file is part of VSC+
 * Copyright (C) 2025 Kieran Cline
 *
 * Licensed under the GNU General Public License v3.0
 * See LICENSE file for details.
 */

#ifndef MIDIIO_H
#define MIDIIO_H
#include <memory>

#include <libremidi/libremidi.hpp>

#include "../Visual/PlaybackHandler.h"

using namespace libremidi;

struct midi1Data {
    //indicates type of midi data being recieved
    message_type status;

    uint8_t channel;

    uint8_t byte1;
    uint8_t byte2;

    midi1Data(): status(), channel(-1), byte1(-1), byte2(-1) {}
    midi1Data(const message& message) {
        assert(!message.empty());
        uint8_t statusByte = message[0];
        status = static_cast<message_type> (statusByte & 0xF0); // Extract message type
        channel = (statusByte & 0x0F) +1;

        if (message.size()>2) {
            byte1 = message[1];
            byte2 = message[2];
        } else if (message.size()==2) {
            byte1 = message[1];
            byte2 = -1;
        } else {
            byte1 = -1;
            byte2 = -1;
        }
    }

    midi1Data& operator= (message message) {
        assert(!message.empty());
        uint8_t statusByte = message[0];
        status = static_cast<message_type> (statusByte & 0xF0); // Extract message type
        channel = statusByte & 0x0F;

        if (message.size()>2) {
            byte1 = message[1];
            byte2 = message[2];
        } else if (message.size()==2) {
            byte1 = message[1];
            byte2 = -1;
        } else {
            byte1 = -1;
            byte2 = -1;
        }

        return *this;
    }
};

inline std::ostream& operator<< (std::ostream& s, const midi1Data& data) {
    s<<"[";
    switch (data.status) {
        case message_type::NOTE_ON:
            s << "note on | channel: "<<(int)data.channel<<" note: "<<(int)data.byte1<<" vel: "<<(int)data.byte2;
        break;
        case message_type::NOTE_OFF:
            s<<"note off | channel: "<<(int)data.channel<< " note: "<<(int)data.byte1<< " vel: "<<(int)data.byte2;
        break;
        case message_type::PROGRAM_CHANGE:
            s<<"program change | channel: "<<(int)data.channel<< "program: "<<(int)data.byte1;
        break;
        case message_type::CONTROL_CHANGE:
            s<<"control change | channel: "<<(int)data.channel<<" controller: "<<(int)data.byte1<<" change: "<<(int)data.byte2;
        break;
    }
    s<<"]";
    return s;
}

class MidiIO {
    std::optional<input_port> mInDev;

    std::shared_ptr<PlaybackHandler> mPlaybackHandler;

    midi_in* mMidiStream;

    bool mIsMonitering = false;
    bool mIsTesting = false;

public:

    MidiIO(std::shared_ptr<PlaybackHandler> playback_handler)
        :mInDev(midi1::in_default_port()), mPlaybackHandler(playback_handler) {

    }
    ~MidiIO();

    void startMidiMonitering();
    bool getIsMonitering(){return mIsMonitering;}
    void exitMidiMonitering();

    void testMidiConn();
    void endTest();


    void midiInputSelector();
private:
    //Callback Stuff
    void midiCallback(midi1Data message);
};



#endif //MIDIIO_H
