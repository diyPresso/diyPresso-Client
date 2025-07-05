#include "DpcDownload.h"
#include "DpcColors.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>

DpcDownload::DpcDownload(bool verbose) : m_verbose(verbose) {
}

bool DpcDownload::downloadFirmware(const std::string& version, const std::string& customUrl, const std::string& outputPath) {
    std::cout << DpcColors::highlight("=== diyPresso Firmware Download ===") << std::endl;
    
    // Determine output path
    std::string finalOutputPath = outputPath.empty() ? getDefaultOutputPath() : outputPath;
    
    if (m_verbose) {
        std::cout << "Output path: " << finalOutputPath << std::endl;
    }
    
    // Check if firmware already exists
    if (checkExistingFirmware(finalOutputPath)) {
        if (!promptOverwriteExisting(finalOutputPath)) {
            std::cout << "Download cancelled by user." << std::endl;
            return false;
        }
        
        // Backup existing file
        if (!backupExistingFile(finalOutputPath)) {
            std::cerr << DpcColors::warning("Failed to backup existing firmware file") << std::endl;
        }
    }
    
    // Determine download URL
    std::string downloadUrl;
    if (!customUrl.empty()) {
        downloadUrl = customUrl;
        std::cout << "Using custom URL: " << downloadUrl << std::endl;
    } else {
        std::string targetVersion = version;
        if (version == "latest") {
            targetVersion = getLatestVersionTag();
            if (targetVersion.empty()) {
                std::cerr << DpcColors::error("Failed to get latest version from GitHub") << std::endl;
                return false;
            }
            std::cout << "Latest version: " << targetVersion << std::endl;
        }
        
        downloadUrl = buildDownloadUrl(targetVersion);
        std::cout << "Downloading firmware version: " << targetVersion << std::endl;
    }
    
    if (m_verbose) {
        std::cout << "Download URL: " << downloadUrl << std::endl;
    }
    
    // Download the file
    std::cout << "Downloading firmware..." << std::endl;
    if (!downloadFile(downloadUrl, finalOutputPath)) {
        std::cerr << DpcColors::error("Failed to download firmware") << std::endl;
        return false;
    }
    
    // Validate downloaded file
    if (!validateFirmwareFile(finalOutputPath)) {
        std::cerr << DpcColors::error("Downloaded firmware file validation failed") << std::endl;
        return false;
    }
    
    std::cout << DpcColors::ok("Firmware downloaded successfully to: " + finalOutputPath) << std::endl;
    return true;
}

bool DpcDownload::checkExistingFirmware(const std::string& outputPath) {
    std::string path = outputPath.empty() ? getDefaultOutputPath() : outputPath;
    return fileExists(path);
}

std::string DpcDownload::getLatestVersionTag() {
    try {
        auto releaseInfo = getLatestRelease();
        if (releaseInfo.contains("tag_name")) {
            return releaseInfo["tag_name"];
        }
    } catch (const std::exception& e) {
        if (m_verbose) {
            std::cerr << "Error getting latest version: " << e.what() << std::endl;
        }
    }
    return "";
}

std::vector<std::string> DpcDownload::getAvailableVersions() {
    std::vector<std::string> versions;
    try {
        auto releases = getAllReleases();
        for (const auto& release : releases) {
            if (release.contains("tag_name")) {
                versions.push_back(release["tag_name"]);
            }
        }
    } catch (const std::exception& e) {
        if (m_verbose) {
            std::cerr << "Error getting available versions: " << e.what() << std::endl;
        }
    }
    return versions;
}

nlohmann::json DpcDownload::getLatestRelease() {
    std::string url = std::string(GITHUB_API_BASE) + "/releases/latest";
    
    if (m_verbose) {
        std::cout << "Fetching latest release info from: " << url << std::endl;
    }
    
    cpr::Response r = cpr::Get(cpr::Url{url});
    
    if (r.status_code != 200) {
        throw std::runtime_error("HTTP request failed with status: " + std::to_string(r.status_code));
    }
    
    return nlohmann::json::parse(r.text);
}

nlohmann::json DpcDownload::getAllReleases() {
    std::string url = std::string(GITHUB_API_BASE) + "/releases";
    
    if (m_verbose) {
        std::cout << "Fetching all releases from: " << url << std::endl;
    }
    
    cpr::Response r = cpr::Get(cpr::Url{url});
    
    if (r.status_code != 200) {
        throw std::runtime_error("HTTP request failed with status: " + std::to_string(r.status_code));
    }
    
    return nlohmann::json::parse(r.text);
}

std::string DpcDownload::buildDownloadUrl(const std::string& version) {
    std::string cleanVersion = sanitizeVersion(version);
    return std::string(GITHUB_DOWNLOAD_BASE) + "/" + cleanVersion + "/" + FIRMWARE_FILENAME;
}

bool DpcDownload::downloadFile(const std::string& url, const std::string& outputPath) {
    std::ofstream file(outputPath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << DpcColors::error("Failed to open output file: " + outputPath) << std::endl;
        return false;
    }
    
    // Download with progress callback
    cpr::Response r = cpr::Get(
        cpr::Url{url},
        cpr::WriteCallback([&file](const std::string_view& data, intptr_t /* userdata */) {
            file.write(data.data(), data.size());
            return true;
        }),
        cpr::ProgressCallback([this](cpr::cpr_off_t downloadTotal, cpr::cpr_off_t downloadNow, 
                                   cpr::cpr_off_t /* uploadTotal */, cpr::cpr_off_t /* uploadNow */, intptr_t /* userdata */) {
            if (downloadTotal > 0) {
                printProgress(downloadNow, downloadTotal);
            }
            return true;
        })
    );
    
    file.close();
    
    if (r.status_code != 200) {
        std::cerr << std::endl << DpcColors::error("HTTP request failed with status: " + std::to_string(r.status_code)) << std::endl;
        if (r.status_code == 404) {
            std::cerr << "The requested firmware version was not found." << std::endl;
        }
        // Clean up partial download
        std::filesystem::remove(outputPath);
        return false;
    }
    
    std::cout << std::endl; // New line after progress
    return true;
}

bool DpcDownload::validateFirmwareFile(const std::string& filePath) {
    if (!fileExists(filePath)) {
        if (m_verbose) {
            std::cerr << "Firmware file not found: " << filePath << std::endl;
        }
        return false;
    }
    
    // Check file size - firmware should be reasonably large (> 1KB)
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        if (m_verbose) {
            std::cerr << "Cannot open firmware file: " << filePath << std::endl;
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

bool DpcDownload::backupExistingFile(const std::string& filePath) {
    if (!fileExists(filePath)) {
        return true; // Nothing to backup
    }
    
    std::string backupPath = filePath + ".backup." + getCurrentTimestamp();
    
    try {
        std::filesystem::copy_file(filePath, backupPath);
        if (m_verbose) {
            std::cout << "Backed up existing file to: " << backupPath << std::endl;
        }
        return true;
    } catch (const std::exception& e) {
        if (m_verbose) {
            std::cerr << "Failed to backup file: " << e.what() << std::endl;
        }
        return false;
    }
}

bool DpcDownload::promptOverwriteExisting(const std::string& filePath) {
    std::cout << "Existing firmware found: " << filePath << std::endl;
    std::cout << "Download new version? (Y/n): ";
    
    std::string input;
    std::getline(std::cin, input);
    
    // Convert to lowercase
    std::transform(input.begin(), input.end(), input.begin(), ::tolower);
    
    // Default to yes if empty input
    return input.empty() || input == "y" || input == "yes";
}

std::string DpcDownload::getDefaultOutputPath() {
    // Try to use the same path logic as DpcFirmware
    // For now, just use the current directory
    return DEFAULT_OUTPUT_PATH;
}

bool DpcDownload::fileExists(const std::string& path) {
    std::ifstream file(path);
    return file.good();
}

std::string DpcDownload::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    return ss.str();
}

void DpcDownload::printProgress(size_t downloaded, size_t total) {
    if (total == 0) return;
    
    int progress = (downloaded * 100) / total;
    int barWidth = 50;
    int pos = (downloaded * barWidth) / total;
    
    std::cout << "\r[";
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }
    std::cout << "] " << progress << "% (" << downloaded << "/" << total << " bytes)" << std::flush;
}

bool DpcDownload::isValidVersion(const std::string& version) {
    if (version.empty()) return false;
    if (version == "latest") return true;
    
    // Basic version validation - should start with 'v' and contain dots
    return version.find('v') == 0 && version.find('.') != std::string::npos;
}

std::string DpcDownload::sanitizeVersion(const std::string& version) {
    std::string clean = version;
    
    // Ensure version starts with 'v'
    if (!clean.empty() && clean[0] != 'v') {
        clean = "v" + clean;
    }
    
    return clean;
} 