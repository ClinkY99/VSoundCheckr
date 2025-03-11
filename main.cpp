//#include "Audio/IO/AudioIO.h"
//#include "Visual/AppBase.h"

//wxIMPLEMENT_APP(MyApp);


#include <iostream>
#include <memory>

#include "Audio/AudioData/SqliteSampleBlock.h"
#include "Audio/IO/AudioIO.h"

int main() {
    auto sampleBlockFacotry = std::make_shared<SqliteSampleBlockFactory>();

    Floats samples = Floats((unsigned) 50);

    for (int i = 0; i < 50; ++i) {
        samples[i] = -1.0f + ((float)rand()/(RAND_MAX/2.0f));
        std::cout<<samples[i]<< ", ";
    }
    std::cout<<std::endl;

    auto blocktest = sampleBlockFacotry->Create((constSamplePtr)samples.get(), floatSample, 50);

    std::cout<<"TEST"<<std::endl;

    auto values = blocktest->GetMaxMinRMS();

    std::cout<<"Max: "<<values.max<<" Min: "<<values.min << " RMS: "<<values.RMS<<std::endl;

    return 0;
}

