// diyPresso Client Serial - Platform support: macOS 13+ and Windows 10/11 only
#include "DpcSerial.h"
#include <iostream>
#include <memory>
#include <libusbp-1/libusbp.hpp>

#ifdef _WIN32
    #include <windows.h>
    #include <synchapi.h>  // For Sleep()
#else
    #include <fcntl.h>
    #include <unistd.h>
    #include <termios.h>
    #include <cstring>
#endif
#include <thread>

DpcSerial::DpcSerial() : is_open_(false), verbose_(false) {
#ifdef _WIN32
    handle_ = INVALID_HANDLE_VALUE;
#else
    fd_ = -1;
#endif
}

DpcSerial::~DpcSerial() {
    close();
}

std::string DpcSerial::find_controller(bool& bootloader_mode) {
    bootloader_mode = false;
    
    try {
        // Get list of all connected USB devices
        std::vector<libusbp::device> devices = libusbp::list_connected_devices();
        
        // Look for Arduino MKR WiFi 1010
        for (const auto& device : devices) {
            uint16_t vendor_id = device.get_vendor_id();
            uint16_t product_id = device.get_product_id();
            
            // Check if this is our Arduino MKR WiFi 1010
            if (vendor_id == ARDUINO_VENDOR_ID && 
                (product_id == ARDUINO_MKR_WIFI_1010_PRODUCT_ID || 
                 product_id == ARDUINO_MKR_WIFI_1010_PRODUCT_ID_BOOTLOADER)) {
                
                bootloader_mode = (product_id == ARDUINO_MKR_WIFI_1010_PRODUCT_ID_BOOTLOADER);
                
                // Create serial port object and get its name
                try {
                    libusbp::serial_port serial_port(device);
                    std::string port_name = serial_port.get_name();
                    
                    if (!port_name.empty()) {
                        std::cout << "Found Arduino MKR WiFi 1010 on port: " << port_name 
                                  << " (bootloader: " << (bootloader_mode ? "yes" : "no") << ")" << std::endl;
                        return port_name;
                    }
                } catch (const libusbp::error& e) {
                    // Device might not have a serial port, continue to next device
                    continue;
                }
            }
        }
    } catch (const libusbp::error& e) {
        std::cerr << "USB enumeration error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    
    return "";
}

bool DpcSerial::open(const std::string& port, unsigned int baudrate) {
    // Close any existing connection first
    close();
    
#ifdef _WIN32
    // Windows implementation
    std::string full_port = "\\\\.\\";
    full_port += port;
    
    handle_ = CreateFileA(
        full_port.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );
    
    if (handle_ == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    // Configure the serial port
    DCB dcb = {};
    dcb.DCBlength = sizeof(dcb);
    
    if (!GetCommState(handle_, &dcb)) {
        close();
        return false;
    }
    
    dcb.BaudRate = baudrate;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fParity = FALSE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_DISABLE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fTXContinueOnXoff = FALSE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fErrorChar = FALSE;
    dcb.fNull = FALSE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;
    dcb.fAbortOnError = FALSE;
    
    if (!SetCommState(handle_, &dcb)) {
        close();
        return false;
    }
    
    // Set timeouts
    COMMTIMEOUTS timeouts = {};
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 1000;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    
    if (!SetCommTimeouts(handle_, &timeouts)) {
        close();
        return false;
    }
    
#else
    // macOS implementation
    fd_ = ::open(port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ == -1) {
        return false;
    }
    
    // Save original settings
    if (tcgetattr(fd_, &original_termios_) != 0) {
        close();
        return false;
    }
    
    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    
    if (tcgetattr(fd_, &tty) != 0) {
        close();
        return false;
    }
    
    // Set baud rate
    speed_t speed;
    switch (baudrate) {
        case 1200:   speed = B1200; break;
        case 9600:   speed = B9600; break;
        case 19200:  speed = B19200; break;
        case 38400:  speed = B38400; break;
        case 57600:  speed = B57600; break;
        case 115200: speed = B115200; break;
        default:     speed = B115200; break;
    }
    
    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);
    
    // Configure for raw mode
    tty.c_cflag |= (CLOCAL | CREAD);    // Enable receiver and set local mode
    tty.c_cflag &= ~PARENB;             // No parity
    tty.c_cflag &= ~CSTOPB;             // 1 stop bit
    tty.c_cflag &= ~CSIZE;              // Clear size bits
    tty.c_cflag |= CS8;                 // 8 data bits
    tty.c_cflag &= ~CRTSCTS;            // No hardware flow control
    
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // Raw input
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);         // No software flow control
    tty.c_iflag &= ~(ICRNL | INLCR);               // No CR/LF conversion
    
    tty.c_oflag &= ~OPOST;              // Raw output
    
    // Set read timeouts (non-blocking)
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;
    
    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        close();
        return false;
    }
    
    // Clear any existing data
    tcflush(fd_, TCIOFLUSH);
#endif
    
    is_open_ = true;
    return true;
}

bool DpcSerial::is_open() const {
    return is_open_;
}

std::string DpcSerial::readline() {
    if (!is_open_) return "";
    
    std::string line;
    char c;
    
#ifdef _WIN32
    DWORD bytes_read;
    
    while (true) {
        if (!ReadFile(handle_, &c, 1, &bytes_read, nullptr) || bytes_read == 0) {
            // Wait a bit for data
            Sleep(1);
            continue;
        }
        
        line += c;
        if (c == '\n') {
            break;
        }
    }
#else
    while (true) {
        ssize_t result = ::read(fd_, &c, 1);
        if (result > 0) {
            line += c;
            if (c == '\n') {
                break;
            }
        } else if (result == 0) {
            // No data available, wait a bit
            usleep(1000); // 1ms
        } else {
            // Error
            break;
        }
    }
#endif
    
    if (verbose_ && !line.empty()) {
        std::string display_line = line;
        // Remove newline for display
        if (!display_line.empty() && display_line.back() == '\n') {
            display_line.pop_back();
        }
        if (!display_line.empty() && display_line.back() == '\r') {
            display_line.pop_back();
        }
        std::cout << "[RECV] " << display_line << std::endl;
    }
    
    return line;
}

void DpcSerial::write(const std::string& data) {
    if (!is_open_) return;
    
    if (verbose_) {
        std::string display_data = data;
        // Remove newline for display
        if (!display_data.empty() && display_data.back() == '\n') {
            display_data.pop_back();
        }
        if (!display_data.empty() && display_data.back() == '\r') {
            display_data.pop_back();
        }
        std::cout << "[SEND] " << display_data << std::endl;
    }
    
#ifdef _WIN32
    DWORD bytes_written;
    if (!WriteFile(handle_, data.c_str(), static_cast<DWORD>(data.length()), &bytes_written, nullptr)) {
        if (verbose_) {
            std::cerr << "Write failed: " << GetLastError() << std::endl;
        }
    }
#else
    ::write(fd_, data.c_str(), data.length());
#endif
}

void DpcSerial::close() {
    if (!is_open_) return;
    
#ifdef _WIN32
    if (handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
#else
    if (fd_ != -1) {
        // Flush all pending data to ensure clean close
        tcflush(fd_, TCIOFLUSH);
        
        // Restore original settings
        tcsetattr(fd_, TCSANOW, &original_termios_);
        
        // Close the file descriptor
        ::close(fd_);
        fd_ = -1;
    }
#endif
    
    is_open_ = false;
}

void DpcSerial::set_verbose(bool verbose) {
    verbose_ = verbose;
}

bool DpcSerial::is_verbose() const {
    return verbose_;
}

// Static utility methods for simple operations
std::unique_ptr<DpcSerial> DpcSerial::create_and_connect(unsigned int baudrate) {
    bool bootloader_mode;
    std::string port = find_controller(bootloader_mode);
    
    if (port.empty()) {
        return nullptr;
    }
    
    auto serial = std::make_unique<DpcSerial>();
    if (!serial->open(port, baudrate)) {
        return nullptr;
    }
    
    return serial;
}

bool DpcSerial::simple_monitor(bool verbose) {
    std::cout << "Searching for diyPresso device..." << std::endl;
    
    auto serial = create_and_connect();
    if (!serial) {
        std::cerr << "Failed to find or connect to diyPresso device" << std::endl;
        return false;
    }
    
    serial->set_verbose(verbose);
    std::cout << "Connected to diyPresso" << std::endl;
    std::cout << "Monitoring serial output. Press Ctrl+C to exit." << std::endl;
    std::cout << std::endl;
    
    while (true) {
        if (serial->is_open()) {
            std::string line = serial->readline();
            if (!line.empty()) {
                // Print without extra newline since readline() includes it
                std::cout << line << std::flush;
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    
    return true;
}

bool DpcSerial::reset_to_bootloader(const std::string& port, bool verbose) {
#ifdef _WIN32
    // Windows implementation - proper 1200 baud reset
    if (verbose) {
        std::cout << "Opening port for bootloader reset: " << port << std::endl;
    }

    // Format port name for Windows
    std::string full_port = "\\\\.\\";
    full_port += port;
    
    // Open the serial port
    HANDLE hSerial = CreateFileA(
        full_port.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );
    
    if (hSerial == INVALID_HANDLE_VALUE) {
        if (verbose) {
            std::cerr << "Failed to open port for reset: " << port << std::endl;
        }
        return false;
    }
    
    // Configure for 1200 baud
    DCB dcb = {};
    dcb.DCBlength = sizeof(dcb);
    
    if (!GetCommState(hSerial, &dcb)) {
        if (verbose) {
            std::cerr << "Failed to get port state" << std::endl;
        }
        CloseHandle(hSerial);
        return false;
    }
    
    dcb.BaudRate = 1200;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    
    if (!SetCommState(hSerial, &dcb)) {
        if (verbose) {
            std::cerr << "Failed to set port to 1200 baud" << std::endl;
        }
        CloseHandle(hSerial);
        return false;
    }
    
    if (verbose) {
        std::cout << "Port configured to 1200 baud, closing to trigger reset..." << std::endl;
    }
    
    // Close the port — this triggers the bootloader
    CloseHandle(hSerial);
    
    if (verbose) {
        std::cout << "Done. Board should now be in bootloader mode." << std::endl;
    }
    
    return true;
#else
    // macOS implementation - based on working bootloader_reset_test.cpp
    if (verbose) {
        std::cout << "Opening port for bootloader reset: " << port << std::endl;
    }

    // Open the serial port
    int fd = ::open(port.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1) {
        if (verbose) {
            perror("Failed to open port");
        }
        return false;
    }

    // Get current port settings
    struct termios options;
    if (tcgetattr(fd, &options) < 0) {
        if (verbose) {
            perror("Failed to get port attributes");
        }
        ::close(fd);
        return false;
    }

    // Set baud rate to 1200
    if (cfsetispeed(&options, B1200) < 0 || cfsetospeed(&options, B1200) < 0) {
        if (verbose) {
            perror("Failed to set baud rate");
        }
        ::close(fd);
        return false;
    }

    // Apply the settings immediately
    if (tcsetattr(fd, TCSANOW, &options) < 0) {
        if (verbose) {
            perror("Failed to set port attributes");
        }
        ::close(fd);
        return false;
    }

    if (verbose) {
        std::cout << "Port configured to 1200 baud, closing to trigger reset..." << std::endl;
    }

    // Close the port — this triggers the bootloader
    ::close(fd);

    if (verbose) {
        std::cout << "Done. Board should now be in bootloader mode." << std::endl;
    }
    
    return true;
#endif
}