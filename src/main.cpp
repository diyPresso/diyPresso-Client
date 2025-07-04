// diyPresso Client C++ - Platform support: macOS 13+ and Windows 10/11 only
#include <iostream>
#include <CLI/CLI.hpp>
#include <chrono>
#include <thread>
#include <csignal>
#include "DpcDevice.h"
#include "DpcSerial.h"
#include "DpcSettings.h"
#include "DpcFirmware.h"
#include "DpcColors.h"

const std::string VERSION = "1.0.0";

// Global variables
DpcDevice* g_device = nullptr;
volatile bool g_interrupted = false;
bool g_verbose = false;

void signal_handler(int signal) {
    g_interrupted = true;
    if (g_device) {
        g_device->disconnect();
    }
    if (signal == SIGINT) {
        std::cout << "\nOperation cancelled by user." << std::endl;
        std::exit(130);
    } else {
        std::exit(1);
    }
}

bool wait_for_device_connection(DpcDevice& device) {
    // Set verbose mode first so it's used during firmware detection
    device.set_verbose(g_verbose);
    
    std::cout << "Searching for diyPresso device..." << std::endl;
    
    // Try immediate connection first
    if (device.find_and_connect()) {
        return true;
    }
    
    // Device not found - start waiting process
    std::cout << "No diyPresso device found." << std::endl;
    std::cout << DpcColors::highlight("Please power OFF the diyPresso machine and connect the USB cable.") << std::endl;
    std::cout << "Waiting for device connection... (Ctrl+C to cancel)" << std::endl;
    
    const int timeout_seconds = 30;
    const int check_interval_ms = 500;
    const int max_attempts = (timeout_seconds * 1000) / check_interval_ms;
    
    int dots_printed = 0;
    for (int attempt = 0; attempt < max_attempts && !g_interrupted; ++attempt) {
        // Print progress dots
        if (attempt % 2 == 0) { // Every second (2 * 500ms)
            std::cout << "." << std::flush;
            dots_printed++;
            if (dots_printed >= 50) { // New line every 50 dots
                std::cout << std::endl;
                dots_printed = 0;
            }
        }
        
        // Check for device
        if (device.find_and_connect()) {
            std::cout << std::endl << DpcColors::ok("Device connected!") << std::endl;
            return true;
        }
        
        // Wait before next attempt
        std::this_thread::sleep_for(std::chrono::milliseconds(check_interval_ms));
    }
    
    if (g_interrupted) {
        return false;
    }
    
    std::cout << std::endl << "Timeout: No device found after " << timeout_seconds << " seconds." << std::endl;
    std::cout << "Please check:" << std::endl;
    std::cout << "- diyPresso machine is powered OFF" << std::endl;
    std::cout << "- USB cable is properly connected" << std::endl;
    return false;
}

void print_device_info(const DpcDevice::DeviceInfo& info) {
    std::cout << "Device Information:" << std::endl;
    std::cout << "  Port: " << info.port << " (VID: " << info.vendor_id << ", PID: " << info.product_id << ")" << std::endl;
    std::cout << "  In bootloader mode: " << (info.bootloader_mode ? "true" : "false") << std::endl;
    std::cout << "  Firmware Version: " << info.firmware_version << std::endl;
}

void check_bootloader_mode_error(const DpcDevice& device) {
    if (device.is_in_bootloader_mode()) {
        std::cerr << "\n" << DpcColors::error("The diyPresso is in bootloader mode. The requested action requires the device to be in normal mode.") << std::endl;
        std::cerr << "Restart the device to try to switch to normal operation." << std::endl;
        std::cerr << "Alternatively, use the upload-firmware action to upload (new) firmware." << std::endl;
        std::exit(1);
    }
}

int main(int argc, char** argv) {
    // Set up signal handling
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    CLI::App app{std::string("diyPresso management client v") + VERSION};
    app.set_help_all_flag("--help-all", "Expand all help");
    app.description("Manage diyPresso espresso machine controllers");
    app.usage("diypresso <SUBCOMMAND> [OPTIONS]");

    // Create device instance
    DpcDevice device;
    DpcSettings settings_manager;
    g_device = &device;

    // Info command
    auto info_cmd = app.add_subcommand("info", "Print device info from the diyPresso machine");
    info_cmd->add_flag("-v,--verbose", g_verbose, "Enable verbose mode");
    info_cmd->callback([&]() {
        if (!wait_for_device_connection(device)) {
            std::exit(1);
        }

        check_bootloader_mode_error(device);

        auto info = device.get_device_info();
        print_device_info(info);
    });

    // Monitor command
    auto monitor_cmd = app.add_subcommand("monitor", "Monitor the serial output from the diyPresso");
    monitor_cmd->add_flag("-v,--verbose", g_verbose, "Enable verbose mode");
    monitor_cmd->callback([&]() {
        if (!DpcSerial::simple_monitor(g_verbose)) {
            std::exit(1);
        }
    });

    // Get settings command
    auto get_settings_cmd = app.add_subcommand("get-settings", "Print the settings from the diyPresso");
    get_settings_cmd->add_flag("-v,--verbose", g_verbose, "Enable verbose mode");
    get_settings_cmd->callback([&]() {
        if (!wait_for_device_connection(device)) {
            std::exit(1);
        }

        check_bootloader_mode_error(device);

        try {
            std::cout << "Getting settings..." << std::endl;
            auto settings = settings_manager.get_settings(device);
            
            settings_manager.print_settings(settings);
            
            // Save to file automatically
            if (settings_manager.save_to_file(settings)) {
                std::cout << "\nSettings retrieved and saved successfully." << std::endl;
            }
            
            // Validate settings
            if (!settings_manager.validate_settings(settings)) {
                std::cerr << "Settings validation failed." << std::endl;
                std::exit(1);
            }
        } catch (const std::exception& e) {
            std::cerr << "Error getting settings: " << e.what() << std::endl;
            std::exit(1);
        }
    });

    // Restore settings command
    std::string settings_file = "";
    auto restore_settings_cmd = app.add_subcommand("restore-settings", "Restore the settings to the diyPresso");
    restore_settings_cmd->add_flag("-v,--verbose", g_verbose, "Enable verbose mode");
    restore_settings_cmd->add_option("--settings-file", settings_file, "Specify the path to the settings file")->required();
    restore_settings_cmd->callback([&]() {
        if (!wait_for_device_connection(device)) {
            std::exit(1);
        }

        check_bootloader_mode_error(device);

        try {
            std::cout << "Loading settings from file: " << settings_file << std::endl;
            auto settings = settings_manager.load_from_file(settings_file);
            
            std::cout << "Loaded " << settings_manager.get_settings_count(settings) << " settings from file." << std::endl;
            
            if (g_verbose) {
                settings_manager.print_settings(settings);
            }
            
            std::cout << "Restoring settings to device..." << std::endl;
            if (settings_manager.put_settings(device, settings)) {
                std::cout << "Settings restored successfully." << std::endl;
            } else {
                std::cerr << "Failed to restore settings." << std::endl;
                std::exit(1);
            }
        } catch (const std::exception& e) {
            std::cerr << "Error restoring settings: " << e.what() << std::endl;
            std::exit(1);
        }
    });

    // Upload firmware command
    std::string firmware_path = "";
    std::string bossac_path = "";
    auto upload_cmd = app.add_subcommand("upload-firmware", "Upload firmware to the diyPresso controller");
    upload_cmd->add_flag("-v,--verbose", g_verbose, "Enable verbose mode");
    upload_cmd->add_option("-b,--binary-file", firmware_path, "Specify the path to the firmware binary");
    upload_cmd->add_option("--bossac-file", bossac_path, "Specify the path to the bossac tool");
    upload_cmd->callback([&]() {
        if (!wait_for_device_connection(device)) {
            std::exit(1);
        }

        try {
            // Create firmware uploader
            DpcFirmware firmware_uploader(g_verbose);
            
            if (!firmware_uploader.uploadFirmware(&device, firmware_path, bossac_path)) {
                std::cerr << DpcColors::error("Firmware upload failed!") << std::endl;
                std::exit(1);
            }
        } catch (const std::exception& e) {
            std::cerr << DpcColors::error("Error during firmware upload: " + std::string(e.what())) << std::endl;
            std::exit(1);
        }
    });

    CLI11_PARSE(app, argc, argv);
    return 0;
} 