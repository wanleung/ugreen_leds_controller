# UGREEN Monitor Script

## Overview

The `ugreen-monitor` script provides comprehensive network and disk health monitoring for UGREEN NAS devices, updating LED indicators based on system status using `ugreen_leds_cli`.

## Features

- **Network Monitoring**: Monitors network interface status and connectivity
- **Disk S.M.A.R.T. Monitoring**: Checks disk health using S.M.A.R.T. attributes  
- **LED Status Indicators**: Visual feedback through colored LEDs
- **Configurable**: Extensive configuration options
- **Auto-detection**: Automatic network interface and disk detection
- **Systemd Integration**: Can run as a system service

## LED Color Scheme

| Color  | Status    | Meaning |
|--------|-----------|---------|
| 🟢 Green | Healthy | All systems normal |
| 🟡 Yellow | Warning | Minor issues or degraded performance |
| 🔴 Red | Critical | Failure or critical condition |
| 🔵 Blue | Offline | Unknown status or device not responding |
| ⚫ Off | Disabled | No device present or monitoring disabled |

## Requirements

- Root privileges (sudo)
- `ugreen_leds_cli` built and available in PATH
- `smartmontools` package for disk monitoring (`apt install smartmontools`)
- `i2c-dev` kernel module loaded (`modprobe i2c-dev`)

## Installation

1. **Copy the script to system directory:**
   ```bash
   sudo cp ugreen-monitor /usr/bin/
   sudo chmod +x /usr/bin/ugreen-monitor
   ```

2. **Copy the configuration file:**
   ```bash
   sudo cp ugreen-monitor.conf /etc/
   ```

3. **Install systemd service (optional):**
   ```bash
   sudo cp systemd/ugreen-monitor.service /etc/systemd/system/
   sudo systemctl daemon-reload
   sudo systemctl enable ugreen-monitor
   sudo systemctl start ugreen-monitor
   ```

## Usage

### Manual Execution

```bash
# Run with default settings
sudo ugreen-monitor

# Run a single test cycle
sudo ugreen-monitor --test

# Monitor only network
sudo ugreen-monitor --network-only

# Monitor only disks  
sudo ugreen-monitor --disks-only

# Custom monitoring interval
sudo ugreen-monitor --interval 60

# Show help
ugreen-monitor --help
```

### Service Management

```bash
# Start the service
sudo systemctl start ugreen-monitor

# Stop the service
sudo systemctl stop ugreen-monitor

# Check service status
sudo systemctl status ugreen-monitor

# View logs
sudo journalctl -u ugreen-monitor -f
```

## Configuration

Edit `/etc/ugreen-monitor.conf` to customize behavior:

### Key Settings

```bash
# Monitoring intervals and features
MONITOR_INTERVAL=30        # Seconds between checks
MONITOR_NETWORK=true       # Enable network monitoring
MONITOR_DISKS=true         # Enable disk S.M.A.R.T. monitoring

# Network settings
NETWORK_INTERFACES="eth0"  # Specific interfaces (auto-detect if empty)
PING_TARGET="8.8.8.8"     # Connectivity test target

# LED colors (R G B format, 0-255)
COLOR_HEALTHY="0 255 0"    # Green for healthy
COLOR_WARNING="255 255 0"  # Yellow for warnings
COLOR_CRITICAL="255 0 0"   # Red for critical
COLOR_OFFLINE="0 0 255"    # Blue for offline/unknown
```

## Network Monitoring

The script monitors network interfaces and connectivity:

- **Green**: All monitored interfaces are up with internet connectivity
- **Yellow**: Some interfaces down but connectivity exists, or partial issues
- **Red**: All interfaces down or no internet connectivity
- **Blue**: Cannot determine network status

### Auto-Detection
By default, the script automatically detects active network interfaces, excluding loopback, Docker, and virtual interfaces.

## Disk Monitoring

Monitors disk health using S.M.A.R.T. attributes:

- **Green**: S.M.A.R.T. status PASSED, no warnings
- **Yellow**: Errors in logs or self-test failures
- **Red**: Disk failing or prefail attributes below threshold
- **Blue**: S.M.A.R.T. unavailable or device access issues
- **Off**: No disk detected in slot

### Disk Mapping

The script uses the same disk-to-LED mapping as `ugreen-diskiomon`:

- **ata**: Maps by ATA port (default, same as UGOS)
- **hctl**: Maps by SCSI HCTL address
- **serial**: Maps by disk serial number (most reliable)

Device-specific mappings are automatically applied for known UGREEN models.

## Troubleshooting

### Common Issues

1. **"Cannot communicate with LED controller"**
   - Ensure running as root: `sudo ugreen-monitor`
   - Load i2c-dev module: `sudo modprobe i2c-dev`
   - Unload led-ugreen kernel module if loaded

2. **"smartctl not found"**
   - Install smartmontools: `sudo apt install smartmontools`

3. **Network monitoring not working**
   - Check interface names: `ip link show`
   - Verify connectivity: `ping 8.8.8.8`
   - Review configuration in `/etc/ugreen-monitor.conf`

4. **Disk LEDs not updating**
   - Check disk mapping: `lsblk -S`
   - Verify S.M.A.R.T. support: `sudo smartctl -i /dev/sdX`
   - Try different mapping method in config

### Debug Mode

Run a single test cycle to debug issues:
```bash
sudo ugreen-monitor --test
```

Check service logs:
```bash
sudo journalctl -u ugreen-monitor -f
```

## Integration with Existing Scripts

This script can run alongside other UGREEN monitoring scripts, but be aware:

- Only one script should control the network LED at a time
- The `led-ugreen` kernel module conflicts with `ugreen_leds_cli`
- Consider stopping other LED services when using this monitor

## License

GPL-2.0-only (consistent with the ugreen_leds_controller project)