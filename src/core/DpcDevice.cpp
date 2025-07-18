// diyPresso Client Device Management - Platform support: macOS 13+ and Windows 10/11 only
#include "DpcDevice.h"
#include "DpcColors.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <sstream>

DpcDevice::DpcDevice() : serial_(std::make_unique<DpcSerial>()), connected_(false), verbose_(false) {
    clear_device_info();
}

DpcDevice::~DpcDevice() {
    disconnect();
}

bool DpcDevice::find_and_connect(unsigned int baudrate) {
    // Find the device
    bool bootloader_mode;
    std::string port = serial_->find_controller(bootloader_mode);
    
    if (port.empty()) {
        if (verbose_) {
            std::cerr << "Device not found" << std::endl;
        }
        return false;
    }

    // Open the serial connection
    if (!serial_->open(port, baudrate)) {
        std::cerr << "Failed to open serial port: " << port << std::endl;
        return false;
    }

    connected_ = true;
    
    // Update device info with the found information
    device_info_.port = port;
    device_info_.bootloader_mode = bootloader_mode;
    device_info_.vendor_id = DpcSerial::ARDUINO_VENDOR_ID;
    device_info_.product_id = bootloader_mode ? 
        DpcSerial::ARDUINO_MKR_WIFI_1010_PRODUCT_ID_BOOTLOADER : 
        DpcSerial::ARDUINO_MKR_WIFI_1010_PRODUCT_ID;
    
    // Get firmware version if not in bootloader mode
    if (!bootloader_mode) {
        device_info_.firmware_version = get_firmware_version();
    } else {
        device_info_.firmware_version = "bootloader";
    }

    return true;
}

bool DpcDevice::is_connected() const {
    return connected_ && serial_->is_open();
}

void DpcDevice::disconnect() {
    if (connected_) {
        serial_->close();
        connected_ = false;
        clear_device_info();
    }
}

DpcDevice::DeviceInfo DpcDevice::get_device_info() const {
    return device_info_;
}

std::string DpcDevice::get_firmware_version() {
    if (!is_connected() || device_info_.bootloader_mode) {
        return "unknown";
    }

    // Wait for device to finish boot sequence (first setpoint line)
    if (!wait_for_boot_sequence_completion()) {
        if (verbose_) {
            std::cout << "Boot sequence did not complete properly" << std::endl;
        }
        return "unknown";
    }

    // Boot sequence completed - now try API command to determine firmware version
    try {
        auto lines = send_command("GET info", 2);
        
        if (verbose_) {
            std::cout << "GET info command succeeded, got " << lines.size() << " lines:" << std::endl;
            for (const auto& line : lines) {
                std::cout << "  '" << line << "'" << std::endl;
            }
        }
        
        // Look for firmwareVersion=x.x.x
        for (const auto& line : lines) {
            if (line.find("firmwareVersion=") == 0) {
                return line.substr(16); // Remove "firmwareVersion="
            }
        }
        
        // Command succeeded but no firmwareVersion found - early API version
        return "1.6.2+"; // Has API but no version info
        
    } catch (const std::exception& e) {
        if (verbose_) {
            std::cout << "GET info command failed: " << e.what() << std::endl;
            std::cout << "This indicates pre-1.6.2 firmware (no API support)" << std::endl;
        }
        // API command failed - this is pre-1.6.2 firmware
        return "pre-1.6.2";
    }
}

std::string DpcDevice::detect_pre_162_by_setpoint_lines() {
    if (verbose_) {
        std::cout << "Checking for setpoint lines to detect pre-1.6.2 firmware..." << std::endl;
    }
    
    // Clear any previously cached boot lines
    boot_sequence_lines_.clear();
    
    // Wait up to 10 seconds for setpoint lines to appear
    auto start_time = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(10);
    int lines_checked = 0;
    
    while (std::chrono::steady_clock::now() - start_time < timeout) {
        if (serial_->is_open()) {
            std::string line = serial_->readline();
            lines_checked++;
            
            // Remove newline characters
            if (!line.empty() && line.back() == '\n') {
                line.pop_back();
            }
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            
            if (verbose_) {
                std::cout << "  Read line " << lines_checked << ": '" << line << "'" << std::endl;
            }
            
            // Store all lines for later parsing by DpcSettings
            boot_sequence_lines_.push_back(line);
            
            // Check if this line starts with "setpoint:" to confirm pre-1.6.2
            if (line.find("setpoint:") == 0) {
                if (verbose_) {
                    std::cout << "  Found setpoint line! Detected pre-1.6.2 firmware" << std::endl;
                    std::cout << "  Captured " << boot_sequence_lines_.size() << " lines from boot sequence" << std::endl;
                }
                return "pre-1.6.2";
            }
        } else {
            // No data available, wait a bit before checking again
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    if (verbose_) {
        std::cout << "  No setpoint lines found after checking " << lines_checked << " lines for 10 seconds" << std::endl;
        std::cout << "  Serial available: " << (serial_->is_open() ? "yes" : "no") << std::endl;
        std::cout << "  Captured " << boot_sequence_lines_.size() << " lines from boot sequence" << std::endl;
    }
    
    // No setpoint lines found
    return "unknown";
}

bool DpcDevice::supports_api() const {
    if (!is_connected() || device_info_.bootloader_mode) {
        return false;
    }
    
    // API is supported if firmware is NOT pre-1.6.2
    return device_info_.firmware_version != "pre-1.6.2";
}

std::vector<std::string> DpcDevice::get_boot_sequence_lines() const {
    return boot_sequence_lines_;
}

std::vector<std::string> DpcDevice::send_command(const std::string& command, int timeout_seconds) {
    if (!is_connected()) {
        throw std::runtime_error("Device not connected");
    }

    // Extract the command pattern (VERB object) to determine expected response
    std::string expected_ok_response;
    std::string expected_nok_response;
    
    // Parse command to get first two words (VERB object)
    std::istringstream iss(command);
    std::string verb, object;
    if (iss >> verb >> object) {
        expected_ok_response = verb + " " + object + " OK";
        expected_nok_response = verb + " " + object + " NOK";
    } else {
        throw std::runtime_error("Invalid command format: " + command);
    }

    // Send command
    serial_->write(command + "\n");
    
    std::vector<std::string> lines;
    auto start_time = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(timeout_seconds);
    
    while (std::chrono::steady_clock::now() - start_time < timeout) {
        if (serial_->is_open()) {
            std::string line = serial_->readline();
            
            // Remove newline characters
            if (!line.empty() && line.back() == '\n') {
                line.pop_back();
            }
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            
            // Skip lines starting with "setpoint:" (monitoring data)
            if (line.find("setpoint:") == 0) {
                continue;
            }
            
            lines.push_back(line);
            
            // Check for OK response (success)
            if (line.find(expected_ok_response) == 0) {
                return lines;
            }
            
            // Check for NOK response (failure)
            if (line.find(expected_nok_response) == 0) {
                throw std::runtime_error("Command failed: " + line);
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    throw std::runtime_error("Timeout waiting for response to: " + command);
}

bool DpcDevice::reset_to_bootloader() {
    if (!is_connected()) {
        return false;
    }

    std::string original_port = device_info_.port;
    
    if (verbose_) {
        std::cout << "Attempting to reset device to bootloader mode..." << std::endl;
        std::cout << "Original port: " << original_port << std::endl;
    }
    
    // Close current connection and ensure port is fully released
    serial_->close();
    connected_ = false;
    
    // Critical: Wait for the OS to fully release the port
    if (verbose_) {
        std::cout << "Waiting for port to be fully released..." << std::endl;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Use the working reset implementation from DpcSerial
    if (!DpcSerial::reset_to_bootloader(original_port, verbose_)) {
        if (verbose_) {
            std::cout << "Failed to send reset signal" << std::endl;
        }
        return false;
    }
    
    if (verbose_) {
        std::cout << "Reset signal sent, waiting for device re-enumeration..." << std::endl;
    }
    
    // Wait for device to reset and re-enumerate
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Search for device in bootloader mode
    bool bootloader_mode;
    std::string bootloader_port;
    
    // Search for up to 10 seconds for the bootloader to appear
    auto start_time = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(10);
    
    while (std::chrono::steady_clock::now() - start_time < timeout) {
        bootloader_port = serial_->find_controller(bootloader_mode);
        
        if (!bootloader_port.empty() && bootloader_mode) {
            if (verbose_) {
                std::cout << "Found device in bootloader mode on port: " << bootloader_port << std::endl;
            }
            
            // Connect to bootloader
            if (serial_->open(bootloader_port, 115200)) {
                connected_ = true;
                device_info_.port = bootloader_port;
                device_info_.bootloader_mode = true;
                device_info_.firmware_version = "bootloader";
                device_info_.vendor_id = DpcSerial::ARDUINO_VENDOR_ID;
                device_info_.product_id = DpcSerial::ARDUINO_MKR_WIFI_1010_PRODUCT_ID_BOOTLOADER;
                
                if (verbose_) {
                    std::cout << "Successfully connected to bootloader" << std::endl;
                }
                return true;
            }
        }
        
        // Wait a bit before trying again
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    if (verbose_) {
        std::cout << "Timeout - no bootloader found after reset" << std::endl;
    }
    
    return false;
}

bool DpcDevice::is_in_bootloader_mode() const {
    return device_info_.bootloader_mode;
}

std::string DpcDevice::get_port() const {
    return device_info_.port;
}

DpcSerial& DpcDevice::get_serial() {
    return *serial_;
}

void DpcDevice::set_verbose(bool verbose) {
    verbose_ = verbose;
    if (serial_) {
        serial_->set_verbose(verbose);
    }
}

// Private helper methods

void DpcDevice::update_device_info() {
    // This method is now mainly for refreshing firmware version
    // after the initial connection has been established
    if (!device_info_.bootloader_mode && is_connected()) {
        try {
            device_info_.firmware_version = get_firmware_version();
        } catch (...) {
            device_info_.firmware_version = "unknown";
        }
    }
}

void DpcDevice::clear_device_info() {
    device_info_.port = "";
    device_info_.firmware_version = "unknown";
    device_info_.bootloader_mode = false;
    device_info_.vendor_id = 0;
    device_info_.product_id = 0;
    boot_sequence_lines_.clear();
}

bool DpcDevice::wait_for_boot_sequence_completion() {
    if (verbose_) {
        std::cout << "Waiting for device boot sequence to complete..." << std::endl;
    }
    
    // Wait up to 10 seconds for the first "setpoint:" line (indicates boot complete for ALL firmware)
    auto start_time = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(10);
    int lines_checked = 0;
    
    while (std::chrono::steady_clock::now() - start_time < timeout) {
        if (serial_->is_open()) {
            std::string line = serial_->readline();
            lines_checked++;
            
            // Remove newline characters
            if (!line.empty() && line.back() == '\n') {
                line.pop_back();
            }
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            
            if (verbose_) {
                std::cout << "  Boot line " << lines_checked << ": '" << line << "'" << std::endl;
            }
            
            // Store all lines for later parsing by DpcSettings
            boot_sequence_lines_.push_back(line);
            
            // Check for first setpoint line - this indicates boot sequence is complete
            if (line.find("setpoint:") == 0) {
                if (verbose_) {
                    std::cout << "  Found first setpoint line - boot sequence completed!" << std::endl;
                }
                return true;
            }
        } else {
            // No data available, wait a bit before checking again
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    if (verbose_) {
        std::cout << "  Timeout: No setpoint lines found after " << lines_checked << " lines in 10 seconds" << std::endl;
        std::cout << "  Device may not be functioning properly" << std::endl;
        std::cout << "  Captured " << boot_sequence_lines_.size() << " lines from boot sequence" << std::endl;
    }
    
    // Timeout reached without seeing setpoint lines - device may have issues
    return false;
}

// DeviceInfo JSON conversion
nlohmann::json DpcDevice::DeviceInfo::to_json() const {
    return nlohmann::json{
        {"port", port},
        {"firmware_version", firmware_version},
        {"bootloader_mode", bootloader_mode},
        {"vendor_id", vendor_id},
        {"product_id", product_id}
    };
} 