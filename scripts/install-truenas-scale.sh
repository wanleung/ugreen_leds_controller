#!/bin/bash

# TrueNAS SCALE UGREEN LED Monitor Installation Script
# This script installs and configures the UGREEN LED monitoring system on TrueNAS SCALE

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if running as root
if [[ $EUID -ne 0 ]]; then
    print_error "This script must be run as root (use sudo)"
    exit 1
fi

# Verify we're on TrueNAS SCALE
if [[ ! -f /etc/debian_version ]]; then
    print_error "This script is designed for TrueNAS SCALE (Debian-based)"
    print_error "For TrueNAS CORE, use FreeBSD rc scripts instead"
    exit 1
fi

print_success "Running on TrueNAS SCALE (Debian $(cat /etc/debian_version))"

# Define paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "${SCRIPT_DIR}")"
CLI_DIR="${PROJECT_ROOT}/cli"

print_status "Project directory: ${PROJECT_ROOT}"

# Check if we have the source code
if [[ ! -d "${CLI_DIR}" ]]; then
    print_error "Cannot find CLI directory at ${CLI_DIR}"
    print_error "Please ensure you're running this from the ugreen_leds_controller project directory"
    exit 1
fi

# Install required packages
print_status "Installing required packages..."
apt update
apt install -y i2c-tools build-essential smartmontools bc

# Load I2C kernel module
print_status "Loading I2C kernel modules..."
modprobe i2c-dev || print_warning "Failed to load i2c-dev module"

# Make it persistent
if ! grep -q "i2c-dev" /etc/modules; then
    echo "i2c-dev" >> /etc/modules
    print_success "Added i2c-dev to /etc/modules for persistence"
fi

# Build ugreen_leds_cli if needed
print_status "Building ugreen_leds_cli..."
cd "${CLI_DIR}"
make clean && make

if [[ ! -f "${CLI_DIR}/ugreen_leds_cli" ]]; then
    print_error "Failed to build ugreen_leds_cli"
    exit 1
fi

print_success "Successfully built ugreen_leds_cli"

# Install binaries
print_status "Installing binaries..."
cp "${CLI_DIR}/ugreen_leds_cli" /usr/local/bin/
chmod +x /usr/local/bin/ugreen_leds_cli

cp "${SCRIPT_DIR}/ugreen-monitor" /usr/local/bin/
chmod +x /usr/local/bin/ugreen-monitor

print_success "Installed binaries to /usr/local/bin/"

# Install configuration
print_status "Installing configuration..."
cp "${SCRIPT_DIR}/ugreen-monitor.conf" /etc/
print_success "Installed configuration to /etc/ugreen-monitor.conf"

# Test LED controller access
print_status "Testing LED controller access..."
if /usr/local/bin/ugreen_leds_cli power > /dev/null 2>&1; then
    print_success "LED controller communication test passed!"
else
    print_warning "LED controller communication test failed"
    print_warning "This is normal if:"
    print_warning "  1. UGREEN device is not connected"
    print_warning "  2. led-ugreen kernel module is loaded (conflicts with CLI)"
    print_warning "  3. I2C device permissions need adjustment"
    
    # Check for conflicting kernel module
    if lsmod | grep -q led_ugreen; then
        print_warning "Found led_ugreen kernel module loaded - this conflicts with ugreen_leds_cli"
        print_status "Attempting to unload led_ugreen module..."
        modprobe -r led_ugreen || print_warning "Failed to unload led_ugreen module"
    fi
    
    # Show diagnostic info
    print_status "Diagnostic information:"
    echo "I2C adapters:"
    i2cdetect -l || print_warning "No I2C adapters found"
    echo ""
    echo "I2C bus 1 scan:"
    i2cdetect -y 1 || print_warning "Cannot scan I2C bus 1"
fi

# Install systemd service
print_status "Installing systemd service..."
cp "${SCRIPT_DIR}/systemd/ugreen-monitor.service" /etc/systemd/system/
systemctl daemon-reload
print_success "Installed systemd service"

# Create systemd service for i2c module loading
print_status "Creating I2C module loading service..."
cat > /etc/systemd/system/ugreen-i2c.service << 'EOF'
[Unit]
Description=Load I2C modules for UGREEN LED controller
Before=ugreen-monitor.service

[Service]
Type=oneshot
ExecStart=/sbin/modprobe i2c-dev
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
EOF

systemctl enable ugreen-i2c.service
print_success "Created I2C module loading service"

# Test the monitor script
print_status "Testing monitor script..."
if /usr/local/bin/ugreen-monitor --test; then
    print_success "Monitor script test completed successfully!"
else
    print_warning "Monitor script test encountered issues (this may be normal)"
fi

# Offer to start the service
echo ""
print_status "Installation completed!"
echo ""
echo "Next steps:"
echo "1. Review configuration: nano /etc/ugreen-monitor.conf"
echo "2. Test manually: sudo ugreen-monitor --test"
echo "3. Start service: sudo systemctl start ugreen-monitor"
echo "4. Enable at boot: sudo systemctl enable ugreen-monitor"
echo "5. Check status: sudo systemctl status ugreen-monitor"
echo "6. View logs: sudo journalctl -u ugreen-monitor -f"
echo ""

read -p "Would you like to start the ugreen-monitor service now? (y/N): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    systemctl enable ugreen-monitor
    systemctl start ugreen-monitor
    print_success "Service started and enabled!"
    echo ""
    systemctl status ugreen-monitor
else
    print_status "Service not started. You can start it later with:"
    print_status "sudo systemctl start ugreen-monitor"
fi

print_success "Installation complete!"