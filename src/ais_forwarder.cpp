/*
 * AIS Forwarder Daemon
 * 
 * Copyright (c) 2025 David Hoy
 * SPDX-License-Identifier: MIT
 * 
 * This program connects to an AIS (Automatic Identification System) transponder over TCP,
 * monitors the connection health, and forwards valid NMEA sentences to MarineTraffic via UDP.
 * It provides robust connection management, system notifications for connection events,
 * and is suitable for running as a systemd service or standalone daemon.
 * 
 * Features:
 * - TCP connection to AIS transponder with keepalive and health checks.
 * - UDP forwarding of "!AIVDM" and "!AIVDO" NMEA sentences to MarineTraffic.
 * - System notifications via syslog and desktop notification (notify-send).
 * - Automatic reconnection and notification on connection loss/restoration.
 * - Designed for reliability and fast detection of connection issues.
 * 
 * Usage:
 *   Compile and run as a daemon on a Linux system with access to AIS and MarineTraffic endpoints.
 *   Customize notification user and addresses as needed.
 * 
 * Dependencies:
 *   - POSIX sockets
 *   - syslog/logger
 *   - notify-send (for desktop notifications)
 * 
 * License: MIT
 */

#include <iostream>
#include <cstring>
#include <string>
#include <fstream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdlib>
#include <chrono>
#include <ctime>
#include <sys/select.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <getopt.h>

// Configuration structure
struct Config {
    std::string ais_ip = "192.168.50.37";     // Default AIS IP
    int ais_port = 39150;                      // Default AIS port
    std::string mt_ip = "5.9.207.224";        // Default MarineTraffic IP
    int mt_port = 10170;                       // Default MarineTraffic port
    std::string notification_user = "david";   // User for desktop notifications
    std::string config_file = "";             // Optional config file path
};

// Function to load configuration from file
bool load_config_file(const std::string& filename, Config& config) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') continue;
        
        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;
        
        std::string key = line.substr(0, eq_pos);
        std::string value = line.substr(eq_pos + 1);
        
        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        if (key == "ais_ip") config.ais_ip = value;
        else if (key == "ais_port") config.ais_port = std::stoi(value);
        else if (key == "mt_ip") config.mt_ip = value;
        else if (key == "mt_port") config.mt_port = std::stoi(value);
        else if (key == "notification_user") config.notification_user = value;
    }
    
    return true;
}

// Function to load configuration from environment variables
void load_env_config(Config& config) {
    const char* env_val;
    
    if ((env_val = getenv("AIS_IP")) != nullptr) {
        config.ais_ip = env_val;
    }
    if ((env_val = getenv("AIS_PORT")) != nullptr) {
        config.ais_port = std::stoi(env_val);
    }
    if ((env_val = getenv("MT_IP")) != nullptr) {
        config.mt_ip = env_val;
    }
    if ((env_val = getenv("MT_PORT")) != nullptr) {
        config.mt_port = std::stoi(env_val);
    }
    if ((env_val = getenv("NOTIFICATION_USER")) != nullptr) {
        config.notification_user = env_val;
    }
}

// Function to show usage information
void show_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]\n"
              << "\nOptions:\n"
              << "  -h, --help                 Show this help message\n"
              << "  -c, --config FILE          Load configuration from file\n"
              << "  -a, --ais-ip IP            AIS transponder IP address\n"
              << "  -p, --ais-port PORT        AIS transponder port\n"
              << "  -m, --mt-ip IP             MarineTraffic server IP address\n"
              << "  -t, --mt-port PORT         MarineTraffic server port\n"
              << "  -u, --user USER            User for desktop notifications\n"
              << "\nEnvironment Variables:\n"
              << "  AIS_IP                     AIS transponder IP address\n"
              << "  AIS_PORT                   AIS transponder port\n"
              << "  MT_IP                      MarineTraffic server IP address\n"
              << "  MT_PORT                    MarineTraffic server port\n"
              << "  NOTIFICATION_USER          User for desktop notifications\n"
              << "\nConfiguration File Format:\n"
              << "  ais_ip=192.168.50.37\n"
              << "  ais_port=39150\n"
              << "  mt_ip=5.9.207.224\n"
              << "  mt_port=10170\n"
              << "  notification_user=david\n"
              << "\nPriority: Command line > Environment > Config file > Defaults\n";
}

// Function to send system notification
void send_notification(const std::string& title, const std::string& message, const std::string& notification_user, const std::string& urgency = "normal") {
    // Always log to syslog for reliable notification
    std::string syslog_command = "logger -t ais_forwarder \"" + title + ": " + message + "\"";
    system(syslog_command.c_str());
    
    // Try to send desktop notification to active user sessions
    // This works better for systemd services
    std::string desktop_notify = "sudo -u " + notification_user + " DISPLAY=:0 DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/$(id -u " + notification_user + ")/bus notify-send --urgency=" + urgency + " \"" + title + "\" \"" + message + "\" 2>/dev/null || true";
    system(desktop_notify.c_str());
}

// Function to get current timestamp as string
std::string get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    char buffer[100];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&time_t));
    return std::string(buffer);
}

// Function to set socket timeout
bool set_socket_timeout(int socket_fd, int timeout_seconds) {
    struct timeval timeout;
    timeout.tv_sec = timeout_seconds;
    timeout.tv_usec = 0;
    
    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        std::cerr << "Failed to set receive timeout" << std::endl;
        return false;
    }
    
    if (setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        std::cerr << "Failed to set send timeout" << std::endl;
        return false;
    }
    
    return true;
}

// Function to test if connection is still alive
bool is_connection_alive(int socket_fd) {
    // Try to send a small amount of data to test the connection
    // This will fail immediately if the connection is broken
    char test_byte = 0;
    ssize_t result = send(socket_fd, &test_byte, 0, MSG_NOSIGNAL);
    
    if (result < 0) {
        if (errno == EPIPE || errno == ECONNRESET || errno == ENOTCONN) {
            return false;  // Connection definitely broken
        }
    }
    
    // Check socket error status
    int error = 0;
    socklen_t len = sizeof(error);
    int retval = getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &error, &len);
    
    if (retval != 0 || error != 0) {
        return false;
    }
    
    // Try to peek at data without removing it from the queue
    char peek_byte;
    ssize_t peek_result = recv(socket_fd, &peek_byte, 1, MSG_PEEK | MSG_DONTWAIT);
    
    if (peek_result == 0) {
        // Connection closed by peer
        return false;
    } else if (peek_result < 0) {
        // Check if it's just no data available (normal) or actual error
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return true;  // No data available, but connection is fine
        } else if (errno == ECONNRESET || errno == ENOTCONN || errno == EPIPE) {
            return false;  // Connection broken
        }
    }
    
    return true;  // Data available or connection appears good
}

// Function to create and connect AIS socket
int connect_to_ais(const Config& config) {
    int ais_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (ais_sock == -1) {
        std::string error_msg = "Error creating AIS socket";
        std::cerr << get_timestamp() << " - " << error_msg << std::endl;
        send_notification("AIS Socket Error", error_msg, config.notification_user, "critical");
        return -1;
    }

    // Enable TCP keepalive to detect broken connections faster
    int keepalive = 1;
    if (setsockopt(ais_sock, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive)) < 0) {
        std::cerr << "Warning: Failed to set SO_KEEPALIVE" << std::endl;
    }
    
    // Set keepalive parameters for faster detection
    int keepidle = 10;   // Start keepalive after 10 seconds of inactivity
    int keepintvl = 5;   // Send keepalive every 5 seconds
    int keepcnt = 3;     // Give up after 3 failed keepalive attempts
    
    setsockopt(ais_sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle));
    setsockopt(ais_sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl));
    setsockopt(ais_sock, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt));

    // Define AIS address
    struct sockaddr_in ais_addr;
    ais_addr.sin_family = AF_INET;
    ais_addr.sin_port = htons(config.ais_port);
    inet_pton(AF_INET, config.ais_ip.c_str(), &ais_addr.sin_addr);

    // Connect to AIS
    if (connect(ais_sock, (struct sockaddr*)&ais_addr, sizeof(ais_addr)) < 0) {
        close(ais_sock);
        return -1;
    }
    
    // Set socket timeouts (10 seconds for faster detection)
    if (!set_socket_timeout(ais_sock, 10)) {
        std::string error_msg = "Failed to set socket timeouts";
        std::cerr << get_timestamp() << " - " << error_msg << std::endl;
        close(ais_sock);
        return -1;
    }
    
    return ais_sock;
}

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

int main(int argc, char* argv[]) {
    Config config;
    
    // Command line options
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"config", required_argument, 0, 'c'},
        {"ais-ip", required_argument, 0, 'a'},
        {"ais-port", required_argument, 0, 'p'},
        {"mt-ip", required_argument, 0, 'm'},
        {"mt-port", required_argument, 0, 't'},
        {"user", required_argument, 0, 'u'},
        {0, 0, 0, 0}
    };
    
    // Parse command line arguments
    int option_index = 0;
    int c;
    
    while ((c = getopt_long(argc, argv, "hc:a:p:m:t:u:", long_options, &option_index)) != -1) {
        switch (c) {
            case 'h':
                show_usage(argv[0]);
                return 0;
            case 'c':
                config.config_file = optarg;
                break;
            case 'a':
                config.ais_ip = optarg;
                break;
            case 'p':
                config.ais_port = std::stoi(optarg);
                break;
            case 'm':
                config.mt_ip = optarg;
                break;
            case 't':
                config.mt_port = std::stoi(optarg);
                break;
            case 'u':
                config.notification_user = optarg;
                break;
            case '?':
                show_usage(argv[0]);
                return 1;
            default:
                break;
        }
    }
    
    // Load configuration in priority order: defaults -> config file -> environment -> command line
    
    // 1. Load from config file if specified
    if (!config.config_file.empty()) {
        if (!load_config_file(config.config_file, config)) {
            std::cerr << "Warning: Could not load config file: " << config.config_file << std::endl;
        }
    } else {
        // Try default config file locations
        if (load_config_file("/etc/ais_forwarder.conf", config)) {
            std::cout << get_timestamp() << " - Loaded configuration from /etc/ais_forwarder.conf" << std::endl;
        } else if (load_config_file("./ais_forwarder.conf", config)) {
            std::cout << get_timestamp() << " - Loaded configuration from ./ais_forwarder.conf" << std::endl;
        }
    }
    
    // 2. Override with environment variables
    load_env_config(config);
    
    // 3. Command line arguments already parsed and override everything
    
    // Print configuration
    std::cout << get_timestamp() << " - Configuration:" << std::endl;
    std::cout << "  AIS: " << config.ais_ip << ":" << config.ais_port << std::endl;
    std::cout << "  MarineTraffic: " << config.mt_ip << ":" << config.mt_port << std::endl;
    std::cout << "  Notification User: " << config.notification_user << std::endl;

    // UDP Socket for MarineTraffic
    int mt_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (mt_sock == -1) {
        std::cerr << "Error creating MT socket" << std::endl;
        return 1;
    }

    // Define MarineTraffic address
    struct sockaddr_in mt_addr;
    mt_addr.sin_family = AF_INET;
    mt_addr.sin_port = htons(config.mt_port);
    inet_pton(AF_INET, config.mt_ip.c_str(), &mt_addr.sin_addr);

    int ais_sock = -1;
    bool was_connected = false;  // Track previous connection state
    bool connection_lost_notified = false;  // Track if we've already notified about loss
    std::string leftover;  // To store any incomplete NMEA sentence from previous reads
    
    while (true) {
        // Try to establish/maintain AIS connection
        if (ais_sock == -1) {
            std::cout << get_timestamp() << " - Attempting to connect to AIS transponder..." << std::endl;
            ais_sock = connect_to_ais(config);
            
            if (ais_sock != -1) {
                std::string success_msg = "Successfully connected to AIS transponder at " + config.ais_ip + ":" + std::to_string(config.ais_port);
                std::cout << get_timestamp() << " - " << success_msg << std::endl;
                
                // Only send notification if we had a previous connection (reconnection)
                // or if this is the first successful connection after failed attempts
                if (was_connected || connection_lost_notified) {
                    send_notification("AIS Connection Restored", success_msg, config.notification_user, "normal");
                } else {
                    // First time connecting since service start
                    send_notification("AIS Forwarder Started", success_msg, config.notification_user, "normal");
                }
                
                was_connected = true;
                connection_lost_notified = false;  // Reset the notification flag
                leftover.clear();  // Clear any leftover data from previous connection
            } else {
                // Connection failed
                if (was_connected && !connection_lost_notified) {
                    // We had a connection before and haven't notified about the loss yet
                    std::string error_msg = "Failed to reconnect to AIS transponder at " + config.ais_ip + ":" + std::to_string(config.ais_port);
                    send_notification("AIS Connection Failed", error_msg, config.notification_user, "critical");
                    connection_lost_notified = true;
                }
                // Wait before retrying (no notification spam)
                sleep(10);
                continue;
            }
        }
        
        // Connection is established, monitor and forward data
        auto last_health_check = std::chrono::steady_clock::now();
        const auto health_check_interval = std::chrono::seconds(5);  // Check every 5 seconds for faster detection
        
        while (ais_sock != -1) {
            // Periodic connection health check
            auto now = std::chrono::steady_clock::now();
            if (now - last_health_check >= health_check_interval) {
                if (!is_connection_alive(ais_sock)) {
                    std::string error_msg = "AIS connection health check failed - connection lost";
                    std::string timestamp_msg = get_timestamp() + " - " + error_msg;
                    
                    std::cerr << timestamp_msg << std::endl;
                    
                    // Send notification only once when connection is lost
                    if (!connection_lost_notified) {
                        send_notification("AIS Connection Lost", error_msg, config.notification_user, "critical");
                        connection_lost_notified = true;
                    }
                    
                    close(ais_sock);
                    ais_sock = -1;
                    break;  // Break out of data processing loop to reconnect
                }
                last_health_check = now;
            }

            // Use select to wait for data with timeout
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(ais_sock, &read_fds);
            
            struct timeval timeout;
            timeout.tv_sec = 2;   // 2 second timeout for more frequent health checks
            timeout.tv_usec = 0;
            
            int select_result = select(ais_sock + 1, &read_fds, NULL, NULL, &timeout);
            
            if (select_result < 0) {
                std::string error_msg = "Error in select() call - connection may be broken";
                std::string timestamp_msg = get_timestamp() + " - " + error_msg;
                
                std::cerr << timestamp_msg << std::endl;
                
                // Send notification only once when connection is lost
                if (!connection_lost_notified) {
                    send_notification("AIS Connection Error", error_msg, config.notification_user, "critical");
                    connection_lost_notified = true;
                }
                
                close(ais_sock);
                ais_sock = -1;
                break;
            } else if (select_result == 0) {
                // Timeout - no data available, continue to health check
                continue;
            }

            // Data is available, read it
            char buffer[1024];
            ssize_t bytes_received = recv(ais_sock, buffer, sizeof(buffer) - 1, 0);
            
            if (bytes_received < 0) {
                std::string error_msg = "Error reading from AIS socket - connection may be lost";
                std::string timestamp_msg = get_timestamp() + " - " + error_msg;
                
                std::cerr << timestamp_msg << std::endl;
                
                // Send notification only once when connection is lost
                if (!connection_lost_notified) {
                    send_notification("AIS Connection Lost", error_msg, config.notification_user, "critical");
                    connection_lost_notified = true;
                }
                
                close(ais_sock);
                ais_sock = -1;
                break;
            }
            
            if (bytes_received == 0) {
                std::string error_msg = "AIS connection closed by remote host";
                std::string timestamp_msg = get_timestamp() + " - " + error_msg;
                
                std::cerr << timestamp_msg << std::endl;
                
                // Send notification only once when connection is lost
                if (!connection_lost_notified) {
                    send_notification("AIS Connection Closed", error_msg, config.notification_user, "critical");
                    connection_lost_notified = true;
                }
                
                close(ais_sock);
                ais_sock = -1;
                break;
            }

            // Process received data
            buffer[bytes_received] = '\0';
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
    }

    // This should never be reached, but clean up if it is
    if (ais_sock != -1) {
        close(ais_sock);
    }
    close(mt_sock);
    return 0;
}
