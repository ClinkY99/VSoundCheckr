/*
 * This file is part of SoundCheckr
 * Copyright (C) 2025 Kieran Cline
 *
 * Licensed under the GNU General Public License v3.0
 * See LICENSE file for details.
 */

#ifndef CHANNEL_H
#define CHANNEL_H


namespace AudioGraph {
    enum ChannelType {
        MonoChannel,
        SterioChannel
    };

    struct Channel {
        virtual ~Channel() = 0;
        virtual ChannelType getChannelType() const = 0;
    };

    inline bool isMono(const Channel &channel) {
        return channel.getChannelType() == MonoChannel;
    }

    inline bool isSterio(const Channel &channel) {
        return channel.getChannelType() == SterioChannel;
    }
}



#endif //CHANNEL_H
