# UGREEN LED Controller - C++ Monitors

This directory contains high-performance C++ implementations of monitoring scripts for UGREEN NAS devices with LED control.

## Executables

### ugreen_leds_cli
The original LED controller command-line interface for direct LED manipulation.

### ugreen_zfs_monitor  
ZFS pool and disk monitoring with LED indicators:
- Monitors ZFS pool health status
- Tracks scrub and resilver operations  
- Maps individual disks to LED positions
- Supports multiple pool configurations
- Provides detailed pool and vdev status

### ugreen_monitor
General network and disk health monitoring:
- Network interface monitoring (bridge-aware for TrueNAS SCALE)
- S.M.A.R.T. disk health monitoring
- Connectivity testing with ping
- Comprehensive configuration options
- LED status indicators for all components

## Building

```bash
make clean      # Clean previous builds
make all        # Build all executables
make ugreen_monitor     # Build only general monitor
make ugreen_zfs_monitor # Build only ZFS monitor
```

## Usage

### ZFS Monitor
```bash
# Monitor ZFS pools continuously
sudo ./ugreen_zfs_monitor

# Run single check
sudo ./ugreen_zfs_monitor -t

# Use custom config
sudo ./ugreen_zfs_monitor -c /path/to/config.conf

# Monitor specific pools only
sudo ./ugreen_zfs_monitor -p "pool1 pool2"
```

### General Monitor
```bash
# Monitor network and disks
sudo ./ugreen_monitor

# Network monitoring only
sudo ./ugreen_monitor -n

# Disk monitoring only  
sudo ./ugreen_monitor -d

# Single test run
sudo ./ugreen_monitor -t

# Custom interval (60 seconds)
sudo ./ugreen_monitor -i 60

# Show status
sudo ./ugreen_monitor -s
```

## Configuration

### ZFS Monitor Config
Default location: `/etc/ugreen-zfs-monitor.conf`
- Pool monitoring settings
- LED color schemes for pool states
- Disk mapping methods (ATA, HCTL, Serial)
- Monitoring intervals

### General Monitor Config  
Default location: `/etc/ugreen-monitor.conf`
- Network interface settings
- S.M.A.R.T. monitoring options
- LED color configuration
- Ping connectivity testing

## Requirements

- **Root privileges**: Required for I2C device access (`sudo`)
- **i2c-dev module**: Kernel module for I2C communication
- **smartmontools**: For S.M.A.R.T. disk monitoring (`smartctl`)
- **ZFS tools**: For ZFS monitoring (zpool, zfs commands)
- **Compatible Hardware**: UGREEN DX/DXP series NAS devices

## LED Mapping

The monitors support up to 8 disk LEDs plus power and network LEDs:
- `power`: System power status LED
- `netdev`: Network status LED  
- `disk1` - `disk8`: Individual disk status LEDs

LED colors indicate:
- **Green**: Healthy/Online
- **Yellow**: Warning/Degraded  
- **Red**: Critical/Faulted
- **Blue**: Offline/Unknown
- **Off**: Disabled/No disk

## Architecture

Both C++ monitors are built with:
- **Object-oriented design**: Modular component architecture
- **Error handling**: Comprehensive exception and error management
- **Configuration management**: Flexible config file support
- **Signal handling**: Graceful shutdown on SIGINT/SIGTERM
- **Static linking**: Self-contained executables
- **C++17 features**: Modern C++ with std::optional and std::filesystem

## Integration

These C++ monitors are designed to replace the bash script versions with:
- Better performance and lower resource usage
- More robust error handling
- Enhanced configuration options
- Improved logging and diagnostics
- Professional-grade code structure

They maintain full compatibility with the existing LED controller system and configuration patterns.