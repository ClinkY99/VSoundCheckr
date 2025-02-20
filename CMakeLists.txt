cmake_minimum_required(VERSION 3.30)
project(RecordingSoftwareISP)

set(CMAKE_CXX_STANDARD 20)

add_link_options(-Xlinker --allow-multiple-definition)

add_executable(RecordingSoftwareISP main.cpp
        Audio/IO/AudioIO.cpp
        Audio/IO/AudioIO.h
        Visual/AppBase.cpp
        Visual/AppBase.h
        Audio/Track.cpp
        Audio/Track.h
        Audio/audioBuffers.cpp
        Audio/audioBuffers.h
        MemoryManagement/MemoryX.cpp
        MemoryManagement/MemoryX.h
        Audio/SampleFormat.h
)


find_package(wxWidgets CONFIG REQUIRED)
target_link_libraries(RecordingSoftwareISP PRIVATE wx::core wx::base)

find_package(portaudio CONFIG REQUIRED)
target_link_libraries(RecordingSoftwareISP PRIVATE portaudio_static)

