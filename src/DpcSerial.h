// diyPresso Client Serial - Platform support: macOS 13+ and Windows 10/11 only
#pragma once
#include <string>
#include <memory>
#include <libusbp-1/libusbp.hpp>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <termios.h>
#endif

class DpcSerial {
public:
    // Constructor and destructor
    DpcSerial();
    ~DpcSerial();
    
    // Delete copy constructor and assignment operator
    DpcSerial(const DpcSerial&) = delete;
    DpcSerial& operator=(const DpcSerial&) = delete;

    // Static methods
    static std::string find_controller(bool& bootloader_mode);
    
    // Static utility methods for simple operations
    static std::unique_ptr<DpcSerial> create_and_connect(unsigned int baudrate = 115200);
    static bool simple_monitor(bool verbose = false);
    static bool reset_to_bootloader(const std::string& port, bool verbose = false);

    // Instance methods
    bool open(const std::string& port, unsigned int baudrate = 115200);
    bool is_open() const;
    std::string readline();
    void write(const std::string& data);
    void close();
    
    // Verbose mode
    void set_verbose(bool verbose);
    bool is_verbose() const;

    // Arduino MKR WiFi 1010 IDs
    static constexpr int ARDUINO_VENDOR_ID = 9025;
    static constexpr int ARDUINO_MKR_WIFI_1010_PRODUCT_ID = 32852;
    static constexpr int ARDUINO_MKR_WIFI_1010_PRODUCT_ID_BOOTLOADER = 84;

private:
#ifdef _WIN32
    HANDLE handle_;
#else
    int fd_;
    struct termios original_termios_;
#endif
    bool is_open_;
    bool verbose_;
}; 