cmake_minimum_required(VERSION 3.30)
project(RecordingSoftwareISP)

set(CMAKE_CXX_STANDARD 20)

add_executable(RecordingSoftwareISP main.cpp
        Audio/AudioIO.cpp
        Audio/AudioIO.h
        Visual/AppBase.cpp
        Visual/AppBase.h
)

find_package(wxWidgets CONFIG REQUIRED)
target_link_libraries(RecordingSoftwareISP PRIVATE wx::core wx::base)