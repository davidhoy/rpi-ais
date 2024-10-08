cmake_minimum_required(VERSION 3.0)

project(ais_forwarder)

add_executable(ais_forwarder 
               src/ais_forwarder.cpp)

# Enable debugging symbols
set(CMAKE_BUILD_TYPE Debug) 
