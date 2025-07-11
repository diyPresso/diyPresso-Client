// diyPresso Client Download - Platform support: macOS 13+ and Windows 10/11 only
#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

class DpcDownload {
public:
    // Constructor
    DpcDownload(bool verbose = false);
    
    // Main download functionality
    bool downloadFirmware(const std::string& version = "latest", 
                         const std::string& customUrl = "",
                         const std::string& outputPath = "");
    
    // Utility methods
    bool checkExistingFirmware(const std::string& outputPath = "");
    std::string getLatestVersionTag();
    std::vector<std::string> getAvailableVersions();
    
    // GitHub API methods
    nlohmann::json getLatestRelease();
    nlohmann::json getAllReleases();
    std::string buildDownloadUrl(const std::string& version);
    
    // File operations
    bool downloadFile(const std::string& url, const std::string& outputPath);
    bool validateFirmwareFile(const std::string& filePath);
    std::string backupExistingFile(const std::string& filePath);
    bool filesAreIdentical(const std::string& file1, const std::string& file2);
    bool removeFile(const std::string& filePath);
    
    // User interaction
    bool promptOverwriteExisting(const std::string& filePath);
    
private:
    bool m_verbose;
    
    // Constants
    static constexpr const char* GITHUB_API_BASE = "https://api.github.com/repos/diyPresso/diyPresso-One";
    static constexpr const char* GITHUB_DOWNLOAD_BASE = "https://github.com/diyPresso/diyPresso-One/releases/download";
    static constexpr const char* FIRMWARE_FILENAME = "firmware.bin";
    static constexpr const char* DEFAULT_OUTPUT_PATH = "firmware.bin";
    
    // Helper methods
    std::string getDefaultOutputPath();
    bool fileExists(const std::string& path);
    std::string getCurrentTimestamp();
    void printProgress(size_t downloaded, size_t total);
    bool isValidVersion(const std::string& version);
    std::string sanitizeVersion(const std::string& version);
}; 