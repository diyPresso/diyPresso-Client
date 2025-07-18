#pragma once
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

class DpcColors {
public:
    // ANSI Color codes
    static const std::string RESET;
    static const std::string RED;
    static const std::string GREEN;
    static const std::string YELLOW;
    static const std::string BLUE;
    static const std::string MAGENTA;
    static const std::string CYAN;
    static const std::string WHITE;
    static const std::string BOLD;

    // Check if terminal supports colors
    static bool is_color_supported() {
        static bool checked = false;
        static bool supported = false;
        
        if (!checked) {
            checked = true;
            
            // Check if stdout is a terminal
            if (!isatty(fileno(stdout))) {
                supported = false;
                return supported;
            }
            
#ifdef _WIN32
            // Try to enable ANSI support on Windows 10+
            HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
            DWORD dwMode = 0;
            if (GetConsoleMode(hOut, &dwMode)) {
                dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                supported = SetConsoleMode(hOut, dwMode);
            }
#else
            // On macOS/Linux, assume ANSI support
            supported = true;
#endif
        }
        
        return supported;
    }

    // Helper functions for colored output
    static std::string ok(const std::string& text) {
        return is_color_supported() ? GREEN + "OK: " + RESET + text : "OK: " + text;
    }
    
    static std::string error(const std::string& text) {
        return is_color_supported() ? RED + "ERROR: " + RESET + text : "ERROR: " + text;
    }
    
    static std::string warning(const std::string& text) {
        return is_color_supported() ? YELLOW + "WARNING: " + RESET + text : "WARNING: " + text;
    }
    
    static std::string step(const std::string& text) {
        return is_color_supported() ? CYAN + text + RESET : text;
    }
    
    static std::string highlight(const std::string& text) {
        return is_color_supported() ? BOLD + text + RESET : text;
    }
}; 