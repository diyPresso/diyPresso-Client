#include <iostream>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

int main() {
    const char* portname = "/dev/cu.usbmodem111301";  // replace with your port

    std::cout << "Opening port: " << portname << std::endl;

    // Open the serial port
    int fd = open(portname, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1) {
        perror("Failed to open port");
        return 1;
    }

    // Get current port settings
    struct termios options;
    if (tcgetattr(fd, &options) < 0) {
        perror("Failed to get port attributes");
        close(fd);
        return 1;
    }

    // Set baud rate to 1200
    if (cfsetispeed(&options, B1200) < 0 || cfsetospeed(&options, B1200) < 0) {
        perror("Failed to set baud rate");
        close(fd);
        return 1;
    }

    // Apply the settings immediately
    if (tcsetattr(fd, TCSANOW, &options) < 0) {
        perror("Failed to set port attributes");
        close(fd);
        return 1;
    }

    std::cout << "Port configured to 1200 baud, closing to trigger reset..." << std::endl;

    // Close the port — this triggers the bootloader
    close(fd);

    std::cout << "Done. Board should now be in bootloader mode." << std::endl;
    return 0;
}
