#include "ugreen_monitor.h"
#include <iostream>
#include <signal.h>
#include <getopt.h>
#include <memory>

std::unique_ptr<UgreenMonitor> monitor_instance;

void signal_handler(int signum) {
    if (monitor_instance) {
        std::cout << "\nReceived signal " << signum << ", shutting down..." << std::endl;
        monitor_instance->stopMonitoring();
    }
}

void print_usage() {
    std::cout << "UGREEN NAS Network and Disk Health Monitor (C++)\n\n";
    std::cout << "USAGE:\n";
    std::cout << "    ugreen_monitor [OPTIONS]\n\n";
    std::cout << "OPTIONS:\n";
    std::cout << "    -h, --help              Show this help message\n";
    std::cout << "    -i, --interval SECONDS  Set monitoring interval (default: 30)\n";
    std::cout << "    -n, --network-only      Monitor network only\n";
    std::cout << "    -d, --disks-only        Monitor disks only\n";
    std::cout << "    -t, --test              Run one test cycle and exit\n";
    std::cout << "    -c, --config FILE       Use specific config file\n";
    std::cout << "    -s, --status            Show monitor status\n";
    std::cout << "    -v, --version           Show version information\n\n";
    std::cout << "MONITORING FEATURES:\n";
    std::cout << "    - Network interface status and connectivity\n";
    std::cout << "    - Bridge-aware network monitoring (TrueNAS SCALE compatible)\n";
    std::cout << "    - S.M.A.R.T. disk health monitoring\n";
    std::cout << "    - LED status indicators\n\n";
    std::cout << "CONFIG FILE:\n";
    std::cout << "    Default location: /etc/ugreen-monitor.conf\n";
    std::cout << "    See example configuration for available options\n\n";
    std::cout << "REQUIREMENTS:\n";
    std::cout << "    - Root privileges (sudo) for I2C access\n";
    std::cout << "    - smartmontools package (for disk monitoring)\n";
    std::cout << "    - i2c-dev kernel module\n";
    std::cout << "    - Compatible UGREEN NAS device\n\n";
    std::cout << "EXAMPLES:\n";
    std::cout << "    sudo ugreen_monitor                     # Start monitoring with defaults\n";
    std::cout << "    sudo ugreen_monitor -t                  # Run one test cycle\n";
    std::cout << "    sudo ugreen_monitor -i 60 -n            # Monitor network only, 60s interval\n";
    std::cout << "    sudo ugreen_monitor -c /path/to/config   # Use custom config file\n";
}

void print_version() {
    std::cout << "UGREEN Monitor C++ v1.0.0" << std::endl;
    std::cout << "Copyright (c) 2024 - LED Controller for UGREEN NAS devices" << std::endl;
    std::cout << "Compatible with UGREEN DX/DXP series NAS" << std::endl;
}

int main(int argc, char* argv[]) {
    // Command line options
    bool test_mode = false;
    bool network_only = false;
    bool disks_only = false;
    bool show_status = false;
    bool show_help = false;
    bool show_version = false;
    int monitor_interval = 30;
    std::string config_file;
    
    // Parse command line arguments
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"interval", required_argument, 0, 'i'},
        {"network-only", no_argument, 0, 'n'},
        {"disks-only", no_argument, 0, 'd'},
        {"test", no_argument, 0, 't'},
        {"config", required_argument, 0, 'c'},
        {"status", no_argument, 0, 's'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0}
    };
    
    int option_index = 0;
    int c;
    
    while ((c = getopt_long(argc, argv, "hi:ndtc:sv", long_options, &option_index)) != -1) {
        switch (c) {
            case 'h':
                show_help = true;
                break;
            case 'i':
                monitor_interval = std::atoi(optarg);
                if (monitor_interval <= 0) {
                    std::cerr << "Error: Invalid interval value" << std::endl;
                    return 1;
                }
                break;
            case 'n':
                network_only = true;
                break;
            case 'd':
                disks_only = true;
                break;
            case 't':
                test_mode = true;
                break;
            case 'c':
                config_file = optarg;
                break;
            case 's':
                show_status = true;
                break;
            case 'v':
                show_version = true;
                break;
            case '?':
                std::cerr << "Use -h for help" << std::endl;
                return 1;
            default:
                break;
        }
    }
    
    // Handle help/version options
    if (show_help) {
        print_usage();
        return 0;
    }
    
    if (show_version) {
        print_version();
        return 0;
    }
    
    // Check root privileges
    if (!SystemUtils::isRoot()) {
        std::cerr << "Error: This program requires root privileges for I2C access" << std::endl;
        std::cerr << "Please run with sudo: sudo " << argv[0] << std::endl;
        return 1;
    }
    
    // Validate options
    if (network_only && disks_only) {
        std::cerr << "Error: Cannot specify both --network-only and --disks-only" << std::endl;
        return 1;
    }
    
    try {
        // Create monitor instance
        monitor_instance = std::make_unique<UgreenMonitor>();
        
        // Load configuration
        monitor_instance->loadConfig(config_file);
        
        // Override config with command line options
        UgreenMonitorConfig config = monitor_instance->getConfig();
        config.monitor_interval = monitor_interval;
        
        if (network_only) {
            config.monitor_network = true;
            config.monitor_disks = false;
        } else if (disks_only) {
            config.monitor_network = false;
            config.monitor_disks = true;
        }
        
        monitor_instance->setConfig(config);
        
        // Initialize monitor
        if (!monitor_instance->initialize()) {
            std::cerr << "Error: Failed to initialize monitor" << std::endl;
            return 1;
        }
        
        // Handle status option
        if (show_status) {
            monitor_instance->showStatus();
            return 0;
        }
        
        // Set up signal handlers for graceful shutdown
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
        
        if (test_mode) {
            // Run single test cycle
            std::cout << "Running single test cycle..." << std::endl;
            monitor_instance->runSingleCheck();
            std::cout << "Test cycle completed." << std::endl;
        } else {
            // Start continuous monitoring
            monitor_instance->startMonitoring();
        }
        
        std::cout << "Monitor shutdown complete." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Error: Unknown exception occurred" << std::endl;
        return 1;
    }
    
    return 0;
}