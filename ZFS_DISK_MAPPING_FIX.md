# ZFS Disk Identifier Resolution Fix

## Problem
The original ZFS monitors (both bash and C++) failed to correctly map ZFS pool disk identifiers to physical disk positions because:

1. **`zpool status` shows disks using various identifiers**:
   - UUIDs: `123e4567-e89b-12d3-a456-426614174000`
   - by-id links: `ata-SAMSUNG_SSD_980_1TB_S12345ABCD-part1`
   - WWN identifiers: `wwn-0x1234567890abcdef`
   - Partition names: `sda1`, `sdb2`, etc.

2. **Disk position detection maps to `/dev/sd*` devices**:
   - LED position mapping finds physical devices like `/dev/sda`, `/dev/sdb`
   - But ZFS pools often reference these disks by their stable identifiers

3. **Result**: Disk LEDs showed incorrect status because the monitors couldn't correlate ZFS disk identifiers with physical disk positions.

## Solution

### Bash Script Improvements (`scripts/ugreen-zfs-monitor`)

Added comprehensive disk identifier resolution functions:

1. **`resolve_zfs_disk_identifier()`** - Converts ZFS identifiers back to `/dev/sd*` devices
2. **`find_zfs_identifiers_for_device()`** - Finds all possible ZFS identifiers for a physical device  
3. **Enhanced `check_disk_zfs_status()`** - Uses multiple identifiers to search ZFS pool status

### C++ Implementation Improvements (`cli/zfs_monitor.cpp`)

1. **`getDeviceIdentifiers()`** - C++ version of identifier resolution
2. **Enhanced `checkDiskZfsStatus()`** - Improved parsing logic for ZFS status output
3. **Robust line parsing** - Better handling of ZFS status format variations

## Key Features

### Identifier Resolution
Both versions now handle:
- `/dev/sd*` device names (sda, sdb, etc.)
- by-id symlinks (`/dev/disk/by-id/ata-*`, `wwn-*`, `scsi-*`)
- by-uuid symlinks (`/dev/disk/by-uuid/*`)
- Partition identifiers (sda1, sdb2, etc.)
- Full device paths

### ZFS Status Parsing
- Searches ZFS pool status for any matching identifier
- Extracts status from the correct column in `zpool status` output
- Handles various ZFS status formats and edge cases

### Error Handling
- Graceful handling of missing devices
- Proper fallback when identifiers can't be resolved
- Detailed logging for troubleshooting

## Result

The monitors now correctly:
1. **Map physical disk positions** (LED 1-8) to actual `/dev/sd*` devices
2. **Resolve ZFS identifiers** in pool status back to physical devices  
3. **Display accurate LED status** based on actual disk health in ZFS pools
4. **Work with any ZFS configuration** regardless of identifier type used

## Example

Before fix:
```
Disk 1 (disk1): Not in pool - /dev/sda
Disk 2 (disk2): Not in pool - /dev/sdb
```
(Even though sda/sdb are actually in ZFS pools but shown as UUIDs)

After fix:
```
Disk 1 (disk1): Online - /dev/sda
Disk 2 (disk2): Online - /dev/sdb  
```
(Correctly identifies that /dev/sda is in pool as ata-SAMSUNG_... identifier)

This resolves the core issue where ZFS pools using UUID or by-id identifiers couldn't be properly correlated with physical disk LED positions.