# Changelog

## Version 2.0 - Parameterized Configuration (2025-09-03)

### Added
- Full configuration support via command line, environment variables, and config files
- Configuration priority: Command line > Environment > Config file > Defaults
- New command line options: `--ais-ip`, `--ais-port`, `--mt-ip`, `--mt-port`, `--user`, `--config`, `--help`
- Environment variable support: `AIS_IP`, `AIS_PORT`, `MT_IP`, `MT_PORT`, `NOTIFICATION_USER`
- Config file support with key=value format
- Configuration logging on startup
- Sample configuration file (`ais_forwarder.conf`)

### Changed
- Removed hardcoded IP addresses and ports from source code
- Updated README.md with comprehensive configuration documentation

### Technical Details
- Added `Config` struct to centralize all configuration
- Implemented `load_config_file()`, `load_env_config()`, and command line parsing
- Updated `connect_to_ais()` and `send_notification()` to use config object
- Configuration validation and error handling

## Version 1.0 - Robust Connection Management (2025-09-03)

### Added
- Automatic reconnection logic (no exit on connection failure)
- Desktop notifications via notify-send
- System log notifications via syslog
- Connection health monitoring with TCP keepalive
- Socket timeouts and health checks
- Smart notification logic (only on state changes)
- Comprehensive systemd service configuration
- Restart delays and policies

### Features
- Robust TCP connection handling
- NMEA sentence filtering (AIS messages only)
- UDP forwarding to MarineTraffic
- Real-time connection status monitoring
- Notification management (no spam)

### Technical Details
- TCP keepalive with aggressive settings
- Socket select() with timeout for health checks
- Graceful error handling and recovery
- Proper systemd integration
- User-based notification delivery
