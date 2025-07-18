// diyPresso Client C++ - Platform support: macOS 13+ and Windows 10/11 only
#include <iostream>
#include <iomanip>
#include <CLI/CLI.hpp>
#include <chrono>
#include <thread>
#include <csignal>
#include "../core/DpcDevice.h"
#include "../core/DpcSerial.h"
#include "../core/DpcSettings.h"
#include "../core/DpcFirmware.h"
#include "../core/DpcDownload.h"
#include "../core/DpcColors.h"

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
    std::string upload_version = "latest";
    std::string upload_binary_url = "";
    auto upload_cmd = app.add_subcommand("upload-firmware", "Upload firmware to the diyPresso controller");
    upload_cmd->add_flag("-v,--verbose", g_verbose, "Enable verbose mode");
    upload_cmd->add_option("-b,--binary-file", firmware_path, "Skip download and use provided firmware binary file");
    upload_cmd->add_option("--bossac-file", bossac_path, "Specify the path to the bossac tool");
    upload_cmd->add_option("--version", upload_version, "Specific version/tag to download (default: latest)");
    upload_cmd->add_option("--binary-url", upload_binary_url, "Custom URL to download firmware from");
    upload_cmd->callback([&]() {
        if (!wait_for_device_connection(device)) {
            std::exit(1);
        }

        try {
            // Create firmware uploader
            DpcFirmware firmware_uploader(g_verbose);
            
            if (!firmware_uploader.uploadFirmware(&device, firmware_path, bossac_path, upload_version, upload_binary_url)) {
                std::cerr << DpcColors::error("Firmware upload failed!") << std::endl;
                std::exit(1);
            }
        } catch (const std::exception& e) {
            std::cerr << DpcColors::error("Error during firmware upload: " + std::string(e.what())) << std::endl;
            std::exit(1);
        }
    });

    // Download firmware command
    std::string download_version = "latest";
    std::string download_url = "";
    std::string download_output = "";
    bool check_version = false;
    bool list_versions = false;
    auto download_cmd = app.add_subcommand("download", "Download firmware from GitHub");
    download_cmd->add_flag("-v,--verbose", g_verbose, "Enable verbose mode");
    download_cmd->add_option("--version", download_version, "Specific version/tag to download or check.");
    download_cmd->add_option("--binary-url", download_url, "Custom URL to download firmware from");
    download_cmd->add_option("-o,--output", download_output, "Output file path (default: firmware.bin)");
    download_cmd->add_flag("--check", check_version, "Show firmware version information (use with --version for specific version, defaults to latest version)");
    download_cmd->add_flag("--list-versions", list_versions, "List all available firmware versions");
    download_cmd->callback([&]() {
        try {
            // Create download manager
            DpcDownload downloader(g_verbose);
            
            // Handle check version
            if (check_version) {
                std::string target_version = download_version;
                
                if (download_version == "latest") {
                    std::cout << DpcColors::highlight("=== Latest Firmware Information ===") << std::endl;
                    target_version = downloader.getLatestVersionTag();
                    if (target_version.empty()) {
                        std::cerr << DpcColors::error("Failed to get latest version from GitHub") << std::endl;
                        std::exit(1);
                    }
                    
                    auto release_info = downloader.getLatestRelease();
                    std::cout << "Latest version: " << target_version << std::endl;
                    
                    if (release_info.contains("published_at")) {
                        std::string datetime = release_info["published_at"].get<std::string>();
                        // Convert from ISO format to readable format
                        if (datetime.length() >= 19) {
                            std::string date = datetime.substr(0, 10);           // YYYY-MM-DD
                            std::string time = datetime.substr(11, 5);           // HH:MM
                            std::cout << "Released: " << date << " " << time << " UTC" << std::endl;
                        } else {
                            std::cout << "Released: " << datetime << std::endl;
                        }
                    }
                    
                    if (g_verbose && release_info.contains("body")) {
                        std::cout << "\nRelease Notes:" << std::endl;
                        std::cout << release_info["body"].get<std::string>() << std::endl;
                    }
                } else {
                    std::cout << DpcColors::highlight("=== Firmware Version Information ===") << std::endl;
                    std::cout << "Version: " << target_version << std::endl;
                }
                
                std::string download_url = downloader.buildDownloadUrl(target_version);
                std::cout << "Download URL: " << download_url << std::endl;
                
                return;
            }
            
            // Handle list versions
            if (list_versions) {
                std::cout << DpcColors::highlight("=== Available Firmware Versions ===") << std::endl;
                auto releases = downloader.getAllReleases();
                if (releases.empty()) {
                    std::cerr << DpcColors::error("Failed to get available versions from GitHub") << std::endl;
                    std::exit(1);
                }
                
                std::cout << "Available firmware versions:" << std::endl;
                for (size_t i = 0; i < releases.size(); ++i) {
                    const auto& release = releases[i];
                    
                    if (release.contains("tag_name")) {
                        std::string version = release["tag_name"].get<std::string>();
                        
                        // Build version display string
                        std::string version_display = version;
                        if (i == 0) {
                            version_display += " (latest)";
                        }
                        
                        // Left-align version in 12-character column + room for " (latest)"
                        std::cout << "  " << std::left << std::setw(21) << version_display;
                        
                        // Add date and time if available
                        if (release.contains("published_at")) {
                            std::string datetime = release["published_at"].get<std::string>();
                            if (datetime.length() >= 19) {
                                std::string date = datetime.substr(0, 10);  // YYYY-MM-DD
                                std::string time = datetime.substr(11, 5);  // HH:MM
                                std::cout << "  " << date << "  " << time << " UTC";
                            }
                        }
                        
                        std::cout << std::endl;
                    }
                    
                    // Limit output unless verbose
                    if (!g_verbose && i >= 19) {
                        std::cout << "  ... (" << (releases.size() - i - 1) << " more versions)" << std::endl;
                        std::cout << "  Use -v,--verbose to see all versions" << std::endl;
                        break;
                    }
                }
                return;
            }
            
            // Normal download behavior
            std::string downloaded_path = downloader.downloadFirmware(download_version, download_url, download_output);
            if (downloaded_path.empty()) {
                std::cerr << DpcColors::error("Firmware download failed!") << std::endl;
                std::exit(1);
            }
        } catch (const std::exception& e) {
            std::cerr << DpcColors::error("Error during firmware download: " + std::string(e.what())) << std::endl;
            std::exit(1);
        }
    });

    CLI11_PARSE(app, argc, argv);
    return 0;
} 