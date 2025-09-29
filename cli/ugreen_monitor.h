#ifndef UGREEN_MONITOR_H
#define UGREEN_MONITOR_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>

#include "zfs_monitor.h"  // For LedColor and DiskMapper
#include "ugreen_leds.h"  // For LED control

// Forward declarations
// (LedColor and DiskMapper now included from zfs_monitor.h)

// Network Interface Status
enum class NetworkStatus {
    HEALTHY = 0,        // All interfaces up with connectivity
    WARNING = 1,        // Some issues but partial connectivity
    CRITICAL = 2,       // All interfaces down
    UNKNOWN = 3         // Cannot determine status
};

// S.M.A.R.T. Status
enum class SmartStatus {
    HEALTHY = 0,        // S.M.A.R.T. PASSED
    WARNING = 1,        // Errors in logs or self-test
    CRITICAL = 2,       // Disk failing or prefail attributes
    UNAVAILABLE = 3,    // Cannot determine S.M.A.R.T. status
    DEVICE_NOT_FOUND = 4 // Device not found
};

// Network Interface Information
struct NetworkInterface {
    std::string name;
    bool is_up;
    bool is_bridge;
    std::string status;
    uint64_t speed;
};

// S.M.A.R.T. Disk Information
struct SmartDiskInfo {
    std::string device_path;
    std::string device_name;
    SmartStatus status;
    std::string health_status;
    uint64_t temperature;
    uint64_t reallocated_sectors;
    uint64_t pending_sectors;
    std::string model;
    std::string serial;
};

// General Monitor Configuration
struct UgreenMonitorConfig {
    std::string ugreen_leds_cli_path;
    int monitor_interval;
    bool monitor_network;
    bool monitor_disks;
    bool turn_off_leds_on_exit;
    
    // Network monitoring configuration
    std::vector<std::string> network_interfaces;  // Empty = auto-detect
    std::string network_led;
    std::string ping_target;
    int ping_count;
    int ping_timeout;
    
    // Disk mapping configuration
    std::string mapping_method;
    std::vector<std::string> serial_map;
    
    // LED Colors
    LedColor color_healthy;
    LedColor color_warning;
    LedColor color_critical;
    LedColor color_offline;
    LedColor color_disabled;
    
    // Default constructor
    UgreenMonitorConfig();
};

// Network Monitor Class
class NetworkMonitor {
public:
    NetworkMonitor();
    ~NetworkMonitor();
    
    // Configuration
    void setNetworkInterfaces(const std::vector<std::string>& interfaces);
    void setPingTarget(const std::string& target, int count = 1, int timeout = 3);
    
    // Network monitoring
    std::vector<NetworkInterface> getNetworkInterfaces();
    NetworkStatus checkNetworkStatus();
    bool testConnectivity(const std::string& target, int count, int timeout);
    
private:
    std::vector<std::string> configured_interfaces_;
    std::string ping_target_;
    int ping_count_;
    int ping_timeout_;
    
    std::string runCommand(const std::string& command);
    std::vector<std::string> autoDetectInterfaces();
    bool isInterfaceUp(const std::string& interface);
    bool isBridgeInterface(const std::string& interface);
    uint64_t getInterfaceSpeed(const std::string& interface);
};

// S.M.A.R.T. Monitor Class
class SmartMonitor {
public:
    SmartMonitor();
    ~SmartMonitor();
    
    // S.M.A.R.T. monitoring
    SmartDiskInfo getSmartInfo(const std::string& device_path);
    SmartStatus checkSmartStatus(const std::string& device_path);
    bool isSmartAvailable();
    
private:
    bool smart_available_;
    
    std::string runCommand(const std::string& command);
    void parseSmartOutput(const std::string& output, SmartDiskInfo& info);
    SmartStatus interpretExitCode(int exit_code);
};

// Main UGREEN Monitor Class
class UgreenMonitor {
public:
    UgreenMonitor();
    ~UgreenMonitor();
    
    // Configuration
    bool loadConfig(const std::string& config_file = "");
    void setConfig(const UgreenMonitorConfig& config);
    UgreenMonitorConfig getConfig() const;
    
    // Initialization
    bool initialize();
    void cleanup();
    
    // Monitoring functions
    bool runSingleCheck();
    void startMonitoring();
    void stopMonitoring();
    
    // LED control
    bool updateLed(const std::string& led_name, const LedColor& color, uint8_t brightness = 255);
    bool turnOffAllLeds();
    
    // Status and utilities
    void showHelp() const;
    void showStatus() const;
    
private:
    UgreenMonitorConfig config_;
    std::unique_ptr<NetworkMonitor> network_monitor_;
    std::unique_ptr<SmartMonitor> smart_monitor_;
    std::unique_ptr<DiskMapper> disk_mapper_;
    std::unique_ptr<ugreen_leds_t> led_controller_;
    
    bool running_;
    bool led_available_;
    
    std::vector<std::string> led_names_;
    
    // Monitoring functions
    void monitorNetwork();
    void monitorDisks();
    
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

// Command Executor Utility
class CommandExecutor {
public:
    static std::string execute(const std::string& command);
    static int executeWithExitCode(const std::string& command, std::string& output);
    static bool commandExists(const std::string& command);
    
private:
    static std::string runCommand(const std::string& command, int* exit_code = nullptr);
};

// System Utility Functions
namespace SystemUtils {
    bool isRoot();
    bool moduleLoaded(const std::string& module);
    bool loadModule(const std::string& module);
    bool fileExists(const std::string& path);
    bool deviceExists(const std::string& device);
    std::string getCurrentTimestamp();
    std::vector<std::string> listBlockDevices();
    std::string getDeviceModel(const std::string& device);
    std::string getDeviceSerial(const std::string& device);
}

// String Utility Functions
namespace StringUtils {
    std::vector<std::string> split(const std::string& str, char delimiter);
    std::string trim(const std::string& str);
    std::string toLower(const std::string& str);
    bool startsWith(const std::string& str, const std::string& prefix);
    bool endsWith(const std::string& str, const std::string& suffix);
    bool contains(const std::string& str, const std::string& substring);
}

#endif // UGREEN_MONITOR_H