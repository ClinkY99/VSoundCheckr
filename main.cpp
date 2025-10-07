/*
 * This file is part of SoundCheckr
 * Copyright (C) 2025 Kieran Cline
 *
 * Licensed under the GNU General Public License v3.0
 * See LICENSE file for details.
 */

//#include "Audio/IO/AudioIO.h"
//#include "Visual/AppBase.h"

//wxIMPLEMENT_APP(MyApp);

#include <iostream>
#include <iterator>
#include <memory>


#include "Audio/Dither.h"
#include "Audio/IO/AudioIO.h"
#include "Visual/PlaybackHandler.h"

using namespace std;

// void test() {
//     DBConnection db;
//     db.open("C:/Users/cline/CLionProjects/RecordingSoftwareISP/cmake-build-debug/VirtualSoundCheckSessions/test.audio");
//     auto stmt = db.Prepare(DBConnection::GetSamples, "SELECT samples FROM sampleBlocks WHERE blockID = 3;");
//
//     auto err = sqlite3_step(stmt);
//
//     if ( err != SQLITE_ROW) {
//         throw;
//     }
//
//     samplePtr src = (samplePtr) sqlite3_column_blob(stmt, 0);
//     size_t blobBytes = sqlite3_column_bytes(stmt, 0);
//
//     std::ofstream file;
//     file.open("./test3");
//     file.write(src, blobBytes);
//
//     vector<float> output;
//     output.resize(blobBytes/SAMPLE_SIZE(floatSample));
//
//     CopySamples(src, floatSample, (samplePtr)(output.data()), floatSample, blobBytes/SAMPLE_SIZE(floatSample), none);
//
//     std::ofstream output_file("./test2");
//     std::ostream_iterator<float> output_iterator(output_file, ",");
//     std::copy(output.begin(), output.end(), output_iterator);
// }

int main() {
    auto app = make_shared<PlaybackHandler>();
    app->StartCApp();

   //test();
}

