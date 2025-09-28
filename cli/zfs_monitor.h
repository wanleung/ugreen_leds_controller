#ifndef ZFS_MONITOR_H
#define ZFS_MONITOR_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>

// ZFS Pool Health Status
enum class ZfsPoolHealth {
    ONLINE = 0,
    DEGRADED = 1,
    FAULTED = 2,
    UNAVAIL = 3,
    SCRUB_ACTIVE = 5,
    RESILVER_ACTIVE = 6,
    SCRUB_ERRORS = 7,
    UNKNOWN = 8
};

// ZFS Disk Status
enum class ZfsDiskStatus {
    ONLINE = 0,
    DEGRADED = 1,
    FAULTED = 2,
    NOT_IN_POOL = 3,
    DEVICE_NOT_FOUND = 4,
    UNKNOWN = 5
};

// LED Color Structure
struct LedColor {
    uint8_t r, g, b;
    LedColor(uint8_t red = 0, uint8_t green = 0, uint8_t blue = 0) : r(red), g(green), b(blue) {}
};

// ZFS Pool Information
struct ZfsPoolInfo {
    std::string name;
    ZfsPoolHealth health;
    std::string scan_status;
    bool scrub_active;
    bool resilver_active;
    bool scrub_errors;
    uint64_t errors;
};

// ZFS Disk Information
struct ZfsDiskInfo {
    std::string device_path;
    std::string device_name;
    ZfsDiskStatus status;
    std::string pool_name;
    uint64_t errors;
};

// Configuration Structure
struct ZfsMonitorConfig {
    std::string ugreen_leds_cli_path;
    int monitor_interval;
    bool monitor_zfs_pools;
    bool monitor_zfs_disks;
    bool monitor_scrub_status;
    bool turn_off_leds_on_exit;
    
    std::vector<std::string> zfs_pools;
    std::string pool_status_led;
    std::string network_led;
    
    std::string mapping_method;
    std::vector<std::string> serial_map;
    
    // LED Colors
    LedColor color_online;
    LedColor color_degraded;
    LedColor color_faulted;
    LedColor color_unavail;
    LedColor color_scrub_active;
    LedColor color_resilver;
    LedColor color_scrub_progress;
    LedColor color_offline;
    
    // Default constructor with sensible defaults
    ZfsMonitorConfig();
};

// ZFS Command Executor Class
class ZfsCommandExecutor {
public:
    ZfsCommandExecutor();
    ~ZfsCommandExecutor();
    
    // Execute ZFS commands
    std::string executeCommand(const std::string& command);
    std::vector<std::string> getPoolList();
    ZfsPoolInfo getPoolInfo(const std::string& pool_name);
    std::string getPoolStatus(const std::string& pool_name);
    
private:
    bool isZfsAvailable();
    std::string runCommand(const std::string& command);
};

// Disk Mapping Class
class DiskMapper {
public:
    enum class MappingMethod {
        ATA,
        HCTL,
        SERIAL
    };
    
    DiskMapper(MappingMethod method = MappingMethod::ATA);
    ~DiskMapper();
    
    void setMappingMethod(MappingMethod method);
    void setSerialMap(const std::vector<std::string>& serials);
    std::string getDiskDevice(int disk_index);
    void detectUgreenModel();
    
private:
    MappingMethod mapping_method_;
    std::vector<std::string> ata_map_;
    std::vector<std::string> hctl_map_;
    std::vector<std::string> serial_map_;
    std::string detected_model_;
    
    void initializeDefaultMappings();
    void applyModelSpecificMappings();
    std::string runCommand(const std::string& command);
};

// Forward declaration
class ugreen_leds_t;

// Main ZFS Monitor Class
class ZfsMonitor {
public:
    ZfsMonitor();
    ~ZfsMonitor();
    
    // Configuration
    bool loadConfig(const std::string& config_file = "");
    void setConfig(const ZfsMonitorConfig& config);
    
    // Initialization
    bool initialize();
    void cleanup();
    
    // Monitoring functions
    bool runSingleCheck();
    void startMonitoring();
    void stopMonitoring();
    
    // Status checking
    ZfsPoolHealth checkPoolStatus(const std::string& pool_name);
    ZfsDiskStatus checkDiskZfsStatus(const std::string& device_path);
    
    // LED control
    bool updateLed(const std::string& led_name, const LedColor& color, uint8_t brightness = 255);
    bool turnOffAllLeds();
    
    // Utilities
    void showHelp() const;
    void showStatus() const;
    
private:
    ZfsMonitorConfig config_;
    std::unique_ptr<ZfsCommandExecutor> zfs_executor_;
    std::unique_ptr<DiskMapper> disk_mapper_;
    std::unique_ptr<ugreen_leds_t> led_controller_;
    
    bool running_;
    bool led_available_;
    
    std::vector<std::string> led_names_;
    
    // Monitoring functions
    void monitorZfsPools();
    void monitorZfsDisks();
    void monitorScrubResilver();
    
    // Utility functions
    bool initializeLedController();
    bool loadI2cModules();
    std::string colorToString(const LedColor& color) const;
    LedColor stringToColor(const std::string& color_str) const;
    void logMessage(const std::string& message) const;
    void logError(const std::string& error) const;
    
    // Configuration parsing
    void parseConfigLine(const std::string& line);
    void setDefaultConfig();
};

// Utility Functions
std::vector<std::string> splitString(const std::string& str, char delimiter);
std::string trimString(const std::string& str);
bool fileExists(const std::string& filename);
std::string getCurrentTimestamp();

#endif // ZFS_MONITOR_H