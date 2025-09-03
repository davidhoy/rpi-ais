# AIS Forwarder

A robust C++ daemon that connects to an AIS (Automatic Identification System) transponder over TCP and forwards NMEA sentences to MarineTraffic via UDP.

## Features

- **Robust Connection Management**: TCP connection with keepalive, health checks, and automatic reconnection
- **Fast Failure Detection**: Detects connection issues within 5-15 seconds
- **NMEA Filtering**: Forwards only `!AIVDM` and `!AIVDO` sentences to MarineTraffic
- **System Notifications**: Desktop notifications and syslog messages for connection events
- **Systemd Integration**: Designed to run as a reliable systemd service
- **Smart Notification Logic**: Avoids notification spam - only alerts on state changes

## Architecture

```
AIS Transponder (TCP) → AIS Forwarder → MarineTraffic (UDP)
     192.168.50.37:39150                5.9.207.224:10170
```

## Prerequisites

- Linux system with C++ compiler (g++)
- CMake build system
- System packages:
  - `libnotify-bin` (for desktop notifications)
  - `dbus-x11` (for notification system integration)

## Installation

### 1. Clone and Build

```bash
git clone <repository-url>
cd rpi-ais
mkdir -p build
cd build
cmake ..
make
```

### 2. Create Configuration (Optional)

The service can be configured in multiple ways with the following priority:
**Command line arguments > Environment variables > Config file > Default values**

#### Option 1: Configuration File

```bash
# Copy sample config and edit
cp ../ais_forwarder.conf /etc/ais_forwarder.conf
sudo nano /etc/ais_forwarder.conf
```

#### Option 2: Environment Variables

```bash
export AIS_IP=192.168.50.37
export AIS_PORT=39150
export MT_IP=5.9.207.224
export MT_PORT=10170
export NOTIFICATION_USER=david
```

#### Option 3: Command Line Arguments

```bash
./ais_forwarder --ais-ip 192.168.50.37 --ais-port 39150 --user david
```

### 3. Install Binary

```bash
sudo cp ais_forwarder /usr/local/bin/ais_forwarder
sudo chmod +x /usr/local/bin/ais_forwarder
```

### 4. Install and Enable Service

```bash
sudo cp ../ais_forwarder.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable ais_forwarder.service
sudo systemctl start ais_forwarder.service
```

## Configuration

### Configuration Parameters

All configuration values can be set via command line, environment variables, or config file:

| Parameter | Config File | Environment | Command Line | Default |
|-----------|-------------|-------------|--------------|---------|
| AIS IP | `ais_ip` | `AIS_IP` | `--ais-ip` | `192.168.50.37` |
| AIS Port | `ais_port` | `AIS_PORT` | `--ais-port` | `39150` |
| MarineTraffic IP | `mt_ip` | `MT_IP` | `--mt-ip` | `5.9.207.224` |
| MarineTraffic Port | `mt_port` | `MT_PORT` | `--mt-port` | `10170` |
| Notification User | `notification_user` | `NOTIFICATION_USER` | `--user` | `david` |

### View All Options

```bash
./ais_forwarder --help
```

### Service Configuration

The systemd service is configured with:
- **Restart Policy**: Automatic restart on failure
- **Restart Delay**: 30 seconds between restart attempts
- **Start Limit**: 5 attempts per 5-minute window
- **User**: Runs as `david` user for notification access
- **Environment**: `DISPLAY=:0` for desktop notifications

## Usage

### Service Management

```bash
# Start the service
sudo systemctl start ais_forwarder.service

# Stop the service
sudo systemctl stop ais_forwarder.service

# Restart the service
sudo systemctl restart ais_forwarder.service

# Check service status
systemctl status ais_forwarder.service

# Enable auto-start on boot
sudo systemctl enable ais_forwarder.service
```

### Monitoring

#### Real-time Logs
```bash
# All service logs
journalctl -u ais_forwarder.service -f

# Connection events only
journalctl -t ais_forwarder -f | grep "AIS Connection"
```

#### Connection Status
```bash
# Current service status
systemctl status ais_forwarder.service

# Recent log entries
journalctl -u ais_forwarder.service -n 20
```

## Notifications

The service provides notifications through multiple channels:

### Desktop Notifications
- **Connection Restored**: Normal priority notification when AIS reconnects
- **Connection Lost**: Critical priority notification when AIS disconnects
- **Service Started**: Normal priority notification on initial connection

### System Log Notifications
All notifications are also logged to syslog and can be viewed with:
```bash
journalctl -t ais_forwarder
```

## Connection Behavior

### Normal Operation
1. Service starts and attempts to connect to AIS transponder
2. On successful connection, sends "AIS Forwarder Started" notification
3. Continuously forwards NMEA sentences to MarineTraffic
4. Performs health checks every 5 seconds

### Connection Loss
1. Detects connection loss through health checks or read errors
2. Sends "AIS Connection Lost" notification (once)
3. Begins reconnection attempts every 10 seconds
4. No additional notifications during retry attempts

### Connection Recovery
1. Successfully reconnects to AIS transponder
2. Sends "AIS Connection Restored" notification
3. Resumes normal NMEA forwarding operation

## Technical Details

### Network Configuration
- **TCP Keepalive**: Enabled with aggressive settings for fast detection
  - Keepalive idle: 10 seconds
  - Keepalive interval: 5 seconds
  - Keepalive count: 3 attempts
- **Socket Timeouts**: 10-second timeouts on read/write operations
- **Health Checks**: Proactive connection testing every 5 seconds

### NMEA Processing
- Handles incomplete NMEA sentences across read boundaries
- Filters for AIS message types: `!AIVDM` and `!AIVDO`
- Preserves original NMEA sentence format for MarineTraffic

## Hardware Compatibility

Tested with:
- **Vesper XB-8000 AIS transponder**
- Should work with any AIS device that sends NMEA 0183 messages over TCP

## Troubleshooting

### Service Won't Start
```bash
# Check service status and logs
systemctl status ais_forwarder.service
journalctl -u ais_forwarder.service -n 50
```

### No Desktop Notifications
- Ensure you're logged into a GUI session
- Verify `libnotify-bin` and `dbus-x11` are installed
- Check that `DISPLAY=:0` is set in the service file

### Connection Issues
```bash
# Test AIS transponder connectivity
ping 192.168.50.37
telnet 192.168.50.37 39150

# Test MarineTraffic connectivity
ping 5.9.207.224
```

### High CPU Usage
- Check for rapid reconnection attempts in logs
- Verify AIS transponder is stable and reachable

## Development

### Building from Source
```bash
cd build
make clean
make
```

### Adding Features
The code is structured with clear separation:
- Connection management functions
- Notification system
- NMEA processing
- Main service loop

### Testing
Test connection handling by powering the AIS transponder on/off to verify:
- Fast connection loss detection
- Proper notification behavior
- Successful reconnection

## Migration from Python Version

This C++ version replaces the original Python script with significant improvements:
- Much faster connection failure detection
- Robust automatic reconnection
- System notification integration
- Better resource efficiency
- Enhanced reliability for 24/7 operation

## License

MIT License - see header in `src/ais_forwarder.cpp`

## Author

Copyright (c) 2025 David Hoy  
Email: david@thehoys.com  
S/V Rising Sun  
Catalina 470, hull #17

---

**Note**: You will need to obtain your own IP address and port number from MarineTraffic.com and modify the appropriate constants in the source code. DO NOT use the default MarineTraffic endpoint without permission.
