#include "DpcFirmware.h"
#include "DpcSettings.h"
#include "DpcDevice.h"
#include "DpcColors.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <algorithm>


#ifdef _WIN32
#include <windows.h>
#include <filesystem>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

DpcFirmware::DpcFirmware(bool verbose) : m_verbose(verbose) {
}

bool DpcFirmware::uploadFirmware(DpcDevice* device, const std::string& firmwarePath, const std::string& bossacPath) {
    std::cout << DpcColors::highlight("=== diyPresso Firmware Upload ===") << std::endl;
    
    // Step 0.1: Prepare settings manager and backup filename
    DpcSettings settingsManager;
    std::string backupFilename;
    bool skip_settings = false;
    
    // Step 0.2: Determine paths
    std::string finalFirmwarePath = firmwarePath.empty() ? getFirmwarePath() : firmwarePath;
    std::string finalBossacPath = bossacPath.empty() ? getBossacPath() : bossacPath;
    
    if (m_verbose) {
        std::cout << "Firmware path: " << finalFirmwarePath << std::endl;
        std::cout << "Bossac path: " << finalBossacPath << std::endl;
    }
    
    // Step 1.1: Print and check bossac executable
    std::cout << std::endl << DpcColors::step("Step 1/6: Checking bossac executable...") << std::endl;
    if (!checkBossacExecutable(finalBossacPath)) {
        std::cerr << DpcColors::error("bossac executable not found or not accessible at: " + finalBossacPath) << std::endl;
        return false;
    }
    std::cout << DpcColors::ok("bossac executable found and accessible") << std::endl;
    
    // Step 2.1: Print and check firmware file
    std::cout << std::endl << DpcColors::step("Step 2/6: Checking firmware file...") << std::endl;
    if (!checkFirmwareFile(finalFirmwarePath)) {
        std::cerr << DpcColors::error("Firmware file not found at: " + finalFirmwarePath) << std::endl;
        return false;
    }
    std::cout << DpcColors::ok("Firmware file found: " + finalFirmwarePath) << std::endl;
    
    std::cout << std::endl << DpcColors::step("Step 3/6: Retrieving and backing up current settings...") << std::endl;
    
    // Step 3.1: Validate device pointer
    if (!device) {
        std::cerr << DpcColors::error("No device provided") << std::endl;
        return false;
    }
    
    // Step 3.2: Check if device is in bootloader mode
    if (device->is_in_bootloader_mode()) {
        std::cout << DpcColors::warning("Device is already in bootloader mode. Settings cannot be retrieved and restored.") << std::endl;
        std::cout << std::endl;
        
        std::string choice;
        std::cout << "Do you want to continue firmware upload without settings backup and restore? (y/N): ";
        std::getline(std::cin, choice);
        
        // Convert to lowercase
        std::transform(choice.begin(), choice.end(), choice.begin(), ::tolower);
        
        if (choice == "y" || choice == "yes") {
            skip_settings = true;
            std::cout << DpcColors::warning("Proceeding without settings backup/restore...") << std::endl;
        } else {
            std::cout << "Firmware upload cancelled." << std::endl;
            std::cout << "Please restart the device to normal mode and try again." << std::endl;
            return false;
        }
    }
    
    // Step 3.4: Retrieve settings and save as backup (only if not skipping)
    if (!skip_settings) {
        if (!settingsManager.backup_current_settings(*device, backupFilename)) {
            std::cerr << DpcColors::error("Failed to backup current settings") << std::endl;
            return false;
        }
        std::cout << DpcColors::ok("Settings backed up to: " + backupFilename) << std::endl;
    }
    
    // Step 4: Put device in bootloader mode
    std::cout << std::endl << DpcColors::step("Step 4/6: Putting device in bootloader mode...") << std::endl;
    
    if (!device->is_in_bootloader_mode()) {
        if (!device->reset_to_bootloader()) {
            std::cerr << DpcColors::error("Failed to reset device into bootloader mode") << std::endl;
            return false;
        }
        std::cout << DpcColors::ok("Device successfully entered bootloader mode") << std::endl;
    } else {
        std::cout << DpcColors::ok("Device already in bootloader mode") << std::endl;
    }
    std::string port = device->get_port();
    if (m_verbose) {
        std::cout << "  New port: " << port << std::endl;
    }
    
    // Disconnect from the device to release the COM port
    std::cout << "Releasing COM port for bossac access..." << std::endl;
    device->disconnect();
    
    // Step 5: Upload firmware (build and execute bossac command)
    std::cout << std::endl << DpcColors::step("Step 5/6: Uploading firmware...") << std::endl;
    
    //  Wait for device to stabilize in bootloader mode
    std::cout << "Waiting for device to stabilize in bootloader mode..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Step 5.1: Build and execute bossac command
    std::cout << "Using bootloader port for firmware upload: " << port << std::endl;
    std::string bossacCommand = buildBossacCommand(finalBossacPath, port, finalFirmwarePath);
    
    if (m_verbose) {
        std::cout << std::endl;
        std::cout << "Bossac command:" << std::endl;
        std::cout << "===============" << std::endl;
        std::cout << bossacCommand << std::endl;
        std::cout << "===============" << std::endl;
        std::cout << std::endl;
    }
    
    // Step 5.2: Upload firmware
    std::cout << "Uploading firmware to device..." << std::endl;
    if (!executeBossacCommand(bossacCommand)) {
        std::cerr << DpcColors::error("Firmware upload failed") << std::endl;
        return false;
    }
    
    std::cout << DpcColors::ok("Firmware uploaded successfully") << std::endl;
    
    // Step 6: Waiting for device reboot and restore settings
    std::cout << std::endl << DpcColors::step("Step 6/6: Waiting for device reboot and restoring settings...") << std::endl;
    
    std::cout << "Waiting for device to reboot..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(4));
    
    bool settings_restored = false;
    
    try {
        // Step 6.1: Reconnect to the device (port may have changed after firmware upload)
        if (!device->find_and_connect()) {
            std::cerr << DpcColors::warning("Could not reconnect to device after firmware upload") << std::endl;
            std::cerr << "         Settings were backed up but not restored" << std::endl;
        } else if (device->is_in_bootloader_mode()) {
            std::cerr << DpcColors::warning("Device still in bootloader mode after firmware upload") << std::endl;
            std::cerr << "         Settings were backed up but not restored" << std::endl;
        } else {
            std::cout << DpcColors::ok("Device reconnected successfully") << std::endl;
            
            // Step 6.2: Restore settings from backup file (only if we backed up settings)
            if (!skip_settings) {
                if (settingsManager.restore_settings_from_backup(*device, backupFilename)) {
                    settings_restored = true;
                } else {
                    std::cerr << DpcColors::warning("Failed to restore settings to device") << std::endl;
                    std::cerr << "         Settings backup file can be used for manual restoration" << std::endl;
                }
            } else {
                std::cout << DpcColors::warning("No settings backup available - skipping restore") << std::endl;
                settings_restored = false;
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << DpcColors::warning("Exception during settings restore: " + std::string(e.what())) << std::endl;
        std::cerr << "         Settings backup file can be used for manual restoration" << std::endl;
    }
    
    std::cout << std::endl;
    if (settings_restored) {
        std::cout << DpcColors::ok("Firmware upload completed successfully and device settings restored!") << std::endl;
    } else {
        std::cout << DpcColors::warning("Firmware upload completed successfully, device settings NOT restored.") << std::endl;
        if (!skip_settings) {
            std::cout << "Use the settings backup file for restoration with the restore-settings command if desired." << std::endl;
        }
    }
    
    return true;
}

bool DpcFirmware::checkBossacExecutable(const std::string& bossacPath) {
    std::string finalPath = bossacPath.empty() ? getBossacPath() : bossacPath;
    
    if (!fileExists(finalPath)) {
        if (m_verbose) {
            std::cerr << "Bossac file not found at: " << finalPath << std::endl;
        }
        return false;
    }
    
    // Test bossac executable by running it with no arguments
    // This should return an error but prove the executable works
    std::string testCommand = "\"" + finalPath + "\"";
    
    if (m_verbose) {
        std::cout << "Testing bossac with command: " << testCommand << std::endl;
    }
    
    // For now, just check file existence
    // In real implementation, you'd run: system(testCommand.c_str())
    // and check if it returns a reasonable error code
    
    return true; // Assume it works if file exists
}

bool DpcFirmware::checkFirmwareFile(const std::string& firmwarePath) {
    std::string finalPath = firmwarePath.empty() ? getFirmwarePath() : firmwarePath;
    
    if (!fileExists(finalPath)) {
        if (m_verbose) {
            std::cerr << "Firmware file not found at: " << finalPath << std::endl;
        }
        return false;
    }
    
    // Check file size - firmware should be reasonably large (> 1KB)
    std::ifstream file(finalPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        if (m_verbose) {
            std::cerr << "Cannot open firmware file: " << finalPath << std::endl;
        }
        return false;
    }
    
    std::streamsize size = file.tellg();
    file.close();
    
    if (size < 1024) { // Less than 1KB seems too small for firmware
        if (m_verbose) {
            std::cerr << "Firmware file seems too small: " << size << " bytes" << std::endl;
        }
        return false;
    }
    
    if (m_verbose) {
        std::cout << "Firmware file size: " << size << " bytes" << std::endl;
    }
    
    return true;
}

std::string DpcFirmware::getFirmwarePath() {
    std::string execDir = getExecutableDirectory();
    
#ifdef _WIN32
    const char* separator = "\\";
#else
    const char* separator = "/";
#endif
    
    // Try development path first (from project root)
    std::string devPath = execDir + separator + DEFAULT_DEV_FIRMWARE_PATH;
    if (fileExists(devPath)) {
        return devPath;
    }
    
    // Try production path (relative to executable)
    std::string prodPath = execDir + separator + DEFAULT_FIRMWARE_PATH;
    if (fileExists(prodPath)) {
        return prodPath;
    }
    
    // Return default path even if it doesn't exist (for error reporting)
    return devPath;
}

std::string DpcFirmware::getBossacPath() {
    std::string execDir = getExecutableDirectory();
    
#ifdef _WIN32
    const char* separator = "\\";
#else
    const char* separator = "/";
#endif
    
    // Try development path first (from project root)
    std::string devPath = execDir + separator + DEFAULT_DEV_BOSSAC_PATH;
    if (fileExists(devPath)) {
        return devPath;
    }
    
    // Try production path (relative to executable)
    std::string prodPath = execDir + separator + DEFAULT_BOSSAC_PATH;
    if (fileExists(prodPath)) {
        return prodPath;
    }
    
    // Return default path even if it doesn't exist (for error reporting)
    return devPath;
}

std::string DpcFirmware::buildBossacCommand(const std::string& bossacPath, const std::string& port, const std::string& firmwarePath) {
    // Critical bossac command from Python implementation:
    // bossac --info --port "cu.usbmodem11301" --write --verify --reset --erase -U true firmware.bin
    
    std::ostringstream cmd;
    cmd << "\"" << bossacPath << "\""
        << " --info"
        << " --port "
        << "\"" << port << "\""
        << " --write"
        << " --verify" 
        << " --reset"
        << " --erase"
        << " -U true"
        << " \"" << firmwarePath << "\"";
    
    return cmd.str();
}

bool DpcFirmware::executeBossacCommand(const std::string& command) {
    if (m_verbose) {
        std::cout << "Executing: " << command << std::endl;
        std::cout << "Command length: " << command.length() << " characters" << std::endl;
        std::cout << "Current working directory: " << getExecutableDirectory() << std::endl;
    }
    
    // On Windows, use cmd /c to properly handle quoted commands
    std::string finalCommand;
#ifdef _WIN32
    finalCommand = "cmd /c \"" + command + "\"";
#else
    finalCommand = command;
#endif
    
    if (m_verbose) {
        std::cout << "Final command: " << finalCommand << std::endl;
    }
    
    int result = system(finalCommand.c_str());
    
    if (m_verbose) {
        std::cout << "Command result: " << result << std::endl;
    }
    
    return result == 0;
}

bool DpcFirmware::fileExists(const std::string& path) {
    std::ifstream file(path);
    return file.good();
}

std::string DpcFirmware::getExecutableDirectory() {
#ifdef _WIN32
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string execPath(buffer);
    return execPath.substr(0, execPath.find_last_of("\\/"));
#else
    // On macOS, try different approaches to get executable path
    char buffer[1024];
    
    // Try /proc/self/exe (Linux)
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len != -1) {
        buffer[len] = '\0';
        std::string execPath(buffer);
        return execPath.substr(0, execPath.find_last_of("/"));
    }
    
    // Fallback to current directory for macOS
    return ".";
#endif
} 