#!/bin/bash

# Install script for UGREEN ZFS Monitor (C++ version)
# This script installs the C++ ZFS monitor with systemd service

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CLI_DIR="$(dirname "$SCRIPT_DIR")/cli"

echo "=== Installing UGREEN ZFS Monitor (C++ version) ==="

# Check if running as root
if [[ $EUID -ne 0 ]]; then
    echo "Error: This script must be run as root (use sudo)"
    exit 1
fi

# Build the C++ program if needed
if [[ ! -f "$CLI_DIR/ugreen_zfs_monitor" ]]; then
    echo "Building C++ ZFS monitor..."
    cd "$CLI_DIR"
    make ugreen_zfs_monitor
    cd - > /dev/null
fi

# Install the binary
echo "Installing ugreen_zfs_monitor binary..."
cp "$CLI_DIR/ugreen_zfs_monitor" /usr/local/bin/
chmod +x /usr/local/bin/ugreen_zfs_monitor

# Install config file
echo "Installing configuration file..."
if [[ ! -f /etc/ugreen-zfs-monitor.conf ]]; then
    # Copy from scripts directory, falling back to cli directory
    if [[ -f "$SCRIPT_DIR/ugreen-zfs-monitor.conf" ]]; then
        cp "$SCRIPT_DIR/ugreen-zfs-monitor.conf" /etc/
    elif [[ -f "$CLI_DIR/ugreen-zfs-monitor.conf" ]]; then
        cp "$CLI_DIR/ugreen-zfs-monitor.conf" /etc/
    else
        echo "Warning: No config file found, creating basic one..."
        cat > /etc/ugreen-zfs-monitor.conf << 'EOF'
# UGREEN ZFS Monitor Configuration

# Path to ugreen_leds_cli
UGREEN_LEDS_CLI="/usr/local/bin/ugreen_leds_cli"

# Monitor interval in seconds
MONITOR_INTERVAL=60

# Enable/disable different monitoring features
MONITOR_ZFS_POOLS=true
MONITOR_ZFS_DISKS=true
MONITOR_SCRUB_STATUS=true

# ZFS pools to monitor (empty = all pools)
ZFS_POOLS=""

# LED colors (RGB format: "R G B")
COLOR_ONLINE="0 255 0"          # Green - pool healthy
COLOR_DEGRADED="255 255 0"      # Yellow - pool degraded
COLOR_FAULTED="255 0 0"         # Red - pool faulted/critical
COLOR_SCRUB_ACTIVE="0 0 255"    # Blue - scrub in progress
COLOR_RESILVER="255 0 255"      # Magenta - resilver in progress
COLOR_SCRUB_ERRORS="255 128 0"  # Orange - scrub found errors
COLOR_UNKNOWN="64 64 64"        # Gray - unknown status
EOF
    fi
else
    echo "Config file already exists at /etc/ugreen-zfs-monitor.conf"
fi

# Install systemd service
echo "Installing systemd service..."
cp "$SCRIPT_DIR/systemd/ugreen-zfs-monitor-cpp.service" /etc/systemd/system/

# Reload systemd and enable service
echo "Enabling systemd service..."
systemctl daemon-reload
systemctl enable ugreen-zfs-monitor-cpp.service

# Offer to start the service
echo ""
echo "Installation complete!"
echo ""
echo "Configuration file: /etc/ugreen-zfs-monitor.conf"
echo "Service file: /etc/systemd/system/ugreen-zfs-monitor-cpp.service"
echo ""
echo "To start the service now:"
echo "  sudo systemctl start ugreen-zfs-monitor-cpp"
echo ""
echo "To check service status:"
echo "  sudo systemctl status ugreen-zfs-monitor-cpp"
echo ""
echo "To view logs:"
echo "  sudo journalctl -u ugreen-zfs-monitor-cpp -f"
echo ""
echo "To test without service:"
echo "  sudo /usr/local/bin/ugreen_zfs_monitor -t"