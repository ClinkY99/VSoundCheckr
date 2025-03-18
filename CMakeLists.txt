cmake_minimum_required(VERSION 3.30)
project(RecordingSoftwareISP)

set(CMAKE_CXX_STANDARD 20)

add_link_options(-Xlinker --allow-multiple-definition)

add_executable(RecordingSoftwareISP main.cpp
        Audio/IO/AudioIO.cpp
        Audio/IO/AudioIO.h
        Visual/AppBase.cpp
        Visual/AppBase.h
        Playback/Track.cpp
        Playback/Track.h
        Audio/audioBuffers.cpp
        Audio/audioBuffers.h
        MemoryManagement/MemoryX.cpp
        MemoryManagement/MemoryX.h
        Audio/SampleFormat.h
        Audio/Dither.cpp
        Audio/Dither.h
        MemoryManagement/Math/float_cast.h
        Audio/SampleFormat.cpp
        Playback/AudioGraph/Channel.cpp
        Playback/AudioGraph/Channel.h
        Playback/AudioGraph/buffers.cpp
        Playback/AudioGraph/buffers.h
        Playback/Sequences/AudioIOSequences.cpp
        Playback/Sequences/AudioIOSequences.h
        Audio/SampleCount.cpp
        Audio/SampleCount.h
        Playback/Sequences/SampleTrack.cpp
        Playback/Sequences/SampleTrack.h
        Audio/AudioData/Sequence.cpp
        Audio/AudioData/Sequence.h
        Audio/AudioData/SampleBlock.cpp
        Audio/AudioData/SampleBlock.h
        Audio/AudioData/SqliteSampleBlock.cpp
        Audio/AudioData/SqliteSampleBlock.h
        Saving/DBConnection.cpp
        Saving/DBConnection.h
        project.cpp
        project.h
)

find_package(wxWidgets CONFIG REQUIRED)
target_link_libraries(RecordingSoftwareISP PRIVATE wx::core wx::base)

find_package(portaudio CONFIG REQUIRED)
target_link_libraries(RecordingSoftwareISP PRIVATE portaudio_static)

find_package(unofficial-sqlite3 CONFIG REQUIRED)
target_link_libraries(RecordingSoftwareISP PRIVATE unofficial::sqlite3::sqlite3)

