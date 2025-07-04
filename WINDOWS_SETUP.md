# Windows Setup Guide - diyPresso Client C++

This guide explains how to build and run the diyPresso Client on Windows 10/11.

## Prerequisites

### 1. Install Visual Studio 2022
- Download [Visual Studio 2022 Community](https://visualstudio.microsoft.com/downloads/) (free)
- During installation, select:
  - **Desktop development with C++** workload
  - **Windows 10/11 SDK** (latest version)
  - **CMake tools for Visual Studio**

### 2. Install Git
- Download from [git-scm.com](https://git-scm.com/download/win)
- Use default settings during installation

### 3. Install vcpkg (Package Manager)
```cmd
# Clone vcpkg to C:\vcpkg (recommended location)
cd C:\
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg

# Bootstrap vcpkg
.\bootstrap-vcpkg.bat

# Integrate with Visual Studio (optional but recommended)
.\vcpkg integrate install
```

### 4. Set Environment Variable
- Add `VCPKG_ROOT` environment variable:
  - Press `Win + R`, type `sysdm.cpl`
  - Go to **Advanced** → **Environment Variables**
  - Add new **System Variable**:
    - Name: `VCPKG_ROOT`
    - Value: `C:\vcpkg`

## Building the Project

### Method 1: Automated Build Script
1. Clone the repository:
   ```cmd
   git clone <repository-url>
   cd diyPresso-Client-cpp
   ```

2. Run the Windows build script:
   ```cmd
   build-windows.bat
   ```

### Method 2: Manual Build
1. Install dependencies:
   ```cmd
   REM boost-asio no longer needed - using native Windows API
   %VCPKG_ROOT%\vcpkg install nlohmann-json:x64-windows
   %VCPKG_ROOT%\vcpkg install cli11:x64-windows
   %VCPKG_ROOT%\vcpkg install libusbp:x64-windows
   ```

2. Configure and build:
   ```cmd
   mkdir build
   cd build
   cmake .. -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
   cmake --build . --config Release
   ```

## Running the Application

### USB Driver Setup
1. Connect your Arduino MKR WiFi 1010
2. Install Arduino IDE or just the drivers from [Arduino's website](https://www.arduino.cc/en/software)
3. Verify the device appears in Device Manager under "Ports (COM & LPT)"

## Development with VS Code

### 1. Install VS Code Extensions
- **C/C++** (Microsoft)
- **CMake Tools** (Microsoft)
- **C/C++ Extension Pack** (Microsoft)

### 2. Open Project
1. Open VS Code
2. Open folder: `diyPresso-Client-cpp`
3. Select **Windows** configuration when prompted by C/C++ extension
4. Use `Ctrl+Shift+P` → **CMake: Configure** to configure the project

### 3. Build and Debug
- Press `F5` to build and debug
- Or use `Ctrl+Shift+P` → **CMake: Build**

## Troubleshooting

### Common Issues

#### CMake Cannot Find vcpkg
- Ensure `VCPKG_ROOT` environment variable is set correctly
- Restart command prompt/VS Code after setting environment variables

#### libusbp Not Found
```cmd
# Reinstall libusbp
%VCPKG_ROOT%\vcpkg remove libusbp:x64-windows
%VCPKG_ROOT%\vcpkg install libusbp:x64-windows
```

#### Device Not Detected
- Check Device Manager for COM port
- Try different USB ports
- Restart the Arduino MKR WiFi 1010
- Run application as Administrator if needed

#### Build Errors with MSVC
- Ensure Visual Studio 2022 is properly installed
- Try cleaning and rebuilding:
  ```cmd
  cmake --build . --target clean
  cmake --build . --config Release
  ```

### USB Permissions
Windows typically doesn't require special USB permissions for COM ports, but if you encounter issues:
- Run Command Prompt as Administrator
- Check that the Arduino drivers are properly installed

## File Locations

- **Executable**: `build\Release\diypresso.exe`
- **Settings files**: Same directory as executable
- **Logs**: Console output only (no log files by default)

## Supported Windows Versions

- Windows 10 (version 1909 or later)
- Windows 11 (all versions)
- Windows Server 2019/2022

Both x64 and x86 architectures are supported, though x64 is recommended. 