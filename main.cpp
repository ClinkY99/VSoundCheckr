//#include "Audio/IO/AudioIO.h"
//#include "Visual/AppBase.h"

//wxIMPLEMENT_APP(MyApp);


#include <iostream>
#include <memory>

#include "Audio/AudioData/SqliteSampleBlock.h"
#include "Audio/IO/AudioIO.h"

using namespace std;
int main() {
    Pa_Initialize();

    int asioTest = Pa_HostApiTypeIdToHostApiIndex(paASIO);

    if (asioTest < 0) {
        cout << "Failed to use asio :(((" << endl;
    } else {
        cout<<"YAY IT WORKED"<<endl;
    }
}

