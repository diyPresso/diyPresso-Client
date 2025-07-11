#include <iostream>
#include <fstream>
#include <cstring>

bool filesAreIdentical(const std::string& file1, const std::string& file2) {
    std::ifstream f1(file1, std::ios::binary | std::ios::ate);
    std::ifstream f2(file2, std::ios::binary | std::ios::ate);
    
    if (!f1.is_open() || !f2.is_open()) {
        std::cout << "Failed to open files" << std::endl;
        return false;
    }
    
    auto size1 = f1.tellg();
    auto size2 = f2.tellg();
    
    std::cout << "File1 size: " << size1 << ", File2 size: " << size2 << std::endl;
    
    if (size1 != size2) {
        std::cout << "Sizes don't match" << std::endl;
        return false;
    }
    
    // If sizes match, compare content
    f1.seekg(0);
    f2.seekg(0);
    
    const size_t bufferSize = 4096;
    char buffer1[bufferSize];
    char buffer2[bufferSize];
    
    int chunk = 0;
    while (f1.read(buffer1, bufferSize) && f2.read(buffer2, bufferSize)) {
        std::cout << "Comparing chunk " << chunk << ", read " << f1.gcount() << " bytes" << std::endl;
        if (f1.gcount() != f2.gcount() || std::memcmp(buffer1, buffer2, f1.gcount()) != 0) {
            std::cout << "Content differs in chunk " << chunk << std::endl;
            return false;
        }
        chunk++;
    }
    
    // Check if both files reached EOF at the same time
    bool result = f1.eof() && f2.eof() && f1.gcount() == f2.gcount();
    std::cout << "Final result: " << result << std::endl;
    std::cout << "f1.eof(): " << f1.eof() << ", f2.eof(): " << f2.eof() << std::endl;
    std::cout << "f1.gcount(): " << f1.gcount() << ", f2.gcount(): " << f2.gcount() << std::endl;
    
    return result;
}

int main() {
    std::string file1 = "../firmware.bin";
    std::string file2 = "../firmware.bin.backup.20250705_214809";
    
    std::cout << "Comparing " << file1 << " and " << file2 << std::endl;
    bool identical = filesAreIdentical(file1, file2);
    std::cout << "Files are identical: " << identical << std::endl;
    
    return 0;
} 