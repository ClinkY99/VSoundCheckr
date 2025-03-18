//#include "Audio/IO/AudioIO.h"
//#include "Visual/AppBase.h"

//wxIMPLEMENT_APP(MyApp);


#include <iostream>
#include <memory>

#include "Audio/AudioData/SqliteSampleBlock.h"
#include "Audio/IO/AudioIO.h"

using namespace std;
int main() {
    AudioIO::initAudio();

    auto sampleBlockFacotry = std::make_shared<SqliteSampleBlockFactory>();

    Floats samples = Floats((unsigned) 250);

    for (int i = 0; i < 50; ++i) {
        samples[i] = -1.0f + ((float)rand()/(RAND_MAX/2.0f));
        std::cout<<samples[i]<< ", ";
    }
    std::cout<<std::endl;

    auto blocktest = sampleBlockFacotry->Create((constSamplePtr)samples.get(), floatSample, 50);

    auto blockID = blocktest->getBlockID();

    auto values = blocktest->GetMaxMinRMS();

    std::cout<<"Max: "<<values.max<<" Min: "<<values.min << " RMS: "<<values.RMS<<std::endl;

    values = blocktest->GetMaxMinRMS(0, 50);

    std::cout<<"Max: "<<values.max<<" Min: "<<values.min << " RMS: "<<values.RMS<<std::endl;

    Floats summary256_1((unsigned)50*3);

    blocktest->GetSummary256(summary256_1.get(),0, 50);

    std::cout<<"SUMMARY 256: "<<std::endl;
    for (int i = 0; i < 50; ++i) {
        std::cout<<"Max: "<<summary256_1[i*3]<<" Min: "<<summary256_1[i*3+1] << " RMS: "<<summary256_1[i*3+2]<<std::endl;
    }

    blocktest->GetSummary256(summary256_1.get(),0, 50);
    values = blocktest->GetMaxMinRMS(0, 50);

    Floats samples_test((unsigned) 250);
    assert(blocktest->GetSamples((samplePtr)samples_test.get(), floatSample, 0, 250) == 250);

    auto blocktest2 = sampleBlockFacotry->CreateFromID(floatSample, blockID);

    assert(blocktest2->GetMaxMinRMS().RMS == blocktest->GetMaxMinRMS().RMS);
    Floats samples_test2((unsigned) 250);

    assert(blocktest2->GetSamples((samplePtr)samples_test2.get(), floatSample, 0, 250) == 250);

    AudioIO::sAudioDB->close();

    return 0;
}

