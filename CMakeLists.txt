cmake_minimum_required(VERSION 3.12)

project(ais_forwarder)

add_executable(ais_forwarder 
               src/ais_forwarder.cpp)

# Enable debugging symbols
set(CMAKE_BUILD_TYPE Debug) 

# Install the binary to /usr/local/bin
install(TARGETS ais_forwarder DESTINATION bin)

# Install the systemd service file
install(FILES ais_forwarder.service DESTINATION lib/systemd/system)