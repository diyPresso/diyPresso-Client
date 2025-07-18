// diyPresso Client Settings Management - Platform support: macOS 13+ and Windows 10/11 only
#pragma once
#include "DpcDevice.h"
#include <string>
#include <map>
#include <nlohmann/json.hpp>

class DpcSettings {
public:
    // Settings type (key-value pairs like Python)
    using Settings = std::map<std::string, std::string>;

    // Constructor
    DpcSettings();
    ~DpcSettings();

    // Settings operations (requires connected device)
    Settings get_settings(DpcDevice& device);
    bool put_settings(DpcDevice& device, const Settings& settings);

    // File I/O operations
    bool save_to_file(const Settings& settings, const std::string& filename = "");
    Settings load_from_file(const std::string& filename);
    
    // High-level operations for firmware upload workflow
    bool backup_current_settings(DpcDevice& device, std::string& backup_filename);
    bool restore_settings_from_backup(DpcDevice& device, const std::string& backup_filename);

    // Validation and utilities
    bool validate_settings(const Settings& settings);
    size_t get_settings_count(const Settings& settings);
    void print_settings(const Settings& settings);

private:
    // Helper methods
    std::string generate_default_filename();
    Settings parse_settings_response(const std::vector<std::string>& lines);
    std::string format_settings_for_put(const Settings& settings);
    bool is_settable_key(const std::string& key);
    bool parse_boot_sequence(const std::vector<std::string>& boot_sequence_lines, Settings& settings);
}; 