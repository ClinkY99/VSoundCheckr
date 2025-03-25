//#include "Audio/IO/AudioIO.h"
//#include "Visual/AppBase.h"

//wxIMPLEMENT_APP(MyApp);


#include <iostream>
#include <iterator>
#include <memory>


#include "Audio/Dither.h"
#include "Audio/AudioData/SqliteSampleBlock.h"
#include "Audio/IO/AudioIO.h"
#include "Visual/PlaybackHandler.h"

using namespace std;

#include <fstream>
#include <iostream>

void test() {
    DBConnection db;
    db.open("C:/Users/cline/CLionProjects/RecordingSoftwareISP/tmp/Unsaved Session 2025-03-25.16-03.audioUnsaved");
    auto stmt = db.Prepare(DBConnection::GetSamples, "SELECT samples FROM sampleBlocks WHERE blockID = 1;");

    auto err = sqlite3_step(stmt);

    if ( err != SQLITE_ROW) {
        throw;
    }

    samplePtr src = (samplePtr) sqlite3_column_blob(stmt, 0);
    size_t blobBytes = sqlite3_column_bytes(stmt, 0);

    std::ofstream file;
    file.open("./test3");
    file.write(src, blobBytes);

    vector<float> output;
    output.resize(8820/SAMPLE_SIZE(floatSample));

    CopySamples(src+14459, floatSample, (samplePtr)(output.data()), floatSample, 8820/SAMPLE_SIZE(floatSample), none);

    std::ofstream output_file("./test2");
    std::ostream_iterator<float> output_iterator(output_file, ",");
    std::copy(output.begin(), output.end(), output_iterator);
}

int main() {
    auto app = PlaybackHandler();
    app.StartCApp();

   // test();
}

