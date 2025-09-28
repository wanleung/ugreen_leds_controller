#include "zfs_monitor.h"
#include <iostream>
#include <csignal>
#include <cstdlib>
#include <unistd.h>
#include <getopt.h>

// Global monitor instance for signal handling
static ZfsMonitor* g_monitor = nullptr;

// Signal handler for graceful shutdown
void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
    if (g_monitor) {
        g_monitor->stopMonitoring();
    }
}

// Setup signal handlers
void setupSignalHandlers() {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
}

// Print usage information
void printUsage(const char* program_name) {
    std::cout << "UGREEN NAS ZFS Pool and Disk Monitor (C++)\n\n";
    std::cout << "USAGE:\n";
    std::cout << "    " << program_name << " [OPTIONS]\n\n";
    std::cout << "OPTIONS:\n";
    std::cout << "    -h, --help              Show this help message\n";
    std::cout << "    -i, --interval SECONDS  Set monitoring interval (default: 60)\n";
    std::cout << "    -p, --pools-only        Monitor pools only (not individual disks)\n";
    std::cout << "    -d, --disks-only        Monitor disks only (not pool status)\n";
    std::cout << "    -t, --test              Run one test cycle and exit\n";
    std::cout << "    -s, --status            Show current monitor status and exit\n";
    std::cout << "    -c, --config FILE       Use specific config file\n";
    std::cout << "    -v, --version           Show version information\n\n";
    std::cout << "ZFS MONITORING FEATURES:\n";
    std::cout << "    - Pool health status (ONLINE, DEGRADED, FAULTED)\n";
    std::cout << "    - Scrub and resilver progress monitoring\n";
    std::cout << "    - Individual disk status in ZFS pools\n";
    std::cout << "    - Error detection and reporting\n\n";
    std::cout << "LED MAPPING:\n";
    std::cout << "    Power LED:     Overall pool health status\n";
    std::cout << "    Network LED:   Scrub/resilver operations status\n";
    std::cout << "    Disk LEDs:     Individual disk status in pools\n\n";
    std::cout << "COLOR SCHEME:\n";
    std::cout << "    Green:   Healthy/Online\n";
    std::cout << "    Yellow:  Degraded/Warning\n";
    std::cout << "    Red:     Faulted/Critical\n";
    std::cout << "    Blue:    Unavailable/Not in pool\n";
    std::cout << "    Purple:  Scrub completed with errors\n";
    std::cout << "    Cyan:    Resilver in progress\n";
    std::cout << "    Orange:  Scrub in progress\n\n";
    std::cout << "REQUIREMENTS:\n";
    std::cout << "    - Root privileges (sudo)\n";
    std::cout << "    - ZFS tools (zfsutils-linux)\n";
    std::cout << "    - Active ZFS pools\n\n";
    std::cout << "EXAMPLES:\n";
    std::cout << "    " << program_name << "                    # Run with default settings\n";
    std::cout << "    " << program_name << " --test             # Run single test cycle\n";
    std::cout << "    " << program_name << " -i 30              # Monitor every 30 seconds\n";
    std::cout << "    " << program_name << " --pools-only       # Monitor only pool status\n";
    std::cout << "    " << program_name << " -c /etc/custom.conf # Use custom config\n\n";
}

// Print version information
void printVersion() {
    std::cout << "UGREEN ZFS Monitor (C++) version 1.0\n";
    std::cout << "Part of ugreen_leds_controller project\n";
    std::cout << "License: GPL-2.0-only\n";
}

// Check if running as root
bool checkRootPrivileges() {
    if (geteuid() != 0) {
        std::cerr << "Error: This program must be run as root (use sudo)" << std::endl;
        std::cerr << "ZFS monitoring and LED control require root privileges" << std::endl;
        return false;
    }
    return true;
}

// Check if ZFS is available
bool checkZfsAvailability() {
    int result = system("which zpool > /dev/null 2>&1");
    if (result != 0) {
        std::cerr << "Error: ZFS tools not found" << std::endl;
        std::cerr << "Please install zfsutils-linux package:" << std::endl;
        std::cerr << "    sudo apt install zfsutils-linux" << std::endl;
        return false;
    }
    return true;
}

// Parse command line arguments
bool parseArguments(int argc, char* argv[], ZfsMonitorConfig& config, 
                   bool& test_mode, bool& show_status, std::string& config_file) {
    const struct option long_options[] = {
        {"help",        no_argument,       0, 'h'},
        {"interval",    required_argument, 0, 'i'},
        {"pools-only",  no_argument,       0, 'p'},
        {"disks-only",  no_argument,       0, 'd'},
        {"test",        no_argument,       0, 't'},
        {"status",      no_argument,       0, 's'},
        {"config",      required_argument, 0, 'c'},
        {"version",     no_argument,       0, 'v'},
        {0, 0, 0, 0}
    };
    
    int option_index = 0;
    int c;
    
    while ((c = getopt_long(argc, argv, "hi:pdtsc:v", long_options, &option_index)) != -1) {
        switch (c) {
            case 'h':
                printUsage(argv[0]);
                return false;
                
            case 'i':
                try {
                    config.monitor_interval = std::stoi(optarg);
                    if (config.monitor_interval < 1) {
                        throw std::invalid_argument("Interval must be positive");
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error: Invalid interval value: " << optarg << std::endl;
                    return false;
                }
                break;
                
            case 'p':
                config.monitor_zfs_pools = true;
                config.monitor_zfs_disks = false;
                break;
                
            case 'd':
                config.monitor_zfs_pools = false;
                config.monitor_zfs_disks = true;
                break;
                
            case 't':
                test_mode = true;
                break;
                
            case 's':
                show_status = true;
                break;
                
            case 'c':
                config_file = optarg;
                break;
                
            case 'v':
                printVersion();
                return false;
                
            case '?':
                std::cerr << "Unknown option. Use -h for help." << std::endl;
                return false;
                
            default:
                return false;
        }
    }
    
    // Check for extra arguments
    if (optind < argc) {
        std::cerr << "Error: Unexpected arguments:";
        for (int i = optind; i < argc; i++) {
            std::cerr << " " << argv[i];
        }
        std::cerr << std::endl;
        std::cerr << "Use -h for help." << std::endl;
        return false;
    }
    
    return true;
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    ZfsMonitorConfig config;
    bool test_mode = false;
    bool show_status = false;
    std::string config_file;
    
    if (!parseArguments(argc, argv, config, test_mode, show_status, config_file)) {
        return (argc > 1 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help" ||
                            std::string(argv[1]) == "-v" || std::string(argv[1]) == "--version")) ? 0 : 1;
    }
    
    // Check prerequisites
    if (!checkRootPrivileges()) {
        return 1;
    }
    
    if (!checkZfsAvailability()) {
        return 1;
    }
    
    // Create monitor instance
    std::unique_ptr<ZfsMonitor> monitor = std::make_unique<ZfsMonitor>();
    g_monitor = monitor.get();
    
    // Setup signal handlers for graceful shutdown
    setupSignalHandlers();
    
    try {
        // Load configuration
        if (!config_file.empty()) {
            if (!monitor->loadConfig(config_file)) {
                std::cerr << "Warning: Could not load config file: " << config_file << std::endl;
            }
        } else {
            monitor->loadConfig(); // Load default config
        }
        
        // Apply command line overrides
        monitor->setConfig(config);
        
        // Initialize monitor
        if (!monitor->initialize()) {
            std::cerr << "Error: Failed to initialize monitor" << std::endl;
            return 1;
        }
        
        // Handle different run modes
        if (show_status) {
            monitor->showStatus();
            return 0;
        }
        
        if (test_mode) {
            std::cout << "Running single ZFS test cycle..." << std::endl;
            bool result = monitor->runSingleCheck();
            std::cout << "Test cycle " << (result ? "completed successfully" : "failed") << std::endl;
            return result ? 0 : 1;
        }
        
        // Start continuous monitoring
        std::cout << "Initializing UGREEN ZFS Monitor..." << std::endl;
        monitor->startMonitoring();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Error: Unknown exception occurred" << std::endl;
        return 1;
    }
    
    // Cleanup
    g_monitor = nullptr;
    std::cout << "ZFS Monitor stopped." << std::endl;
    
    return 0;
}