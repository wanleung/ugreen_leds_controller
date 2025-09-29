#include "ugreen_monitor.h"
#include "zfs_monitor.h"  // For LedColor and DiskMapper
#include "ugreen_leds.h"
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
#include <iomanip>

// Default Configuration Constructor
UgreenMonitorConfig::UgreenMonitorConfig() :
    ugreen_leds_cli_path("ugreen_leds_cli"),
    monitor_interval(30),
    monitor_network(true),
    monitor_disks(true),
    turn_off_leds_on_exit(false),
    network_led("netdev"),
    ping_target("8.8.8.8"),
    ping_count(1),
    ping_timeout(3),
    mapping_method("ata"),
    color_healthy(0, 255, 0),      // Green
    color_warning(255, 255, 0),    // Yellow
    color_critical(255, 0, 0),     // Red
    color_offline(0, 0, 255),      // Blue
    color_disabled(0, 0, 0)        // Off
{
}

// Command Executor Implementation
std::string CommandExecutor::execute(const std::string& command) {
    return runCommand(command);
}

int CommandExecutor::executeWithExitCode(const std::string& command, std::string& output) {
    int exit_code = 0;
    output = runCommand(command, &exit_code);
    return exit_code;
}

bool CommandExecutor::commandExists(const std::string& command) {
    std::string check_cmd = "which " + command + " >/dev/null 2>&1";
    return (system(check_cmd.c_str()) == 0);
}

std::string CommandExecutor::runCommand(const std::string& command, int* exit_code) {
    std::string result;
    char buffer[128];
    
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        if (exit_code) *exit_code = -1;
        return "";
    }
    
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    
    int status = pclose(pipe);
    if (exit_code) {
        *exit_code = WEXITSTATUS(status);
    }
    
    return result;
}

// Network Monitor Implementation
NetworkMonitor::NetworkMonitor() : 
    ping_target_("8.8.8.8"), 
    ping_count_(1), 
    ping_timeout_(3) 
{
}

NetworkMonitor::~NetworkMonitor() {}

void NetworkMonitor::setNetworkInterfaces(const std::vector<std::string>& interfaces) {
    configured_interfaces_ = interfaces;
}

void NetworkMonitor::setPingTarget(const std::string& target, int count, int timeout) {
    ping_target_ = target;
    ping_count_ = count;
    ping_timeout_ = timeout;
}

std::vector<NetworkInterface> NetworkMonitor::getNetworkInterfaces() {
    std::vector<NetworkInterface> interfaces;
    std::vector<std::string> interface_names;
    
    if (configured_interfaces_.empty()) {
        interface_names = autoDetectInterfaces();
    } else {
        interface_names = configured_interfaces_;
    }
    
    for (const auto& name : interface_names) {
        NetworkInterface iface;
        iface.name = name;
        iface.is_up = isInterfaceUp(name);
        iface.is_bridge = isBridgeInterface(name);
        iface.speed = getInterfaceSpeed(name);
        iface.status = iface.is_up ? "UP" : "DOWN";
        
        interfaces.push_back(iface);
    }
    
    return interfaces;
}

NetworkStatus NetworkMonitor::checkNetworkStatus() {
    auto interfaces = getNetworkInterfaces();
    
    if (interfaces.empty()) {
        return NetworkStatus::UNKNOWN;
    }
    
    int total_interfaces = interfaces.size();
    int up_interfaces = 0;
    bool bridge_up = false;
    bool physical_up = false;
    
    // Check interface status
    for (const auto& iface : interfaces) {
        if (iface.is_up) {
            up_interfaces++;
            if (iface.is_bridge) {
                bridge_up = true;
                std::cout << "Bridge interface " << iface.name << " is UP" << std::endl;
            } else {
                physical_up = true;
                std::cout << "Physical interface " << iface.name << " is UP" << std::endl;
            }
        } else {
            std::cout << "Interface " << iface.name << " is DOWN" << std::endl;
        }
    }
    
    // Test connectivity if any interface is up
    bool has_connectivity = false;
    if (up_interfaces > 0 && !ping_target_.empty()) {
        has_connectivity = testConnectivity(ping_target_, ping_count_, ping_timeout_);
        if (has_connectivity) {
            std::cout << "Connectivity test to " << ping_target_ << " successful" << std::endl;
        } else {
            std::cout << "Connectivity test to " << ping_target_ << " failed" << std::endl;
        }
    }
    
    // Bridge-aware logic: good status if bridge is up OR (physical interfaces up AND connectivity)
    if (bridge_up || (physical_up && has_connectivity)) {
        return NetworkStatus::HEALTHY;
    } else if (up_interfaces > 0) {
        return NetworkStatus::WARNING;
    } else {
        return NetworkStatus::CRITICAL;
    }
}

bool NetworkMonitor::testConnectivity(const std::string& target, int count, int timeout) {
    std::string cmd = "ping -c " + std::to_string(count) + 
                     " -W " + std::to_string(timeout) + 
                     " " + target + " >/dev/null 2>&1";
    return (system(cmd.c_str()) == 0);
}

std::string NetworkMonitor::runCommand(const std::string& command) {
    return CommandExecutor::execute(command);
}

std::vector<std::string> NetworkMonitor::autoDetectInterfaces() {
    std::vector<std::string> interfaces;
    
    // Get all network interfaces, including bridges but excluding loopback, docker, etc.
    std::string cmd = "ip link show | grep -E '^[0-9]+: [^:]+' | grep -vE '(lo|docker|veth)' | "
                     "grep -E '(eth|ens|enp|br[0-9])' | awk -F': ' '{print $2}' | awk '{print $1}'";
    
    std::string output = runCommand(cmd);
    std::istringstream iss(output);
    std::string interface;
    
    while (std::getline(iss, interface)) {
        interface = StringUtils::trim(interface);
        if (!interface.empty()) {
            interfaces.push_back(interface);
        }
    }
    
    return interfaces;
}

bool NetworkMonitor::isInterfaceUp(const std::string& interface) {
    std::string cmd = "ip link show " + interface + " 2>/dev/null | grep -q 'state UP'";
    return (system(cmd.c_str()) == 0);
}

bool NetworkMonitor::isBridgeInterface(const std::string& interface) {
    return StringUtils::startsWith(interface, "br");
}

uint64_t NetworkMonitor::getInterfaceSpeed(const std::string& interface) {
    std::string speed_file = "/sys/class/net/" + interface + "/speed";
    std::ifstream file(speed_file);
    if (file.is_open()) {
        std::string speed_str;
        std::getline(file, speed_str);
        try {
            return std::stoull(speed_str);
        } catch (...) {
            return 0;
        }
    }
    return 0;
}

// S.M.A.R.T. Monitor Implementation
SmartMonitor::SmartMonitor() : smart_available_(false) {
    smart_available_ = CommandExecutor::commandExists("smartctl");
    if (!smart_available_) {
        std::cerr << "Warning: smartctl not found, S.M.A.R.T. monitoring disabled" << std::endl;
    }
}

SmartMonitor::~SmartMonitor() {}

SmartDiskInfo SmartMonitor::getSmartInfo(const std::string& device_path) {
    SmartDiskInfo info;
    info.device_path = device_path;
    info.device_name = device_path.substr(device_path.find_last_of('/') + 1);
    info.status = SmartStatus::UNAVAILABLE;
    info.temperature = 0;
    info.reallocated_sectors = 0;
    info.pending_sectors = 0;
    
    if (!smart_available_ || !SystemUtils::deviceExists(device_path)) {
        info.status = SmartStatus::DEVICE_NOT_FOUND;
        return info;
    }
    
    // Get basic device info
    std::string info_cmd = "smartctl -i " + device_path + " 2>/dev/null";
    std::string info_output = runCommand(info_cmd);
    
    // Extract model and serial
    std::regex model_regex(R"(Device Model:\s+(.+))");
    std::regex serial_regex(R"(Serial Number:\s+(.+))");
    std::smatch match;
    
    if (std::regex_search(info_output, match, model_regex)) {
        info.model = StringUtils::trim(match[1].str());
    }
    if (std::regex_search(info_output, match, serial_regex)) {
        info.serial = StringUtils::trim(match[1].str());
    }
    
    // Get S.M.A.R.T. health status
    std::string health_cmd = "smartctl -H " + device_path + " 2>/dev/null";
    std::string health_output;
    int exit_code = CommandExecutor::executeWithExitCode(health_cmd, health_output);
    
    info.status = interpretExitCode(exit_code);
    
    // Parse health status text
    if (StringUtils::contains(health_output, "PASSED")) {
        info.health_status = "PASSED";
    } else if (StringUtils::contains(health_output, "FAILED")) {
        info.health_status = "FAILED";
        info.status = SmartStatus::CRITICAL;
    } else {
        info.health_status = "UNKNOWN";
    }
    
    // Get detailed attributes for temperature and error counts
    std::string attr_cmd = "smartctl -A " + device_path + " 2>/dev/null";
    std::string attr_output = runCommand(attr_cmd);
    parseSmartOutput(attr_output, info);
    
    return info;
}

SmartStatus SmartMonitor::checkSmartStatus(const std::string& device_path) {
    if (!smart_available_ || !SystemUtils::deviceExists(device_path)) {
        return SmartStatus::DEVICE_NOT_FOUND;
    }
    
    std::string cmd = "smartctl -H " + device_path + " 2>/dev/null";
    std::string output;
    int exit_code = CommandExecutor::executeWithExitCode(cmd, output);
    
    return interpretExitCode(exit_code);
}

bool SmartMonitor::isSmartAvailable() {
    return smart_available_;
}

std::string SmartMonitor::runCommand(const std::string& command) {
    return CommandExecutor::execute(command);
}

void SmartMonitor::parseSmartOutput(const std::string& output, SmartDiskInfo& info) {
    std::istringstream iss(output);
    std::string line;
    
    while (std::getline(iss, line)) {
        // Parse temperature (attribute 194)
        if (StringUtils::contains(line, "Temperature_Celsius") || 
            StringUtils::contains(line, "Airflow_Temperature_Cel")) {
            std::regex temp_regex(R"(\s+(\d+)\s+[A-Za-z_]+\s+[0-9x]+\s+\d+\s+\d+\s+\d+\s+[A-Za-z-]+\s+[A-Za-z-]+\s+[A-Za-z-]+\s+(\d+))");
            std::smatch match;
            if (std::regex_search(line, match, temp_regex)) {
                try {
                    info.temperature = std::stoull(match[2].str());
                } catch (...) {}
            }
        }
        
        // Parse reallocated sectors (attribute 5)
        if (StringUtils::contains(line, "Reallocated_Sector_Ct")) {
            std::regex realloc_regex(R"(\s+5\s+[A-Za-z_]+\s+[0-9x]+\s+\d+\s+\d+\s+\d+\s+[A-Za-z-]+\s+[A-Za-z-]+\s+[A-Za-z-]+\s+(\d+))");
            std::smatch match;
            if (std::regex_search(line, match, realloc_regex)) {
                try {
                    info.reallocated_sectors = std::stoull(match[1].str());
                } catch (...) {}
            }
        }
        
        // Parse pending sectors (attribute 197)
        if (StringUtils::contains(line, "Current_Pending_Sector")) {
            std::regex pending_regex(R"(\s+197\s+[A-Za-z_]+\s+[0-9x]+\s+\d+\s+\d+\s+\d+\s+[A-Za-z-]+\s+[A-Za-z-]+\s+[A-Za-z-]+\s+(\d+))");
            std::smatch match;
            if (std::regex_search(line, match, pending_regex)) {
                try {
                    info.pending_sectors = std::stoull(match[1].str());
                } catch (...) {}
            }
        }
    }
}

SmartStatus SmartMonitor::interpretExitCode(int exit_code) {
    // smartctl exit codes (see smartctl man page)
    if ((exit_code & 8) != 0 || (exit_code & 16) != 0) {
        // Bit 3: Disk failing, Bit 4: Prefail attributes below threshold
        return SmartStatus::CRITICAL;
    } else if ((exit_code & 64) != 0 || (exit_code & 128) != 0) {
        // Bit 6: Error log has errors, Bit 7: Self-test log has errors
        return SmartStatus::WARNING;
    } else if ((exit_code & 7) != 0) {
        // Bits 0-2: Command/device issues
        return SmartStatus::UNAVAILABLE;
    } else {
        return SmartStatus::HEALTHY;
    }
}

// UGREEN Monitor Implementation
UgreenMonitor::UgreenMonitor() :
    running_(false),
    led_available_(false),
    network_monitor_(std::make_unique<NetworkMonitor>()),
    smart_monitor_(std::make_unique<SmartMonitor>()),
    disk_mapper_(std::make_unique<DiskMapper>()),
    led_controller_(nullptr)
{
    setDefaultConfig();
    led_names_ = {"disk1", "disk2", "disk3", "disk4", "disk5", "disk6", "disk7", "disk8"};
}

UgreenMonitor::~UgreenMonitor() {
    cleanup();
}

void UgreenMonitor::setDefaultConfig() {
    config_ = UgreenMonitorConfig();
}

bool UgreenMonitor::loadConfig(const std::string& config_file) {
    std::string config_path = config_file.empty() ? "/etc/ugreen-monitor.conf" : config_file;
    
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

void UgreenMonitor::parseConfigLine(const std::string& line) {
    std::string trimmed = StringUtils::trim(line);
    if (trimmed.empty() || trimmed[0] == '#') {
        return;
    }
    
    size_t equals_pos = trimmed.find('=');
    if (equals_pos == std::string::npos) {
        return;
    }
    
    std::string key = StringUtils::trim(trimmed.substr(0, equals_pos));
    std::string value = StringUtils::trim(trimmed.substr(equals_pos + 1));
    
    // Remove quotes if present
    if (value.length() >= 2 && value.front() == '"' && value.back() == '"') {
        value = value.substr(1, value.length() - 2);
    }
    
    // Parse configuration values
    if (key == "UGREEN_LEDS_CLI") {
        config_.ugreen_leds_cli_path = value;
    } else if (key == "MONITOR_INTERVAL") {
        try {
            config_.monitor_interval = std::stoi(value);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Invalid MONITOR_INTERVAL value '" << value << "', using default" << std::endl;
        }
    } else if (key == "MONITOR_NETWORK") {
        config_.monitor_network = (value == "true");
    } else if (key == "MONITOR_DISKS") {
        config_.monitor_disks = (value == "true");
    } else if (key == "NETWORK_INTERFACES") {
        config_.network_interfaces = StringUtils::split(value, ' ');
    } else if (key == "PING_TARGET") {
        config_.ping_target = value;
    } else if (key == "MAPPING_METHOD") {
        config_.mapping_method = value;
    } else if (key == "COLOR_HEALTHY") {
        config_.color_healthy = stringToColor(value);
    } else if (key == "COLOR_WARNING") {
        config_.color_warning = stringToColor(value);
    } else if (key == "COLOR_CRITICAL") {
        config_.color_critical = stringToColor(value);
    } else if (key == "COLOR_OFFLINE") {
        config_.color_offline = stringToColor(value);
    }
    // Add more configuration parsing as needed
}

void UgreenMonitor::setConfig(const UgreenMonitorConfig& config) {
    config_ = config;
}

UgreenMonitorConfig UgreenMonitor::getConfig() const {
    return config_;
}

bool UgreenMonitor::initialize() {
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
    
    // Configure network monitor
    network_monitor_->setNetworkInterfaces(config_.network_interfaces);
    network_monitor_->setPingTarget(config_.ping_target, config_.ping_count, config_.ping_timeout);
    
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

bool UgreenMonitor::loadI2cModules() {
    return SystemUtils::loadModule("i2c-dev");
}

bool UgreenMonitor::initializeLedController() {
    try {
        led_controller_ = std::make_unique<ugreen_leds_t>();
        return (led_controller_->start() == 0);
    } catch (const std::exception& e) {
        std::cerr << "Error initializing LED controller: " << e.what() << std::endl;
        return false;
    }
}

void UgreenMonitor::cleanup() {
    running_ = false;
    
    if (config_.turn_off_leds_on_exit && led_available_) {
        turnOffAllLeds();
    }
    
    if (led_controller_) {
        led_controller_.reset();
    }
}

bool UgreenMonitor::runSingleCheck() {
    std::cout << "=== Monitor check at " << SystemUtils::getCurrentTimestamp() << " ===" << std::endl;
    
    if (config_.monitor_network) {
        monitorNetwork();
    }
    
    if (config_.monitor_disks) {
        monitorDisks();
    }
    
    std::cout << std::endl;
    return true;
}

void UgreenMonitor::startMonitoring() {
    running_ = true;
    
    std::cout << "Starting UGREEN monitoring..." << std::endl;
    std::cout << "Monitor interval: " << config_.monitor_interval << " seconds" << std::endl;
    std::cout << "Network monitoring: " << (config_.monitor_network ? "enabled" : "disabled") << std::endl;
    std::cout << "Disk monitoring: " << (config_.monitor_disks ? "enabled" : "disabled") << std::endl;
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

void UgreenMonitor::stopMonitoring() {
    running_ = false;
}

void UgreenMonitor::monitorNetwork() {
    NetworkStatus status = network_monitor_->checkNetworkStatus();
    
    LedColor color;
    std::string status_desc;
    
    switch (status) {
        case NetworkStatus::HEALTHY:
            color = config_.color_healthy;
            status_desc = "Healthy (bridge up or physical interfaces with connectivity)";
            break;
        case NetworkStatus::WARNING:
            color = config_.color_warning;
            status_desc = "Warning (interfaces up but no bridge or connectivity issues)";
            break;
        case NetworkStatus::CRITICAL:
            color = config_.color_critical;
            status_desc = "Critical (all interfaces down)";
            break;
        default:
            color = config_.color_offline;
            status_desc = "Status unknown";
            break;
    }
    
    updateLed(config_.network_led, color);
    std::cout << "Network: " << status_desc << std::endl;
}

void UgreenMonitor::monitorDisks() {
    for (size_t i = 0; i < led_names_.size(); ++i) {
        std::string device = disk_mapper_->getDiskDevice(static_cast<int>(i));
        std::string led_name = led_names_[i];
        
        if (device.empty()) {
            updateLed(led_name, config_.color_disabled, 0);
            std::cout << "Disk " << i << " (" << led_name << "): No disk detected" << std::endl;
            continue;
        }
        
        SmartStatus status = smart_monitor_->checkSmartStatus(device);
        LedColor color;
        std::string status_desc;
        
        switch (status) {
            case SmartStatus::HEALTHY:
                color = config_.color_healthy;
                status_desc = "Healthy";
                break;
            case SmartStatus::WARNING:
                color = config_.color_warning;
                status_desc = "Warning";
                break;
            case SmartStatus::CRITICAL:
                color = config_.color_critical;
                status_desc = "Critical";
                break;
            case SmartStatus::UNAVAILABLE:
                color = config_.color_offline;
                status_desc = "Unknown status";
                break;
            case SmartStatus::DEVICE_NOT_FOUND:
                color = config_.color_disabled;
                status_desc = "Device not found";
                break;
        }
        
        updateLed(led_name, color);
        std::cout << "Disk " << i << " (" << led_name << "): " << status_desc << " - " << device << std::endl;
    }
}

bool UgreenMonitor::updateLed(const std::string& led_name, const LedColor& color, uint8_t brightness) {
    if (!led_available_ || !led_controller_) {
        return false;
    }
    
    try {
        ugreen_leds_t::led_type_t led_type = ugreen_leds_t::led_type_t::power; // Default
        
        // Map LED names to types
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

bool UgreenMonitor::turnOffAllLeds() {
    if (!led_available_ || !led_controller_) {
        return false;
    }
    
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

void UgreenMonitor::showHelp() const {
    std::cout << "UGREEN NAS Network and Disk Health Monitor (C++)\n\n";
    std::cout << "USAGE:\n";
    std::cout << "    ugreen_monitor [OPTIONS]\n\n";
    std::cout << "OPTIONS:\n";
    std::cout << "    -h, --help              Show this help message\n";
    std::cout << "    -i, --interval SECONDS  Set monitoring interval (default: 30)\n";
    std::cout << "    -n, --network-only      Monitor network only\n";
    std::cout << "    -d, --disks-only        Monitor disks only\n";
    std::cout << "    -t, --test              Run one test cycle and exit\n";
    std::cout << "    -c, --config FILE       Use specific config file\n\n";
    std::cout << "MONITORING FEATURES:\n";
    std::cout << "    - Network interface status and connectivity\n";
    std::cout << "    - Bridge-aware network monitoring\n";
    std::cout << "    - S.M.A.R.T. disk health monitoring\n";
    std::cout << "    - LED status indicators\n\n";
    std::cout << "REQUIREMENTS:\n";
    std::cout << "    - Root privileges (sudo)\n";
    std::cout << "    - smartmontools package (for disk monitoring)\n";
    std::cout << "    - i2c-dev kernel module\n";
}

void UgreenMonitor::showStatus() const {
    std::cout << "=== Monitor Status ===" << std::endl;
    std::cout << "LED Controller: " << (led_available_ ? "Available" : "Not Available") << std::endl;
    std::cout << "S.M.A.R.T. Monitor: " << (smart_monitor_->isSmartAvailable() ? "Available" : "Not Available") << std::endl;
    std::cout << "Monitoring: " << (running_ ? "Active" : "Stopped") << std::endl;
    std::cout << "Interval: " << config_.monitor_interval << " seconds" << std::endl;
    std::cout << "Network Monitoring: " << (config_.monitor_network ? "Enabled" : "Disabled") << std::endl;
    std::cout << "Disk Monitoring: " << (config_.monitor_disks ? "Enabled" : "Disabled") << std::endl;
}

std::string UgreenMonitor::colorToString(const LedColor& color) const {
    return std::to_string(color.r) + " " + std::to_string(color.g) + " " + std::to_string(color.b);
}

LedColor UgreenMonitor::stringToColor(const std::string& color_str) const {
    std::vector<std::string> components = StringUtils::split(color_str, ' ');
    if (components.size() >= 3) {
        try {
            return LedColor(
                static_cast<uint8_t>(std::stoi(components[0])),
                static_cast<uint8_t>(std::stoi(components[1])),
                static_cast<uint8_t>(std::stoi(components[2]))
            );
        } catch (const std::exception& e) {
            std::cerr << "Warning: Invalid color format '" << color_str << "', using black" << std::endl;
        }
    }
    return LedColor(0, 0, 0);
}

void UgreenMonitor::logMessage(const std::string& message) const {
    std::cout << "[INFO] " << message << std::endl;
}

void UgreenMonitor::logError(const std::string& error) const {
    std::cerr << "[ERROR] " << error << std::endl;
}

// System Utility Functions
bool SystemUtils::isRoot() {
    return (geteuid() == 0);
}

bool SystemUtils::moduleLoaded(const std::string& module) {
    std::string cmd = "lsmod | grep -q " + module;
    return (system(cmd.c_str()) == 0);
}

bool SystemUtils::loadModule(const std::string& module) {
    if (moduleLoaded(module)) {
        return true;
    }
    
    std::string cmd = "modprobe " + module + " 2>/dev/null";
    return (system(cmd.c_str()) == 0);
}

bool SystemUtils::fileExists(const std::string& path) {
    return (access(path.c_str(), F_OK) == 0);
}

bool SystemUtils::deviceExists(const std::string& device) {
    return (access(device.c_str(), F_OK) == 0);
}

std::string SystemUtils::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::vector<std::string> SystemUtils::listBlockDevices() {
    std::vector<std::string> devices;
    std::string output = CommandExecutor::execute("lsblk -d -o NAME -n");
    
    std::istringstream iss(output);
    std::string device;
    while (std::getline(iss, device)) {
        device = StringUtils::trim(device);
        if (!device.empty()) {
            devices.push_back("/dev/" + device);
        }
    }
    
    return devices;
}

std::string SystemUtils::getDeviceModel(const std::string& device) {
    std::string cmd = "lsblk -d -o MODEL -n " + device + " 2>/dev/null";
    return StringUtils::trim(CommandExecutor::execute(cmd));
}

std::string SystemUtils::getDeviceSerial(const std::string& device) {
    std::string cmd = "lsblk -d -o SERIAL -n " + device + " 2>/dev/null";
    return StringUtils::trim(CommandExecutor::execute(cmd));
}

// String Utility Functions
std::vector<std::string> StringUtils::split(const std::string& str, char delimiter) {
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string token;
    
    while (std::getline(ss, token, delimiter)) {
        token = trim(token);
        if (!token.empty()) {
            result.push_back(token);
        }
    }
    
    return result;
}

std::string StringUtils::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return "";
    }
    
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

std::string StringUtils::toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

bool StringUtils::startsWith(const std::string& str, const std::string& prefix) {
    return str.length() >= prefix.length() && 
           str.compare(0, prefix.length(), prefix) == 0;
}

bool StringUtils::endsWith(const std::string& str, const std::string& suffix) {
    return str.length() >= suffix.length() && 
           str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

bool StringUtils::contains(const std::string& str, const std::string& substring) {
    return str.find(substring) != std::string::npos;
}