#!/bin/bash

echo "=== Advanced ZFS GUID to Device Mapping ==="

echo "1. ZFS vdev GUIDs from pool Data:"
sudo zpool status Data 2>/dev/null | grep -E "^\s+[a-f0-9-]{36}\s+" | awk '{print $1}' | while read guid; do
    echo "  GUID: $guid"
    
    # Method 1: Use zdb to find which device has this GUID
    for dev in /dev/sd*; do
        if [[ -b "$dev" && ! "$dev" =~ [0-9]$ ]]; then
            # Check if this device contains the GUID
            if sudo zdb -l "$dev" 2>/dev/null | grep -q "$guid"; then
                echo "    -> Found on device: $dev"
                break
            fi
        fi
    done
done

echo -e "\n2. Device to GUID mapping:"
for dev in /dev/sd*; do
    if [[ -b "$dev" && ! "$dev" =~ [0-9]$ ]]; then
        echo "  Device: $dev"
        # Get the vdev GUID from this device
        guid=$(sudo zdb -l "$dev" 2>/dev/null | grep -E "guid:\s*[0-9]+" | head -1 | awk '{print $2}')
        if [[ -n "$guid" ]]; then
            # Convert decimal GUID to hex format used in zpool status
            hex_guid=$(printf "%016x-%04x-%04x-%04x-%012x" \
                       $((guid >> 32)) \
                       $(((guid >> 16) & 0xFFFF)) \
                       $((guid & 0xFFFF)) \
                       $(((guid >> 48) & 0xFFFF)) \
                       $((guid & 0xFFFFFFFFFFFF)))
            echo "    -> GUID (dec): $guid"
            echo "    -> GUID (hex): $hex_guid"
        else
            echo "    -> No ZFS GUID found"
        fi
    fi
done

echo -e "\n3. Alternative method - check zpool labelclear output:"
sudo zpool status Data -v 2>/dev/null