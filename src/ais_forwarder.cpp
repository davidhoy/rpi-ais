#include <iostream>
#include <cstring>
#include <string>
#include <fstream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

// Address and port for AIS and MarineTraffic
const char* AIS_IP = "192.168.50.37";
const int AIS_PORT = 39150;
const char* MT_IP = "5.9.207.224";
const int MT_PORT = 10170;

// Function to run the daemon
void daemonize() {
    pid_t pid, sid;

    // Fork the parent process
    pid = fork();

    // If fork failed, exit
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }

    // If fork succeeded, exit parent process
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    // Change the file mode mask
    fd_mask(0);

    // Create a new SID for the child process
    sid = setsid();
    if (sid < 0) {
        exit(EXIT_FAILURE);
    }

    // Change the current working directory
    if ((chdir("/")) < 0) {
        exit(EXIT_FAILURE);
    }

    // Close stdin, stdout, stderr
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

int main() {
    // Daemonize this process
    daemonize();

    // TCP Socket for AIS
    int ais_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (ais_sock == -1) {
        std::cerr << "Error creating AIS socket" << std::endl;
        return 1;
    }

    // Define AIS address
    struct sockaddr_in ais_addr;
    ais_addr.sin_family = AF_INET;
    ais_addr.sin_port = htons(AIS_PORT);
    inet_pton(AF_INET, AIS_IP, &ais_addr.sin_addr);

    // Connect to AIS
    if (connect(ais_sock, (struct sockaddr*)&ais_addr, sizeof(ais_addr)) < 0) {
        std::cerr << "Error connecting to AIS" << std::endl;
        return 1;
    }

    // UDP Socket for MarineTraffic
    int mt_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (mt_sock == -1) {
        std::cerr << "Error creating MT socket" << std::endl;
        return 1;
    }

    // Define MarineTraffic address
    struct sockaddr_in mt_addr;
    mt_addr.sin_family = AF_INET;
    mt_addr.sin_port = htons(MT_PORT);
    inet_pton(AF_INET, MT_IP, &mt_addr.sin_addr);

    // Read from AIS and forward to MarineTraffic
    char buffer[1024];
    std::string leftover;  // To store any incomplete NMEA sentence from previous reads

    while (true) {
        // Read data from AIS
        ssize_t bytes_received = recv(ais_sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received < 0) {
            std::cerr << "Error reading from AIS socket" << std::endl;
            break;
        }

        // Null-terminate the string
        buffer[bytes_received] = '\0';
        
        // Append the received data to leftover (in case a previous read had an incomplete sentence)
        leftover += std::string(buffer);
        
        // Split the string by '\r\n' to extract complete NMEA sentences
        size_t pos = 0;
        std::string delimiter = "\r\n";
        while ((pos = leftover.find(delimiter)) != std::string::npos) {
            // Extract a complete NMEA sentence
            std::string nmea_str = leftover.substr(0, pos);

            // Remove the extracted sentence from the leftover string
            leftover.erase(0, pos + delimiter.length());

            // Filter out unwanted messages
            if (nmea_str.rfind("!AIVDM", 0) == 0 || nmea_str.rfind("!AIVDO", 0) == 0) {
                // Send NMEA string to MarineTraffic
                sendto(mt_sock, nmea_str.c_str(), nmea_str.size(), 0, (struct sockaddr*)&mt_addr, sizeof(mt_addr));
            }
        }
    }


    // Close sockets
    close(ais_sock);
    close(mt_sock);

    return 0;
}
