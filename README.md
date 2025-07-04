# diyPresso Client C++

C++ management client for diyPresso espresso machine. This application provides firmware upload capabilities, settings configuration and serial monitoring for Arduino MKR WiFi 1010 in the diyPresso machine.


#
## 🖥️ Windows User Guide - Firmware Update

**How to update the firmware of your diyPresso machine on Windows:**

1. **Download the update**
   - Go to the release page of this github repository.
   - Click **diyPresso-Client-Windows.zip** to download it.  
   _(screenshot: download link)_

2. **Extract the files**
   - Open your **Downloads** folder.
   - Right-click `diyPresso-Client-Windows.zip` → choose **Extract All...** and click **Extract**.  
   _(screenshot: extract dialog)_

3. **Open the extracted folder**
   - Double-click the new `diyPresso-Client-Windows` folder to open it.  
   _(screenshot: folder location)_

4. **Open Command Prompt in this folder**
   - Click the folder's address bar (top of the window).
   - Type `cmd` and press **Enter**.  
   _(screenshot: address bar with cmd)_

5. **Turn of your diyPresso machine**
   - Make sure your diyPresso machine is **fully turned off**.
   - Do **not** connect the USB cable yet.

6. **Start the Firmware Upload**
   - In the Command Prompt window, type the command below and press **Enter**:
     ```
     diypresso upload-firmware
     ```
   _(screenshot: command prompt)_

7. **Connect the USB Cable**
   - Plug the USB cable into your diyPresso machine and your computer.
   - The update will start automatically. Watch the messages in the Command Prompt.

9. **Final Steps**
   - Unplug the USB cable.
   - Turn on your diyPresso nachine and enjoy your upgraded coffee!

> **Tip:** If you see "Device not found," make sure the diyPresso is fully powered off before connecting the USB cable.


# 🎪 Other Usage Examples

```bash
# Device information
./diypresso info

# Monitor raw serial output
./diypresso monitor

# Settings management
./diypresso get-settings
./diypresso restore-settings --settings-file backup.json

# upload firmare
./diypresso upload-firmware --binary-file firmware.bin
```


### Platform Support
- **macOS 13+** (Apple Silicon and Intel)
- **Windows 10/11** (x64 and x86)


## 🏛️ Architecture Overview

The application follows a **device-centric architecture** with clear ownership and service patterns:

```
┌─────────────────────────────────────────────────────────────┐
│                    CLI Interface Layer                      │
│                      (main.cpp)                            │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌────────┐ │
│  │   monitor   │ │get-settings │ │upload-firmware│ │ info │ │
│  │    help     │ │restore-sett │ │             │ │      │ │
│  └─────────────┘ └─────────────┘ └─────────────┘ └────────┘ │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                  Device-Centric Core                       │
│  ┌─────────────────────────────────────────────────────┐    │
│  │                  DpcDevice                         │    │
│  │                (Central Hub)                       │    │
│  │  ┌─────────────────────────────────────────────┐   │    │
│  │  │              DpcSerial                      │   │    │
│  │  │           (Owned Instance)                  │   │    │
│  │  │  • USB detection  • Serial I/O             │   │    │
│  │  │  • libusbp enum   • Native Win/macOS API    │   │    │
│  │  │  • port mgmt      • protocol handling      │   │    │
│  │  └─────────────────────────────────────────────┘   │    │
│  │                                                     │    │
│  │  • Device state & lifecycle                        │    │
│  │  • Connection management                           │    │
│  │  • Command/response protocol                       │    │
│  │  • Bootloader operations                           │    │
│  │  • Device information                              │    │
│  └─────────────────────────────────────────────────────┘    │
│                              │                              │
│                              ▼                              │
│  ┌─────────────────┐  ┌─────────────────┐  ┌──────────────┐ │
│  │  DpcSettings    │  │  DpcFirmware    │  │  (Future)    │ │
│  │   (Service)     │  │   (Service)     │  │  Services    │ │
│  │                 │  │                 │  │              │ │
│  │ • get_settings  │  │ • upload        │  │ • ...        │ │
│  │ • put_settings  │  │ • bossac        │  │              │ │
│  │ • save_to_file  │  │ • validation    │  │              │ │
│  │ • load_from_file│  │ • workflow      │  │              │ │
│  │ • validation    │  │ • safety checks │  │              │ │
│  └─────────────────┘  └─────────────────┘  └──────────────┘ │
│         │                      │                   │        │
│         └──────────────────────┼───────────────────┘        │
│                                ▼                            │
│              Operates on DpcDevice reference                │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                   Hardware Layer                           │
│                  Arduino MKR WiFi 1010                     │
│              (diyPresso espresso controller)               │
│                   VID: 9025, PID: 32852/84                │
└─────────────────────────────────────────────────────────────┘
```

### **Key Architecture Principles:**

- **🎯 Single Device Instance** - One DpcDevice per command execution
- **🏠 Clear Ownership** - DpcDevice owns the DpcSerial connection  
- **⚙️ Service Pattern** - DpcSettings and DpcFirmware operate on device
- **🔄 Lifecycle Management** - Device handles connection state
- **🛡️ Encapsulation** - Serial details hidden behind device interface

## 📁 Project Structure

```
diyPresso-Client-cpp/
├── README.md                 # This file
├── CMakeLists.txt           # Build configuration
├── c_cpp_properties.json    # VS Code C++ configuration
│
├── src/                     # Source code
│   ├── main.cpp             # ✅ CLI interface & command routing
│   ├── DpcSerial.h/.cpp     # ✅ Serial communication layer
│   ├── DpcDevice.h/.cpp     # ✅ Device state & operations
│   ├── DpcSettings.h/.cpp   # 🔲 Settings management
│   └── DpcFirmware.h/.cpp   # 🔲 Firmware upload & bootloader
│
├── bin/                     # Binaries and tools
│   ├── firmware/            # Firmware binary files
│   └── bossac/              # bossac tool for firmware upload
│
├── python-src/              # Reference Python implementation
│   ├── diyPresso/           # Python modules
│   └── diyPresso-cli/       # Python CLI application
│
├── build/                   # Build artifacts (generated)
└── external/                # External dependencies
```

Legend: ✅ Implemented | 🔲 Planned

## 🔧 Class Responsibilities

### **DpcSerial** - Serial Communication Layer
**Status:** ✅ Implemented

Handles low-level USB and serial communication:
- USB device enumeration using libusbp
- Serial port management with native platform APIs (Windows/macOS)
- Device detection (Arduino MKR WiFi 1010)
- Raw read/write operations


### **DpcDevice** - Device State & Operations
**Status:** ✅ Implemented

Manages device connection and basic operations:
- Device discovery and connection management
- Firmware version detection
- Serial monitoring (raw output)
- Command/response protocol handling


### **DpcSettings** - Settings Management
**Status:** 🔲 Planned

Handles all settings-related operations:
- GET/PUT settings protocol implementation
- JSON file serialization/deserialization
- Settings validation, backup and restore

### **DpcFirmware** - Firmware Upload & Bootloader
**Status:** 🔲 Planned

Manages firmware upload and bootloader operations:
- Bootloader reset (1200 baud trick)
- bossac integration for firmware upload
- Complete update workflow with settings backup/restore
- Firmware validation




## 🚀 Building

### Windows
See [WINDOWS_SETUP.md](WINDOWS_SETUP.md) for detailed Windows build instructions.

**Quick start:**
```cmd
# Run the automated build script
build-windows.bat
```

### macOS/Linux
1. **Install dependencies via vcpkg:**
   ```bash
   vcpkg install libusbp nlohmann-json cli11
   ```

2. **Configure and build:**
   ```bash
   mkdir build && cd build
   cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
   make
   ```

3. **Run:**
   ```bash
   ./diypresso info
   ```

## 📋 Development Status

- [x] **Serial Communication** - USB enumeration and serial I/O
- [x] **Basic CLI Interface** - Command parsing and device detection
- [x] **Device Discovery** - Arduino MKR WiFi 1010 detection
- [x] **Device Management** - DpcDevice class with connection management
- [x] **Monitor Mode** - Raw serial output monitoring
- [x] **Bootloader Reset** - 1200 baud trick implementation
- [ ] **Settings Management** - GET/PUT settings with JSON I/O
- [ ] **Firmware Upload** - bossac integration and bootloader reset
- [ ] **Complete CLI** - All commands from Python version
- [ ] **Windows Support** - Cross-platform compatibility
- [ ] **Error Handling** - Comprehensive error management
- [ ] **Documentation** - API documentation and examples

## 🚧 TODO

- [ ] **Multi-platform macOS builds** - Create universal binaries (ARM + Intel) for macOS
- [ ] **Add macOS code signing** - Implement code signing step in build script with developer ID and entitlements
- [ ] **Remove std::exit() usage** - Replace with proper error handling and return codes throughout codebase
- [ ] **Refactor global state** - Move g_device, g_interrupted, g_verbose into Application/context class for better testability
- [ ] **Extract command logic** - Move CLI command implementations from main.cpp into separate command classes/functions
- [ ] **Standardize error handling** - Use consistent exceptions or error codes across all modules (DpcSettings, DpcFirmware, DpcDevice, main.cpp)
- [ ] **Improve settings validation** - Add required key validation in DpcSettings beyond just count checking
- [ ] **Extract path logic** - Move firmware and bossac path logic in DpcFirmware to reusable utility function/class
- [ ] **Add unit tests** - Create comprehensive tests for DpcSerial, DpcDevice, DpcSettings, and DpcFirmware
- [ ] **Document build process** - Add macOS build and packaging documentation to README


## 📄 License

GNU General Public License v3.0 - see the [LICENSE](LICENSE) file for details.

---

*diyPresso Client C++ - Modern device management for espresso enthusiasts* ☕ 