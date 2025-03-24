//#include "Audio/IO/AudioIO.h"
//#include "Visual/AppBase.h"

//wxIMPLEMENT_APP(MyApp);


#include <iostream>
#include <memory>


#include "Audio/AudioData/SqliteSampleBlock.h"
#include "Audio/IO/AudioIO.h"
#include "Playback/PlaybackHandler.h"

using namespace std;
int main() {
    auto app = PlaybackHandler();
    app.StartCApp();
}

