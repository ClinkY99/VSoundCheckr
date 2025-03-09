//
// Created by cline on 2025-03-08.
//

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
