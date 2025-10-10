# Shell Compatibility Fixes for UGREEN Monitor Scripts

## Problem
The bash monitoring scripts were failing with errors like:
```
sh: 1: [[: not found
```

This occurred because the scripts were being executed with `sh` instead of `bash`, but they contain bash-specific constructs.

## Root Cause
The scripts use bash-specific features including:
- `[[` double brackets for conditional tests
- `=~` regex pattern matching operator
- Bash array operations
- Other bashisms not supported by POSIX sh

**C++ programs also had issues**: The C++ ZFS monitor was using `[[` constructs in shell commands executed via `popen()`.

## Solution Implemented

### 1. Updated Shebang
Changed from `#!/usr/bin/bash` to `#!/bin/bash` for better compatibility across different systems.

### 2. Added Bash Requirement Check
Added explicit check at the top of both scripts:
```bash
# Ensure this script is running with bash (not sh or other shells)
if [ -z "$BASH_VERSION" ]; then
    echo "Error: This script requires bash to run properly."
    echo "Please run with: bash $0 $*"
    exit 1
fi
```

### 3. Updated SystemD Service Files
Modified service files to explicitly use bash:
```ini
ExecStart=/bin/bash /usr/local/bin/ugreen-zfs-monitor
ExecStart=/bin/bash /usr/local/bin/ugreen-monitor
```

### 4. Fixed C++ Shell Commands
Fixed bash-specific constructs in C++ programs:
```cpp
// Changed from bash-specific [[
"if [[ \"$target\" == \"" + device_path + "\"* ]]; then "
// To POSIX-compatible case statement
"case \"$target\" in \"" + device_path + "\"*) "
```

### 5. Created C++ SystemD Services
Added dedicated service files for C++ versions:
- `scripts/systemd/ugreen-zfs-monitor-cpp.service`
- `scripts/systemd/ugreen-monitor-cpp.service`

### 6. Standardized Installation Paths
Updated all references to use `/usr/local/bin/` consistently for custom monitors.

## Files Modified

### C++ Source Code
- `cli/zfs_monitor.cpp` - Fixed bash-specific `[[` construct in shell command to use POSIX `case` statement

### Scripts
- `scripts/ugreen-zfs-monitor` - Added bash check, updated shebang
- `scripts/ugreen-monitor` - Added bash check, updated shebang

### SystemD Services
- `scripts/systemd/ugreen-zfs-monitor.service` - Force bash execution, fix path
- `scripts/systemd/ugreen-monitor.service` - Force bash execution, fix path
- `scripts/systemd/ugreen-zfs-monitor-cpp.service` - New service for C++ ZFS monitor
- `scripts/systemd/ugreen-monitor-cpp.service` - New service for C++ regular monitor

## Testing
After fixes:
- **Bash scripts**: Running with `bash ./ugreen-zfs-monitor` works correctly
- **Bash scripts**: Running with `sh ./ugreen-zfs-monitor` fails gracefully with helpful error message
- **C++ programs**: Both `ugreen_zfs_monitor` and `ugreen_monitor` work correctly without shell errors
- **SystemD services**: Will explicitly use bash for bash scripts, preventing shell compatibility issues

## Alternative Solutions Considered
1. **Converting to POSIX sh**: Would require extensive rewriting of regex patterns and conditional logic
2. **Using different shell detection**: Current solution is simpler and more reliable
3. **Multiple script versions**: Would increase maintenance burden

## Best Practices for Future Scripts
1. Always use `#!/bin/bash` shebang for bash scripts
2. Add bash requirement check for scripts using bashisms
3. Explicitly specify bash in SystemD service files for bash scripts
4. Test scripts with both `bash` and `sh` to catch compatibility issues early