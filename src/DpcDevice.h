// diyPresso Client Device Management - Platform support: macOS 13+ and Windows 10/11 only
#pragma once
#include "DpcSerial.h"
#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

class DpcDevice {
public:
    // Device info structure
    struct DeviceInfo {
        std::string port;
        std::string firmware_version;
        bool bootloader_mode;
        uint16_t vendor_id;
        uint16_t product_id;
        
        nlohmann::json to_json() const;
    };

    // Constructor and destructor
    DpcDevice();
    ~DpcDevice();

    // Device detection and connection
    bool find_and_connect(unsigned int baudrate = 115200);
    bool is_connected() const;
    void disconnect();

    // Device information
    DeviceInfo get_device_info() const;
    std::string get_firmware_version();
    bool supports_api() const;
    
    // Pre-1.6.2 firmware support
    std::vector<std::string> get_boot_sequence_lines() const;



    // Command/response protocol handling
    std::vector<std::string> send_command(const std::string& command, int timeout_seconds = 5);

    // Bootloader operations
    bool reset_to_bootloader();
    bool is_in_bootloader_mode() const;
    
    // Serial port access for firmware operations
    std::string get_port() const;
    
    // Low-level access for services (DpcSettings, DpcFirmware)
    DpcSerial& get_serial();
    
    // Verbose mode control
    void set_verbose(bool verbose);

private:
    std::unique_ptr<DpcSerial> serial_;
    DeviceInfo device_info_;
    bool connected_;
    bool verbose_;
    std::vector<std::string> boot_sequence_lines_; // Raw lines from boot sequence

    // Helper methods
    void update_device_info();
    void clear_device_info();
    std::string detect_pre_162_by_setpoint_lines();
    bool wait_for_boot_sequence_completion();
}; 