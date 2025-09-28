# UGREEN ZFS Monitor Script

## Overview

The `ugreen-zfs-monitor` script provides comprehensive ZFS pool and disk monitoring for UGREEN NAS devices, specifically designed for TrueNAS SCALE and other ZFS-based systems. It monitors pool health, scrub operations, resilver operations, and individual disk status within ZFS pools.

## Features

- **ZFS Pool Health Monitoring**: Monitors ONLINE, DEGRADED, FAULTED, and UNAVAIL states
- **Scrub Operation Tracking**: Visual indication of scrub progress and completion status
- **Resilver Operation Monitoring**: Shows resilver progress with distinct LED colors
- **Individual Disk Status**: Monitors each disk's status within ZFS pools
- **Error Detection**: Identifies scrub errors and pool issues
- **LED Status Indicators**: Comprehensive visual feedback through colored LEDs
- **TrueNAS SCALE Integration**: Designed specifically for ZFS environments

## ZFS-Specific LED Color Scheme

### Pool Status LED (Power LED)
| Color | Status | Meaning |
|-------|--------|---------|
| 🟢 Green | ONLINE | All pools healthy |
| 🟡 Yellow | DEGRADED | Pool(s) degraded but functional |
| 🔴 Red | FAULTED | Pool(s) in critical state |
| 🔵 Blue | UNAVAIL | Pool(s) not imported or unavailable |
| 🟠 Orange | SCRUB_ACTIVE | Scrub operation in progress |
| 🟣 Purple | SCRUB_ERRORS | Scrub completed but found errors |
| 🔷 Cyan | RESILVER | Resilver operation in progress |

### Individual Disk LEDs (disk1-disk8)
| Color | Status | Meaning |
|-------|--------|---------|
| 🟢 Green | ONLINE | Disk healthy in pool |
| 🟡 Yellow | DEGRADED | Disk degraded but functioning |
| 🔴 Red | FAULTED | Disk failed or offline |
| 🔵 Blue | NO_POOL | Disk not part of any ZFS pool |
| ⚫ Gray | OFFLINE | Disk not detected |

### Scrub/Resilver LED (Network LED)
| Color | Status | Meaning |
|-------|--------|---------|
| 🟠 Orange | SCRUB_ACTIVE | Scrub operation running |
| 🔷 Cyan | RESILVER | Resilver operation running |
| 🟣 Purple | SCRUB_ERRORS | Recent scrub found errors |
| 🟢 Green (Dim) | NORMAL | No operations running |

## Requirements

- Root privileges (sudo)
- `ugreen_leds_cli` built and available
- ZFS tools (`zfsutils-linux` package)
- Active ZFS pools
- TrueNAS SCALE or Linux system with ZFS

## Installation

### Quick Installation

1. **Make scripts executable:**
   ```bash
   chmod +x /home/wanleung/Projects/ugreen_leds_controller/scripts/ugreen-zfs-monitor
   ```

2. **Test the ZFS monitor:**
   ```bash
   sudo /home/wanleung/Projects/ugreen_leds_controller/scripts/ugreen-zfs-monitor --test
   ```

### System Installation

1. **Copy files to system directories:**
   ```bash
   sudo cp ugreen-zfs-monitor /usr/local/bin/
   sudo chmod +x /usr/local/bin/ugreen-zfs-monitor
   sudo cp ugreen-zfs-monitor.conf /etc/
   ```

2. **Install systemd service:**
   ```bash
   sudo cp systemd/ugreen-zfs-monitor.service /etc/systemd/system/
   sudo systemctl daemon-reload
   ```

3. **Enable and start service:**
   ```bash
   sudo systemctl enable ugreen-zfs-monitor
   sudo systemctl start ugreen-zfs-monitor
   ```

## Usage

### Manual Execution

```bash
# Run with default settings (60-second intervals)
sudo ugreen-zfs-monitor

# Run a single test cycle
sudo ugreen-zfs-monitor --test

# Monitor only pool status (not individual disks)
sudo ugreen-zfs-monitor --pools-only

# Monitor only individual disks
sudo ugreen-zfs-monitor --disks-only

# Custom monitoring interval (30 seconds)
sudo ugreen-zfs-monitor --interval 30

# Show help
ugreen-zfs-monitor --help
```

### Service Management

```bash
# Start the ZFS monitor service
sudo systemctl start ugreen-zfs-monitor

# Stop the service
sudo systemctl stop ugreen-zfs-monitor

# Check service status
sudo systemctl status ugreen-zfs-monitor

# View logs
sudo journalctl -u ugreen-zfs-monitor -f

# Check last 50 log entries
sudo journalctl -u ugreen-zfs-monitor -n 50
```

## Configuration

Edit `/etc/ugreen-zfs-monitor.conf` to customize behavior:

### Key Settings

```bash
# Monitoring intervals and features
MONITOR_INTERVAL=60            # Seconds between ZFS checks
MONITOR_ZFS_POOLS=true         # Enable pool health monitoring
MONITOR_ZFS_DISKS=true         # Enable individual disk monitoring
MONITOR_SCRUB_STATUS=true      # Enable scrub/resilver monitoring

# Pool selection
ZFS_POOLS=""                   # Auto-detect all pools (recommended)
# ZFS_POOLS="tank backup"      # Or specify specific pools

# LED assignments
POOL_STATUS_LED="power"        # Pool health indicator
NETWORK_LED="netdev"           # Scrub/resilver indicator

# Colors (R G B format, 0-255)
COLOR_ONLINE="0 255 0"         # Green for healthy
COLOR_DEGRADED="255 255 0"     # Yellow for degraded
COLOR_FAULTED="255 0 0"        # Red for faulted
COLOR_SCRUB_ACTIVE="255 128 0" # Orange for active scrub
```

## ZFS Monitoring Details

### Pool Health States

- **ONLINE**: Pool is healthy and all vdevs are functional
- **DEGRADED**: Pool is functional but with reduced redundancy
- **FAULTED**: Pool has critical errors and may be unusable
- **UNAVAIL**: Pool is not imported or inaccessible

### Scrub Operations

- **Active Scrub**: Orange LED indicates scrub in progress
- **Scrub with Errors**: Purple LED indicates scrub found correctable errors
- **Completed Scrub**: Green LED indicates successful completion

### Resilver Operations

- **Active Resilver**: Cyan LED indicates resilver in progress (rebuilding)
- **Completed Resilver**: Returns to normal pool status indication

## Integration Options

### Coexisting with Regular Monitor

You can run both monitors simultaneously:

```bash
# Edit ZFS monitor config
sudo nano /etc/ugreen-zfs-monitor.conf

# Set coexistence mode
COEXIST_WITH_REGULAR_MONITOR=true
COEXIST_POOL_LED="disk8"       # Use disk8 for pool status
COEXIST_SCRUB_LED="disk7"      # Use disk7 for scrub status
```

### Running Only ZFS Monitor

For ZFS-only monitoring, use the default configuration which uses:
- **Power LED**: Overall pool health
- **Network LED**: Scrub/resilver status  
- **Disk LEDs**: Individual disk status in pools

## Troubleshooting

### Common Issues

1. **"ZFS tools not found"**
   ```bash
   sudo apt install zfsutils-linux
   ```

2. **"Cannot communicate with LED controller"**
   - Same as regular monitor - ensure i2c-dev loaded and no conflicting modules

3. **"No ZFS pools found"**
   - Check pools are imported: `zpool list`
   - Import pools: `zpool import`

4. **Service won't start**
   - Check ZFS services are running: `systemctl status zfs-mount.service`
   - Verify pools are imported before service starts

### Debug Commands

```bash
# Check ZFS pools
zpool list
zpool status

# Test LED controller
sudo ugreen_leds_cli all -status

# Test ZFS monitor once
sudo ugreen-zfs-monitor --test

# Check service logs
sudo journalctl -u ugreen-zfs-monitor --since "1 hour ago"
```

### Service Dependencies

The systemd service waits for ZFS to be ready:
- Requires: `zfs-mount.service`
- After: `zfs-import.target`

## Example Scenarios

### Healthy System
- **Power LED**: Green (all pools online)
- **Network LED**: Dim green (no operations)
- **Disk LEDs**: Green (all disks online in pools)

### During Scrub
- **Power LED**: Orange (scrub active)
- **Network LED**: Orange (scrub progress)
- **Disk LEDs**: Green (disks healthy)

### Degraded Pool
- **Power LED**: Yellow (pool degraded)
- **Network LED**: Dim green (no operations)
- **Disk LEDs**: Mix of green (good) and red (failed disk)

### Pool Failure
- **Power LED**: Red (pool faulted)
- **Network LED**: May show resilver (cyan) if rebuilding
- **Disk LEDs**: Red for failed disks, green for remaining

## Performance Notes

- ZFS monitoring is less CPU-intensive than S.M.A.R.T. monitoring
- Default 60-second interval is appropriate for most setups
- Scrub detection is immediate when operations start/complete
- Pool status changes are detected within one monitoring cycle

## License

GPL-2.0-only (consistent with the ugreen_leds_controller project)