//
// Created by cline on 2025-01-29.
//

#ifndef TRACK_H
#define TRACK_H



class Track {

    int inDevice;
    int outDevice;

    int numChannels;

public:
    Track();

    void changeInDevice(int);
    void changeOutDevice(int);
    void changeNumChannels(int);

private:

};



#endif //TRACK_H
