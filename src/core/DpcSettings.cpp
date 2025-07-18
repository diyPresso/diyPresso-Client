// diyPresso Client Settings Management - Platform support: macOS 13+ and Windows 10/11 only
#include "DpcSettings.h"
#include "DpcColors.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <chrono>
#include <iomanip>

DpcSettings::DpcSettings() {}

DpcSettings::~DpcSettings() {}

DpcSettings::Settings DpcSettings::get_settings(DpcDevice& device) {
    if (!device.is_connected()) {
        throw std::runtime_error("Device not connected");
    }

    if (device.is_in_bootloader_mode()) {
        throw std::runtime_error("Device is in bootloader mode - settings not available");
    }

    // Check if this is pre-1.6.2 firmware
    if (device.get_device_info().firmware_version == "pre-1.6.2") {
        // Check if we have captured settings from boot sequence
        auto boot_lines = device.get_boot_sequence_lines();
        
        if (!boot_lines.empty()) {
            // Parse the boot sequence lines using the existing parser
            Settings settings = parse_settings_response(boot_lines);
            
            if (!settings.empty()) {
                // Check for commissioning status and add commissioningDone if needed
                parse_boot_sequence(boot_lines, settings);
                
                std::cout << DpcColors::ok("Found " + std::to_string(settings.size()) + " settings from boot sequence") << std::endl;
                return settings;
            }
        }
        
        // No settings captured from boot sequence - show instructions
        std::cout << std::endl;
        std::cout << "=== Pre-1.6.2 Firmware Detected ===" << std::endl;
        std::cout << DpcColors::highlight("Disconnect the USB cable and restart this application (with the USB cable disconnected)") << std::endl;
        std::cout << std::endl;
        
        std::exit(0);
    }

    // Send GET settings command and wait for response (1.6.2+ firmware)
    auto lines = device.send_command("GET settings", 5);
    Settings settings = parse_settings_response(lines);
    
    // Check for commissioning status using boot sequence (available for all firmware)
    auto boot_lines = device.get_boot_sequence_lines();
    if (!boot_lines.empty()) {
        parse_boot_sequence(boot_lines, settings);
    }
    
    return settings;
}

bool DpcSettings::put_settings(DpcDevice& device, const Settings& settings) {
    if (!device.is_connected()) {
        throw std::runtime_error("Device not connected");
    }

    if (device.is_in_bootloader_mode()) {
        throw std::runtime_error("Device is in bootloader mode - cannot set settings");
    }

    if (settings.empty()) {
        std::cerr << "Warning: No settings to send" << std::endl;
        return false;
    }

    // Format settings for PUT command
    std::string settings_string = format_settings_for_put(settings);
    std::string command = "PUT settings " + settings_string;

    try {
        auto lines = device.send_command(command, 5);
        
        // If send_command returned without exception, the command was successful
        // (send_command already checked for "PUT settings OK" start)
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error sending settings: " << e.what() << std::endl;
        return false;
    }
}

bool DpcSettings::save_to_file(const Settings& settings, const std::string& filename) {
    std::string output_file = filename.empty() ? generate_default_filename() : filename;

    try {
        nlohmann::json json_settings(settings);

        std::ofstream file(output_file);
        if (!file.is_open()) {
            std::cerr << "Error: Could not create file: " << output_file << std::endl;
            return false;
        }

        file << json_settings.dump(4) << std::endl;
        std::cout << "Settings saved to: " << output_file << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error saving settings to file: " << e.what() << std::endl;
        return false;
    }
}

DpcSettings::Settings DpcSettings::load_from_file(const std::string& filename) {
    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Could not open settings file: " + filename);
        }

        nlohmann::json json_settings;
        file >> json_settings;

        Settings settings;
        for (auto& [key, value] : json_settings.items()) {
            // Ensure all values are stored as strings
            if (value.is_string()) {
                settings[key] = value.get<std::string>();
            } else {
                settings[key] = value.dump(); // Convert non-strings to JSON string
            }
        }

        return settings;
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to load settings from file: " + filename + " (" + e.what() + ")");
    }
}

bool DpcSettings::backup_current_settings(DpcDevice& device, std::string& backup_filename) {
    try {
        // Get current settings from device
        Settings currentSettings = get_settings(device);
        
        // Validate settings
        if (!validate_settings(currentSettings)) {
            return false;
        }
        
        // Generate filename and save settings to backup file
        backup_filename = generate_default_filename();
        if (!save_to_file(currentSettings, backup_filename)) {
            return false;
        }
        
        std::cout << DpcColors::ok("Retrieved " + std::to_string(currentSettings.size()) + " settings from device") << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << DpcColors::error("Failed to backup settings: " + std::string(e.what())) << std::endl;
        return false;
    }
}

bool DpcSettings::restore_settings_from_backup(DpcDevice& device, const std::string& backup_filename) {
    try {
        // Load settings from backup file
        Settings settings = load_from_file(backup_filename);
        
        // Validate the loaded settings
        if (!validate_settings(settings)) {
            std::cerr << "ERROR: Backup file contains invalid settings" << std::endl;
            return false;
        }
        
        std::cout << "Restoring " << settings.size() << " settings to device..." << std::endl;
        
        // Restore settings to device
        if (!put_settings(device, settings)) {
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        return false;
    }
}

bool DpcSettings::validate_settings(const Settings& settings) {
    if (settings.empty()) {
        std::cerr << "Error: Settings are empty" << std::endl;
        return false;
    }

    // Exit if fewer than 12 settings are found
    if (settings.size() < 12) {
        std::cerr << "Error: Only " << settings.size() << " settings found, expected at least 12" << std::endl;
        return false;
    }

    return true;
}

size_t DpcSettings::get_settings_count(const Settings& settings) {
    return settings.size();
}

void DpcSettings::print_settings(const Settings& settings) {
    std::cout << "Settings (" << settings.size() << " entries):" << std::endl;
    for (const auto& [key, value] : settings) {
        std::cout << "  " << key << " = " << value << std::endl;
    }
}

// Private helper methods

std::string DpcSettings::generate_default_filename() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << "settings_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S") << ".json";
    return ss.str();
}

DpcSettings::Settings DpcSettings::parse_settings_response(const std::vector<std::string>& lines) {
    Settings settings;
    std::regex pattern(R"((\w+)=(.+))");

    for (const auto& line : lines) {
        // Stop at end marker
        if (line == "GET settings OK") {
            break;
        }

        // Parse key=value pairs
        std::smatch match;
        if (std::regex_match(line, match, pattern)) {
            std::string key = match[1].str();
            std::string value = match[2].str();
            settings[key] = value;
        }
    }

    return settings;
}

std::string DpcSettings::format_settings_for_put(const Settings& settings) {
    std::stringstream ss;
    bool first = true;

    for (const auto& [key, value] : settings) {
        // Skip non-settable keys
        if (!is_settable_key(key)) {
            continue;
        }

        if (!first) {
            ss << ",";
        }
        ss << key << "=" << value;
        first = false;
    }

    return ss.str();
}

bool DpcSettings::is_settable_key(const std::string& key) {
    // Skip 'crc' and 'version'
    if (key == "crc" || key == "version") {
        return false;
    }
    return true;
}

bool DpcSettings::parse_boot_sequence(const std::vector<std::string>& boot_sequence_lines, Settings& settings) {
    bool device_is_commissioned = false;

    // Check setpoint lines for commissioning status
    for (const auto& line : boot_sequence_lines) {
        if (line.find("setpoint:") == 0 && line.find("brew-state:idle") != std::string::npos) {
            device_is_commissioned = true;
            break;
        }
    }

    // Only add if device is commissioned AND setting doesn't already exist
    if (device_is_commissioned && settings.find("commissioningDone") == settings.end()) {
        settings["commissioningDone"] = "1";
    }

    return true;
}