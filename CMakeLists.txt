cmake_minimum_required(VERSION 3.30)
project(VSoundCheckr)

set(CMAKE_CXX_STANDARD 20)

add_link_options(-Xlinker --allow-multiple-definition)

set(BUILD_SHARED_LIBS OFF)
set(CMAKE_EXE_LINKER_FLAGS "-static")

add_definitions(-DPA_USE_ASIO=1)

add_executable(VSoundCheckr main.cpp
        Audio/IO/AudioIO.cpp
        Audio/IO/AudioIO.h
        Visual/AppBase.cpp
        Visual/AppBase.h
        Playback/Track.cpp
        Playback/Track.h
        Audio/audioBuffers.cpp
        Audio/audioBuffers.h
        MemoryManagement/MemoryTypes.cpp
        MemoryManagement/MemoryTypes.h
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
        Audio/AudioData/Sequence.cpp
        Audio/AudioData/Sequence.h
        Audio/AudioData/SampleBlock.cpp
        Audio/AudioData/SampleBlock.h
        Audio/AudioData/SqliteSampleBlock.cpp
        Audio/AudioData/SqliteSampleBlock.h
        Saving/DBConnection.cpp
        Saving/DBConnection.h
        Audio/IO/PlaybackSchedules.cpp
        Audio/IO/PlaybackSchedules.h
        Audio/IO/Resample.cpp
        Audio/IO/Resample.h
        Visual/PlaybackHandler.cpp
        Visual/PlaybackHandler.h
        Saving/SaveFileDB.cpp
        Saving/SaveFileDB.h
        Midi/MidiIO.cpp
        Midi/MidiIO.h
        Midi/Snapshots.cpp
        Midi/Snapshots.h
        Saving/Exporter.cpp
        Saving/Exporter.h
        Icon/app.o
)

find_package(wxWidgets CONFIG REQUIRED)
target_link_libraries(VSoundCheckr PRIVATE wx::core wx::base)

find_package(portaudio CONFIG REQUIRED)
target_link_libraries(VSoundCheckr PRIVATE portaudio_static)

find_package(libremidi CONFIG REQUIRED)
target_link_libraries(VSoundCheckr PRIVATE libremidi)

find_package(unofficial-sqlite3 CONFIG REQUIRED)
target_link_libraries(VSoundCheckr PRIVATE unofficial::sqlite3::sqlite3)

# Add include directories for soxr
target_include_directories(VSoundCheckr PRIVATE
        ${CMAKE_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/include
)

# Add library directories for soxr
target_link_directories(VSoundCheckr PRIVATE
        ${CMAKE_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/lib
)

# Link the soxr library
target_link_libraries(VSoundCheckr PRIVATE ${CMAKE_SOURCE_DIR}/cmake-build-debug/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/lib/libsoxr.a)