#pragma once

#include <string>

class DpcSerial;
class DpcDevice;

class DpcFirmware {
public:
    DpcFirmware(bool verbose = false);
    
    // Main firmware upload function
    bool uploadFirmware(DpcDevice* device, const std::string& firmwarePath = "", const std::string& bossacPath = "");
    
    // Check if bossac executable exists and is accessible
    bool checkBossacExecutable(const std::string& bossacPath = "");
    
    // Check if firmware file exists
    bool checkFirmwareFile(const std::string& firmwarePath = "");
    
    // Static helper functions to get default paths
    static std::string getFirmwarePath();
    static std::string getBossacPath();
    
private:
    bool m_verbose;
    
    // Helper functions
    std::string buildBossacCommand(const std::string& bossacPath, const std::string& port, const std::string& firmwarePath);
    bool executeBossacCommand(const std::string& command);
    static bool fileExists(const std::string& path);
    static std::string getExecutableDirectory();
    
    // Platform-specific path constants
    static constexpr const char* DEFAULT_FIRMWARE_PATH = "firmware.bin";
    
#ifdef _WIN32
    static constexpr const char* DEFAULT_DEV_FIRMWARE_PATH = "bin\\firmware\\firmware.bin";
    static constexpr const char* DEFAULT_BOSSAC_PATH = "bossac.exe";
    static constexpr const char* DEFAULT_DEV_BOSSAC_PATH = "bin\\bossac\\bossac.exe";
#else
    static constexpr const char* DEFAULT_DEV_FIRMWARE_PATH = "bin/firmware/firmware.bin";
    static constexpr const char* DEFAULT_BOSSAC_PATH = "bossac";
    static constexpr const char* DEFAULT_DEV_BOSSAC_PATH = "bin/bossac/bossac";
#endif
}; 