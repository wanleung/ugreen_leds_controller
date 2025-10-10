#!/bin/bash

echo "=== Debug ZFS Disk Mapping ==="

echo "1. ZFS Pool Status (raw):"
sudo zpool status 2>/dev/null

echo -e "\n2. Physical disks found:"
for disk in /dev/sd*; do
    if [[ -b "$disk" && ! "$disk" =~ [0-9]$ ]]; then
        echo "  $disk"
    fi
done

echo -e "\n3. ZFS identifiers from pool:"
sudo zpool status 2>/dev/null | grep -E "^\s+[a-f0-9-]{36}\s+" | awk '{print $1}' | while read uuid; do
    echo "  UUID: $uuid"
    
    # Check if this UUID exists in /dev/disk/by-uuid
    if [[ -L "/dev/disk/by-uuid/$uuid" ]]; then
        target=$(readlink -f "/dev/disk/by-uuid/$uuid")
        echo "    -> Maps to: $target"
    else
        echo "    -> No /dev/disk/by-uuid/$uuid link found"
        
        # Check if it's a ZFS dataset identifier
        echo "    -> Checking if it's a ZFS vdev..."
        sudo zdb -l /dev/sd* 2>/dev/null | grep -B5 -A5 "$uuid" || echo "    -> Not found in ZFS labels"
    fi
done

echo -e "\n4. by-uuid directory contents:"
ls -la /dev/disk/by-uuid/ 2>/dev/null | head -10

echo -e "\n5. by-id directory contents:"
ls -la /dev/disk/by-id/ 2>/dev/null | head -10