#include "zfs_monitor.h"
#include "ugreen_leds.h"
#include <iomanip>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <thread>
#include <chrono>
#include <regex>

// Default Configuration Constructor
ZfsMonitorConfig::ZfsMonitorConfig() :
    ugreen_leds_cli_path("ugreen_leds_cli"),
    monitor_interval(60),
    monitor_zfs_pools(true),
    monitor_zfs_disks(true),
    monitor_scrub_status(true),
    turn_off_leds_on_exit(false),
    pool_status_led("power"),
    network_led("netdev"),
    mapping_method("ata"),
    color_online(0, 255, 0),        // Green
    color_degraded(255, 255, 0),    // Yellow
    color_faulted(255, 0, 0),       // Red
    color_unavail(0, 0, 255),       // Blue
    color_scrub_active(255, 128, 0), // Orange
    color_resilver(0, 255, 255),    // Cyan
    color_scrub_progress(128, 0, 255), // Purple
    color_offline(64, 64, 64)       // Gray
{
}

// ZFS Command Executor Implementation
ZfsCommandExecutor::ZfsCommandExecutor() {
    if (!isZfsAvailable()) {
        std::cerr << "Warning: ZFS tools not available" << std::endl;
    }
}

ZfsCommandExecutor::~ZfsCommandExecutor() {}

bool ZfsCommandExecutor::isZfsAvailable() {
    std::string result = runCommand("which zpool");
    return !result.empty();
}

std::string ZfsCommandExecutor::runCommand(const std::string& command) {
    std::string result;
    char buffer[128];
    
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return "";
    }
    
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    
    pclose(pipe);
    return result;
}

std::string ZfsCommandExecutor::executeCommand(const std::string& command) {
    return runCommand(command);
}

std::vector<std::string> ZfsCommandExecutor::getPoolList() {
    std::vector<std::string> pools;
    std::string output = runCommand("zpool list -H -o name 2>/dev/null");
    
    if (!output.empty()) {
        std::istringstream iss(output);
        std::string pool;
        while (std::getline(iss, pool)) {
            pool = trimString(pool);
            if (!pool.empty()) {
                pools.push_back(pool);
            }
        }
    }
    
    return pools;
}

ZfsPoolInfo ZfsCommandExecutor::getPoolInfo(const std::string& pool_name) {
    ZfsPoolInfo info;
    info.name = pool_name;
    info.health = ZfsPoolHealth::UNKNOWN;
    info.scrub_active = false;
    info.resilver_active = false;
    info.scrub_errors = false;
    info.errors = 0;
    
    // Get pool health
    std::string health_cmd = "zpool list -H -o health " + pool_name + " 2>/dev/null";
    std::string health_output = runCommand(health_cmd);
    health_output = trimString(health_output);
    
    if (health_output == "ONLINE") {
        info.health = ZfsPoolHealth::ONLINE;
    } else if (health_output == "DEGRADED") {
        info.health = ZfsPoolHealth::DEGRADED;
    } else if (health_output == "FAULTED") {
        info.health = ZfsPoolHealth::FAULTED;
    } else if (health_output == "UNAVAIL") {
        info.health = ZfsPoolHealth::UNAVAIL;
    }
    
    // Get detailed status
    std::string status_cmd = "zpool status " + pool_name + " 2>/dev/null";
    std::string status_output = runCommand(status_cmd);
    
    // Check for operations in progress
    if (status_output.find("scrub in progress") != std::string::npos) {
        info.scrub_active = true;
        info.health = ZfsPoolHealth::SCRUB_ACTIVE;
    } else if (status_output.find("resilver in progress") != std::string::npos) {
        info.resilver_active = true;
        info.health = ZfsPoolHealth::RESILVER_ACTIVE;
    } else if (status_output.find("scrub repaired") != std::string::npos && 
               status_output.find("with") != std::string::npos && 
               status_output.find("errors") != std::string::npos) {
        info.scrub_errors = true;
        if (info.health == ZfsPoolHealth::ONLINE) {
            info.health = ZfsPoolHealth::SCRUB_ERRORS;
        }
    }
    
    return info;
}

std::string ZfsCommandExecutor::getPoolStatus(const std::string& pool_name) {
    std::string cmd = "zpool status " + pool_name + " 2>/dev/null";
    return runCommand(cmd);
}

// Disk Mapper Implementation
DiskMapper::DiskMapper(MappingMethod method) : mapping_method_(method) {
    initializeDefaultMappings();
    detectUgreenModel();
}

DiskMapper::~DiskMapper() {}

void DiskMapper::initializeDefaultMappings() {
    ata_map_ = {"ata1", "ata2", "ata3", "ata4", "ata5", "ata6", "ata7", "ata8"};
    hctl_map_ = {"0:0:0:0", "1:0:0:0", "2:0:0:0", "3:0:0:0", "4:0:0:0", "5:0:0:0", "6:0:0:0", "7:0:0:0"};
}

void DiskMapper::detectUgreenModel() {
    std::string dmidecode_output = runCommand("dmidecode --string system-product-name 2>/dev/null");
    detected_model_ = trimString(dmidecode_output);
    
    if (!detected_model_.empty()) {
        applyModelSpecificMappings();
        std::cout << "Detected UGREEN " << detected_model_ << std::endl;
    }
}

void DiskMapper::applyModelSpecificMappings() {
    if (detected_model_.find("DXP6800") != std::string::npos) {
        hctl_map_ = {"2:0:0:0", "3:0:0:0", "4:0:0:0", "5:0:0:0", "0:0:0:0", "1:0:0:0"};
        ata_map_ = {"ata3", "ata4", "ata5", "ata6", "ata1", "ata2"};
    }
    // Add other model-specific mappings as needed
}

std::string DiskMapper::runCommand(const std::string& command) {
    std::string result;
    char buffer[128];
    
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return "";
    }
    
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    
    pclose(pipe);
    return result;
}

void DiskMapper::setMappingMethod(MappingMethod method) {
    mapping_method_ = method;
}

void DiskMapper::setSerialMap(const std::vector<std::string>& serials) {
    serial_map_ = serials;
}

std::string DiskMapper::getDiskDevice(int disk_index) {
    if (disk_index < 0 || disk_index >= 8) {
        return "";
    }
    
    std::string device;
    
    switch (mapping_method_) {
        case MappingMethod::ATA: {
            if (disk_index < static_cast<int>(ata_map_.size())) {
                std::string ata_name = ata_map_[disk_index];
                std::string cmd = "ls /sys/block/ 2>/dev/null | grep -E '^sd[a-z]+$'";
                std::string block_devices = runCommand(cmd);
                
                std::istringstream iss(block_devices);
                std::string dev;
                while (std::getline(iss, dev)) {
                    dev = trimString(dev);
                    std::string link_cmd = "readlink /sys/block/" + dev + " 2>/dev/null";
                    std::string link_output = runCommand(link_cmd);
                    if (link_output.find(ata_name) != std::string::npos) {
                        device = "/dev/" + dev;
                        break;
                    }
                }
            }
            break;
        }
        
        case MappingMethod::HCTL: {
            if (disk_index < static_cast<int>(hctl_map_.size())) {
                std::string hctl = hctl_map_[disk_index];
                std::string cmd = "lsblk -S -o HCTL,NAME 2>/dev/null | awk '$1==\"" + hctl + "\" {print \"/dev/\"$2}'";
                device = trimString(runCommand(cmd));
            }
            break;
        }
        
        case MappingMethod::SERIAL: {
            if (disk_index < static_cast<int>(serial_map_.size())) {
                std::string serial = serial_map_[disk_index];
                std::string cmd = "lsblk -S -o SERIAL,NAME 2>/dev/null | awk '$1==\"" + serial + "\" {print \"/dev/\"$2}'";
                device = trimString(runCommand(cmd));
            }
            break;
        }
    }
    
    return device;
}

// ZFS Monitor Implementation
ZfsMonitor::ZfsMonitor() : 
    running_(false), 
    led_available_(false),
    zfs_executor_(std::make_unique<ZfsCommandExecutor>()),
    disk_mapper_(std::make_unique<DiskMapper>()),
    led_controller_(nullptr)
{
    setDefaultConfig();
    led_names_ = {"disk1", "disk2", "disk3", "disk4", "disk5", "disk6", "disk7", "disk8"};
}

ZfsMonitor::~ZfsMonitor() {
    cleanup();
}

void ZfsMonitor::setDefaultConfig() {
    config_ = ZfsMonitorConfig();
}

bool ZfsMonitor::loadConfig(const std::string& config_file) {
    std::string config_path = config_file.empty() ? "/etc/ugreen-zfs-monitor.conf" : config_file;
    
    std::ifstream file(config_path);
    if (!file.is_open()) {
        std::cout << "Config file not found, using defaults: " << config_path << std::endl;
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        parseConfigLine(line);
    }
    
    file.close();
    return true;
}

void ZfsMonitor::parseConfigLine(const std::string& line) {
    std::string trimmed = trimString(line);
    if (trimmed.empty() || trimmed[0] == '#') {
        return;
    }
    
    size_t equals_pos = trimmed.find('=');
    if (equals_pos == std::string::npos) {
        return;
    }
    
    std::string key = trimString(trimmed.substr(0, equals_pos));
    std::string value = trimString(trimmed.substr(equals_pos + 1));
    
    // Remove quotes if present
    if (value.length() >= 2 && value.front() == '"' && value.back() == '"') {
        value = value.substr(1, value.length() - 2);
    }
    
    // Parse configuration values
    if (key == "UGREEN_LEDS_CLI") {
        config_.ugreen_leds_cli_path = value;
    } else if (key == "MONITOR_INTERVAL") {
        config_.monitor_interval = std::stoi(value);
    } else if (key == "MONITOR_ZFS_POOLS") {
        config_.monitor_zfs_pools = (value == "true");
    } else if (key == "MONITOR_ZFS_DISKS") {
        config_.monitor_zfs_disks = (value == "true");
    } else if (key == "MONITOR_SCRUB_STATUS") {
        config_.monitor_scrub_status = (value == "true");
    } else if (key == "ZFS_POOLS") {
        config_.zfs_pools = splitString(value, ' ');
    } else if (key == "MAPPING_METHOD") {
        config_.mapping_method = value;
    } else if (key == "COLOR_ONLINE") {
        config_.color_online = stringToColor(value);
    } else if (key == "COLOR_DEGRADED") {
        config_.color_degraded = stringToColor(value);
    } else if (key == "COLOR_FAULTED") {
        config_.color_faulted = stringToColor(value);
    }
    // Add more configuration parsing as needed
}

void ZfsMonitor::setConfig(const ZfsMonitorConfig& config) {
    config_ = config;
}

bool ZfsMonitor::initialize() {
    // Load I2C modules
    if (!loadI2cModules()) {
        std::cerr << "Warning: Failed to load I2C modules" << std::endl;
    }
    
    // Initialize LED controller
    if (!initializeLedController()) {
        std::cerr << "Warning: LED controller not available" << std::endl;
        led_available_ = false;
    } else {
        led_available_ = true;
        std::cout << "LED controller initialized successfully" << std::endl;
    }
    
    // Set up disk mapper
    if (config_.mapping_method == "ata") {
        disk_mapper_->setMappingMethod(DiskMapper::MappingMethod::ATA);
    } else if (config_.mapping_method == "hctl") {
        disk_mapper_->setMappingMethod(DiskMapper::MappingMethod::HCTL);
    } else if (config_.mapping_method == "serial") {
        disk_mapper_->setMappingMethod(DiskMapper::MappingMethod::SERIAL);
        disk_mapper_->setSerialMap(config_.serial_map);
    }
    
    return true;
}

bool ZfsMonitor::loadI2cModules() {
    int result = system("modprobe i2c-dev 2>/dev/null");
    return (result == 0);
}

bool ZfsMonitor::initializeLedController() {
    try {
        led_controller_ = std::make_unique<ugreen_leds_t>();
        return (led_controller_->start() == 0);
    } catch (const std::exception& e) {
        std::cerr << "Error initializing LED controller: " << e.what() << std::endl;
        return false;
    }
}

void ZfsMonitor::cleanup() {
    running_ = false;
    
    if (config_.turn_off_leds_on_exit && led_available_) {
        turnOffAllLeds();
    }
    
    if (led_controller_) {
        led_controller_.reset();
    }
}

bool ZfsMonitor::runSingleCheck() {
    std::cout << "=== ZFS Monitor check at " << getCurrentTimestamp() << " ===" << std::endl;
    
    if (config_.monitor_zfs_pools) {
        monitorZfsPools();
    }
    
    if (config_.monitor_zfs_disks) {
        monitorZfsDisks();
    }
    
    if (config_.monitor_scrub_status) {
        monitorScrubResilver();
    }
    
    std::cout << std::endl;
    return true;
}

void ZfsMonitor::startMonitoring() {
    running_ = true;
    
    std::cout << "Starting UGREEN ZFS monitoring..." << std::endl;
    std::cout << "Monitor interval: " << config_.monitor_interval << " seconds" << std::endl;
    std::cout << "Pool monitoring: " << (config_.monitor_zfs_pools ? "enabled" : "disabled") << std::endl;
    std::cout << "Disk monitoring: " << (config_.monitor_zfs_disks ? "enabled" : "disabled") << std::endl;
    std::cout << "Scrub monitoring: " << (config_.monitor_scrub_status ? "enabled" : "disabled") << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl << std::endl;
    
    while (running_) {
        runSingleCheck();
        
        std::cout << "Next check in " << config_.monitor_interval << " seconds" << std::endl;
        
        // Sleep with interruption check
        for (int i = 0; i < config_.monitor_interval && running_; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void ZfsMonitor::stopMonitoring() {
    running_ = false;
}

ZfsPoolHealth ZfsMonitor::checkPoolStatus(const std::string& pool_name) {
    ZfsPoolInfo info = zfs_executor_->getPoolInfo(pool_name);
    return info.health;
}

ZfsDiskStatus ZfsMonitor::checkDiskZfsStatus(const std::string& device_path) {
    if (device_path.empty()) {
        return ZfsDiskStatus::DEVICE_NOT_FOUND;
    }
    
    // Check if device exists
    if (access(device_path.c_str(), F_OK) != 0) {
        return ZfsDiskStatus::DEVICE_NOT_FOUND;
    }
    
    std::string device_name = device_path.substr(device_path.find_last_of('/') + 1);
    
    // Get list of pools and check if this disk is part of any
    std::vector<std::string> pools = zfs_executor_->getPoolList();
    
    for (const auto& pool : pools) {
        std::string status_output = zfs_executor_->getPoolStatus(pool);
        
        // Look for the device in the pool status
        if (status_output.find(device_name) != std::string::npos) {
            // Parse the status - this is simplified, real implementation would be more robust
            std::regex status_regex(device_name + R"(\s+(\w+))");
            std::smatch match;
            
            if (std::regex_search(status_output, match, status_regex)) {
                std::string status = match[1];
                if (status == "ONLINE") {
                    return ZfsDiskStatus::ONLINE;
                } else if (status == "DEGRADED") {
                    return ZfsDiskStatus::DEGRADED;
                } else if (status == "FAULTED" || status == "OFFLINE" || status == "UNAVAIL") {
                    return ZfsDiskStatus::FAULTED;
                }
            }
            
            // If we found the device but couldn't parse status, assume it's online
            return ZfsDiskStatus::ONLINE;
        }
    }
    
    // Device not found in any pool
    return ZfsDiskStatus::NOT_IN_POOL;
}

void ZfsMonitor::monitorZfsPools() {
    std::vector<std::string> pools;
    
    if (config_.zfs_pools.empty()) {
        pools = zfs_executor_->getPoolList();
    } else {
        pools = config_.zfs_pools;
    }
    
    if (pools.empty()) {
        std::cout << "No ZFS pools found to monitor" << std::endl;
        updateLed(config_.pool_status_led, config_.color_unavail);
        return;
    }
    
    ZfsPoolHealth overall_status = ZfsPoolHealth::ONLINE;
    std::vector<std::string> status_messages;
    
    for (const auto& pool : pools) {
        ZfsPoolInfo info = zfs_executor_->getPoolInfo(pool);
        
        switch (info.health) {
            case ZfsPoolHealth::ONLINE:
                status_messages.push_back(pool + ": ONLINE");
                break;
            case ZfsPoolHealth::DEGRADED:
                status_messages.push_back(pool + ": DEGRADED");
                if (static_cast<int>(overall_status) < static_cast<int>(ZfsPoolHealth::DEGRADED)) {
                    overall_status = ZfsPoolHealth::DEGRADED;
                }
                break;
            case ZfsPoolHealth::FAULTED:
                status_messages.push_back(pool + ": FAULTED");
                overall_status = ZfsPoolHealth::FAULTED;
                break;
            case ZfsPoolHealth::SCRUB_ACTIVE:
                status_messages.push_back(pool + ": SCRUB ACTIVE");
                if (static_cast<int>(overall_status) < static_cast<int>(ZfsPoolHealth::DEGRADED)) {
                    overall_status = ZfsPoolHealth::SCRUB_ACTIVE;
                }
                break;
            case ZfsPoolHealth::RESILVER_ACTIVE:
                status_messages.push_back(pool + ": RESILVER ACTIVE");
                if (static_cast<int>(overall_status) < static_cast<int>(ZfsPoolHealth::DEGRADED)) {
                    overall_status = ZfsPoolHealth::RESILVER_ACTIVE;
                }
                break;
            case ZfsPoolHealth::SCRUB_ERRORS:
                status_messages.push_back(pool + ": SCRUB FOUND ERRORS");
                if (static_cast<int>(overall_status) < static_cast<int>(ZfsPoolHealth::DEGRADED)) {
                    overall_status = ZfsPoolHealth::SCRUB_ERRORS;
                }
                break;
            default:
                status_messages.push_back(pool + ": UNKNOWN");
                if (static_cast<int>(overall_status) < static_cast<int>(ZfsPoolHealth::DEGRADED)) {
                    overall_status = ZfsPoolHealth::UNAVAIL;
                }
                break;
        }
    }
    
    // Update pool status LED
    LedColor color;
    std::string status_desc;
    
    switch (overall_status) {
        case ZfsPoolHealth::ONLINE:
            color = config_.color_online;
            status_desc = "All pools healthy";
            break;
        case ZfsPoolHealth::DEGRADED:
            color = config_.color_degraded;
            status_desc = "Some pools degraded";
            break;
        case ZfsPoolHealth::FAULTED:
            color = config_.color_faulted;
            status_desc = "Critical pool issues";
            break;
        case ZfsPoolHealth::SCRUB_ACTIVE:
            color = config_.color_scrub_active;
            status_desc = "Scrub in progress";
            break;
        case ZfsPoolHealth::RESILVER_ACTIVE:
            color = config_.color_resilver;
            status_desc = "Resilver in progress";
            break;
        case ZfsPoolHealth::SCRUB_ERRORS:
            color = config_.color_scrub_progress;
            status_desc = "Scrub completed with errors";
            break;
        default:
            color = config_.color_unavail;
            status_desc = "Unknown status";
            break;
    }
    
    updateLed(config_.pool_status_led, color);
    std::cout << "ZFS Pools: " << status_desc << std::endl;
    
    for (const auto& msg : status_messages) {
        std::cout << "  " << msg << std::endl;
    }
}

void ZfsMonitor::monitorZfsDisks() {
    for (size_t i = 0; i < led_names_.size(); ++i) {
        std::string device = disk_mapper_->getDiskDevice(static_cast<int>(i));
        std::string led_name = led_names_[i];
        
        if (device.empty()) {
            updateLed(led_name, config_.color_offline, 0);
            std::cout << "Disk " << i << " (" << led_name << "): No disk detected" << std::endl;
            continue;
        }
        
        ZfsDiskStatus status = checkDiskZfsStatus(device);
        LedColor color;
        std::string status_desc;
        
        switch (status) {
            case ZfsDiskStatus::ONLINE:
                color = config_.color_online;
                status_desc = "ONLINE in pool";
                break;
            case ZfsDiskStatus::DEGRADED:
                color = config_.color_degraded;
                status_desc = "DEGRADED in pool";
                break;
            case ZfsDiskStatus::FAULTED:
                color = config_.color_faulted;
                status_desc = "FAULTED in pool";
                break;
            case ZfsDiskStatus::NOT_IN_POOL:
                color = config_.color_unavail;
                status_desc = "Not in ZFS pool";
                break;
            case ZfsDiskStatus::DEVICE_NOT_FOUND:
                color = config_.color_offline;
                status_desc = "Device not found";
                break;
            default:
                color = config_.color_offline;
                status_desc = "Unknown status";
                break;
        }
        
        updateLed(led_name, color);
        std::cout << "Disk " << i << " (" << led_name << "): " << status_desc << " - " << device << std::endl;
    }
}

void ZfsMonitor::monitorScrubResilver() {
    if (!config_.monitor_scrub_status) {
        return;
    }
    
    std::vector<std::string> pools;
    if (config_.zfs_pools.empty()) {
        pools = zfs_executor_->getPoolList();
    } else {
        pools = config_.zfs_pools;
    }
    
    bool scrub_active = false;
    bool resilver_active = false;
    bool scrub_errors = false;
    
    for (const auto& pool : pools) {
        ZfsPoolInfo info = zfs_executor_->getPoolInfo(pool);
        
        if (info.scrub_active) {
            scrub_active = true;
        }
        if (info.resilver_active) {
            resilver_active = true;
        }
        if (info.scrub_errors) {
            scrub_errors = true;
        }
    }
    
    LedColor color;
    std::string status_desc;
    
    if (resilver_active) {
        color = config_.color_resilver;
        status_desc = "Resilver in progress";
    } else if (scrub_active) {
        color = config_.color_scrub_active;
        status_desc = "Scrub in progress";
    } else if (scrub_errors) {
        color = config_.color_scrub_progress;
        status_desc = "Recent scrub found errors";
    } else {
        color = LedColor(config_.color_online.r, config_.color_online.g, config_.color_online.b);
        status_desc = "Normal";
    }
    
    updateLed(config_.network_led, color, scrub_active || resilver_active ? 255 : 128);
    std::cout << "Scrub/Resilver Status: " << status_desc << std::endl;
}

bool ZfsMonitor::updateLed(const std::string& led_name, const LedColor& color, uint8_t brightness) {
    if (!led_available_ || !led_controller_) {
        return false;
    }
    
    try {
        ugreen_leds_t::led_type_t led_type = ugreen_leds_t::led_type_t::power; // Default
        
        // Map LED names to types (simplified mapping)
        if (led_name == "power") {
            led_type = ugreen_leds_t::led_type_t::power;
        } else if (led_name == "netdev") {
            led_type = ugreen_leds_t::led_type_t::netdev;
        } else if (led_name == "disk1") {
            led_type = ugreen_leds_t::led_type_t::disk1;
        } else if (led_name == "disk2") {
            led_type = ugreen_leds_t::led_type_t::disk2;
        } else if (led_name == "disk3") {
            led_type = ugreen_leds_t::led_type_t::disk3;
        } else if (led_name == "disk4") {
            led_type = ugreen_leds_t::led_type_t::disk4;
        } else if (led_name == "disk5") {
            led_type = ugreen_leds_t::led_type_t::disk5;
        } else if (led_name == "disk6") {
            led_type = ugreen_leds_t::led_type_t::disk6;
        } else if (led_name == "disk7") {
            led_type = ugreen_leds_t::led_type_t::disk7;
        } else if (led_name == "disk8") {
            led_type = ugreen_leds_t::led_type_t::disk8;
        }
        
        int result = led_controller_->set_rgb(led_type, color.r, color.g, color.b);
        if (result == 0) {
            led_controller_->set_brightness(led_type, brightness);
            led_controller_->set_onoff(led_type, 1);
        }
        
        return (result == 0);
    } catch (const std::exception& e) {
        std::cerr << "Error updating LED " << led_name << ": " << e.what() << std::endl;
        return false;
    }
}

bool ZfsMonitor::turnOffAllLeds() {
    if (!led_available_ || !led_controller_) {
        return false;
    }
    
    // Turn off all LEDs
    std::vector<ugreen_leds_t::led_type_t> all_leds = {
        ugreen_leds_t::led_type_t::power,
        ugreen_leds_t::led_type_t::netdev,
        ugreen_leds_t::led_type_t::disk1,
        ugreen_leds_t::led_type_t::disk2,
        ugreen_leds_t::led_type_t::disk3,
        ugreen_leds_t::led_type_t::disk4,
        ugreen_leds_t::led_type_t::disk5,
        ugreen_leds_t::led_type_t::disk6,
        ugreen_leds_t::led_type_t::disk7,
        ugreen_leds_t::led_type_t::disk8
    };
    
    for (auto led : all_leds) {
        led_controller_->set_onoff(led, 0);
    }
    
    return true;
}

void ZfsMonitor::showHelp() const {
    std::cout << "UGREEN NAS ZFS Pool and Disk Monitor (C++)\n\n";
    std::cout << "USAGE:\n";
    std::cout << "    ugreen_zfs_monitor [OPTIONS]\n\n";
    std::cout << "OPTIONS:\n";
    std::cout << "    -h, --help              Show this help message\n";
    std::cout << "    -i, --interval SECONDS  Set monitoring interval (default: 60)\n";
    std::cout << "    -p, --pools-only        Monitor pools only (not individual disks)\n";
    std::cout << "    -d, --disks-only        Monitor disks only (not pool status)\n";
    std::cout << "    -t, --test              Run one test cycle and exit\n";
    std::cout << "    -c, --config FILE       Use specific config file\n\n";
    std::cout << "ZFS MONITORING FEATURES:\n";
    std::cout << "    - Pool health status (ONLINE, DEGRADED, FAULTED)\n";
    std::cout << "    - Scrub and resilver progress monitoring\n";
    std::cout << "    - Individual disk status in ZFS pools\n";
    std::cout << "    - Error detection and reporting\n\n";
    std::cout << "LED MAPPING:\n";
    std::cout << "    Power LED:     Overall pool health status\n";
    std::cout << "    Network LED:   Scrub/resilver operations status\n";
    std::cout << "    Disk LEDs:     Individual disk status in pools\n\n";
    std::cout << "REQUIREMENTS:\n";
    std::cout << "    - Root privileges (sudo)\n";
    std::cout << "    - ZFS tools (zfsutils-linux)\n";
    std::cout << "    - Active ZFS pools\n";
}

void ZfsMonitor::showStatus() const {
    std::cout << "=== ZFS Monitor Status ===" << std::endl;
    std::cout << "LED Controller: " << (led_available_ ? "Available" : "Not Available") << std::endl;
    std::cout << "Monitoring: " << (running_ ? "Active" : "Stopped") << std::endl;
    std::cout << "Interval: " << config_.monitor_interval << " seconds" << std::endl;
    std::cout << "Pool Monitoring: " << (config_.monitor_zfs_pools ? "Enabled" : "Disabled") << std::endl;
    std::cout << "Disk Monitoring: " << (config_.monitor_zfs_disks ? "Enabled" : "Disabled") << std::endl;
    std::cout << "Scrub Monitoring: " << (config_.monitor_scrub_status ? "Enabled" : "Disabled") << std::endl;
}

// Utility function implementations
std::vector<std::string> splitString(const std::string& str, char delimiter) {
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string token;
    
    while (std::getline(ss, token, delimiter)) {
        token = trimString(token);
        if (!token.empty()) {
            result.push_back(token);
        }
    }
    
    return result;
}

std::string trimString(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return "";
    }
    
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

bool fileExists(const std::string& filename) {
    return (access(filename.c_str(), F_OK) == 0);
}

std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string ZfsMonitor::colorToString(const LedColor& color) const {
    return std::to_string(color.r) + " " + std::to_string(color.g) + " " + std::to_string(color.b);
}

LedColor ZfsMonitor::stringToColor(const std::string& color_str) const {
    std::vector<std::string> components = splitString(color_str, ' ');
    if (components.size() >= 3) {
        return LedColor(
            static_cast<uint8_t>(std::stoi(components[0])),
            static_cast<uint8_t>(std::stoi(components[1])),
            static_cast<uint8_t>(std::stoi(components[2]))
        );
    }
    return LedColor(0, 0, 0);
}

void ZfsMonitor::logMessage(const std::string& message) const {
    std::cout << "[INFO] " << message << std::endl;
}

void ZfsMonitor::logError(const std::string& error) const {
    std::cerr << "[ERROR] " << error << std::endl;
}