cmake_minimum_required(VERSION 3.21)
project(SamsungTvRemote VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network WebSockets)

qt_add_executable(SamsungTvRemote
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/TvDevice.h
    src/SsdpDiscovery.h
    src/SsdpDiscovery.cpp
    src/SamsungRemote.h
    src/SamsungRemote.cpp
)

target_link_libraries(SamsungTvRemote PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::WebSockets
)

if(APPLE)
    set_target_properties(SamsungTvRemote PROPERTIES MACOSX_BUNDLE TRUE)
endif()
