cmake_minimum_required(VERSION 3.30)
project(RecordingSoftwareISP)

set(CMAKE_CXX_STANDARD 20)

add_link_options(-Xlinker --allow-multiple-definition)

add_executable(RecordingSoftwareISP main.cpp
        Audio/AudioIO.cpp
        Audio/AudioIO.h
        Visual/AppBase.cpp
        Visual/AppBase.h
        Audio/AudioIOBase.cpp
        Audio/AudioIOBase.h
)


find_package(wxWidgets CONFIG REQUIRED)
target_link_libraries(RecordingSoftwareISP PRIVATE wx::core wx::base)

find_package(portaudio CONFIG REQUIRED)
target_link_libraries(RecordingSoftwareISP PRIVATE portaudio_static)

