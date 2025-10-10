#!/bin/bash

echo "=== Manual GUID Mapping Test ==="

echo "1. Testing zdb on each device:"
for dev in /dev/sda /dev/sdb /dev/sdc /dev/sdd; do
    echo "Device: $dev"
    
    # Try to get ZFS labels
    if sudo zdb -l "$dev" 2>/dev/null >/dev/null; then
        echo "  ZFS labels found:"
        sudo zdb -l "$dev" 2>/dev/null | grep -E "(guid|vdev)" | head -5
    else
        echo "  No ZFS labels found"
    fi
    
    # Try alternative: check if device has ZFS signatures
    if sudo zdb -e "$dev" 2>/dev/null >/dev/null; then
        echo "  ZFS pool export data found"
    else
        echo "  No ZFS pool data"
    fi
    echo
done

echo "2. Checking if disks appear in pool import cache:"
sudo zpool import 2>/dev/null | grep -E "(pool:|id:|state:|/dev/)" | head -20

echo -e "\n3. Manual GUID extraction attempt:"
for dev in /dev/sda /dev/sdb /dev/sdc /dev/sdd; do
    echo "Checking $dev for ZFS GUID..."
    
    # Method 1: zdb -l
    guid1=$(sudo zdb -l "$dev" 2>/dev/null | grep "guid:" | head -1 | awk '{print $2}')
    
    # Method 2: hexdump looking for ZFS magic
    if [[ -b "$dev" ]]; then
        # Look for ZFS uberblock signature
        magic=$(sudo hexdump -C "$dev" 2>/dev/null | head -100 | grep -i "00bab10c" | head -1)
        if [[ -n "$magic" ]]; then
            echo "  Found ZFS magic signature"
        fi
    fi
    
    if [[ -n "$guid1" && "$guid1" != "0" ]]; then
        echo "  GUID (decimal): $guid1"
        
        # Convert to the format used in zpool status
        if [[ "$guid1" =~ ^[0-9]+$ ]]; then
            printf "  GUID (hex): %016llx\n" "$guid1" 2>/dev/null || echo "  GUID conversion failed"
        fi
    else
        echo "  No GUID found"
    fi
done