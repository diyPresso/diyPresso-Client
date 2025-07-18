# diyPresso Qt GUI Implementation Plan

## 🎯 Project Overview

Create a simple, user-friendly Qt GUI application for diyPresso espresso machine management, targeting coffee enthusiasts and casual users who prefer visual interfaces over command-line tools.

## 📋 Architecture Decisions

### **Dual-App Strategy (Separate Applications)**
- **`diypresso`** - CLI application (existing, for developers/power users)
- **`diypresso-gui`** - Qt GUI application (new, for casual users)

**Rationale:**
- ✅ Clean separation of concerns
- ✅ Smaller CLI binary (no Qt dependencies)
- ✅ Professional approach (git vs GitHub Desktop)
- ✅ Flexible distribution options

### **Shared Backend Architecture**
```
diyPresso-Client-cpp/
├── src/
│   ├── core/                # Shared backend classes
│   │   ├── DpcDevice.h/.cpp
│   │   ├── DpcFirmware.h/.cpp
│   │   ├── DpcDownload.h/.cpp
│   │   ├── DpcSettings.h/.cpp
│   │   ├── DpcSerial.h/.cpp
│   │   └── DpcColors.h/.cpp
│   ├── cli/                 # Command line interface
│   │   └── main.cpp
│   └── gui/                 # Qt GUI application
│       ├── main.cpp
│       ├── MainWindow.h/.cpp
│       ├── MainWindow.ui
│       ├── FirmwareDialog.h/.cpp
│       ├── FirmwareDialog.ui
│       └── SettingsDialog.h/.cpp
├── resources/               # Qt resources
│   ├── icons/
│   └── diypresso.qrc
└── bin/                     # Tools and output
```

## 🛠️ Technology Stack

### **Framework: Qt6**
**Rationale:**
- ✅ Native C++ integration with existing DPC classes
- ✅ Cross-platform (macOS, Windows, Linux)
- ✅ Professional native look and feel
- ✅ Excellent for device management applications
- ✅ Strong threading support for long-running operations

### **Dependencies:**
- **Qt 6.8 LTS** - Core widgets and functionality (LTS support until 2027+)
- **Qt6 Tools** - Development tools (Designer, etc.)
- **Existing dependencies** - libusbp, nlohmann-json, cli11, cpr

### **Build System Integration:**
```cmake
# CMakeLists.txt targets:
- diyPresso-core    # Shared library
- diypresso         # CLI executable  
- diypresso-gui     # GUI executable (Qt 6.8 LTS)

# Qt 6.8 LTS requirement
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Widgets)
```

## 🎨 User Interface Design

### **Main Window Layout:**
```
┌─────────────────────────────────────────────────┐
│ diyPresso Manager v1.2.0                    [X] │
├─────────────────────────────────────────────────┤
│ Device Status:  [🔴 Not Connected]              │
│                 [🔍 Search for Device]          │
├─────────────────────────────────────────────────┤
│ ┌─ Firmware ─────────────────────────────────┐  │
│ │ Current: v1.6.2    Latest: v1.7.0-rc1     │  │
│ │ [📥 Update Firmware]  [ℹ️ Check Versions]   │  │
│ └────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────┤
│ ┌─ Settings ─────────────────────────────────┐  │
│ │ [📋 Backup Settings]  [📁 Restore Settings] │  │
│ └────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────┤
│ ┌─ Advanced ─────────────────────────────────┐  │
│ │ [📟 Serial Monitor]   [⚙️ Device Info]      │  │
│ └────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────┤
│ Status: Ready                                   │
└─────────────────────────────────────────────────┘
```

### **Key UI Components:**
- **Device Status Indicator** - Visual connection state
- **One-Click Firmware Update** - Progress bar with status
- **Settings Management** - File dialogs for backup/restore
- **Version Information** - Current vs latest firmware
- **Serial Monitor** - Real-time device communication display
- **Log Output** - User feedback and status messages

## 🚀 Implementation Phases

### **Phase 1: Foundation & Setup**
**Branch:** `feature/qt-gui`

**Tasks:**
- [ ] Refactor project structure (move existing files to `src/core/`)
- [ ] Add Qt 6.8 LTS to vcpkg dependencies
- [ ] Update CMakeLists.txt for multiple targets with Qt 6.8 requirement
- [ ] Create basic MainWindow with static UI
- [ ] Test build on Windows and macOS
- [ ] Verify both CLI and GUI can be built

**Deliverable:** Basic window that opens and displays static UI

### **Phase 2: Device Integration**
**Tasks:**
- [ ] Integrate DpcDevice for connection status
- [ ] Display device information (version, port, status)
- [ ] Implement device search functionality
- [ ] Add connection status timer (auto-refresh)
- [ ] Basic error handling with QMessageBox

**Deliverable:** GUI can detect and display device status

### **Phase 3: Firmware Management**
**Tasks:**
- [ ] Implement firmware update with progress bar
- [ ] Integrate DpcDownload for version checking
- [ ] Create firmware version dialog
- [ ] Add download progress indication
- [ ] Handle firmware update workflow (7 steps)

**Deliverable:** Complete firmware update functionality

### **Phase 4: Core Features & Polish**
**Tasks:**
- [ ] Settings backup with QFileDialog
- [ ] Settings restore functionality
- [ ] Serial monitor window with real-time updates
- [ ] Device info dialog
- [ ] Application icon and about dialog
- [ ] Error handling improvements
- [ ] UI polish and user experience refinements

**Deliverable:** Feature-complete MVP GUI application

## 📦 Distribution Strategy

### **Windows Distribution**

#### **Primary Distribution: NSIS Installer**
- **File:** `diyPresso-GUI-Setup-v1.2.0.exe`
- **Features:**
  - ✅ Professional installer experience
  - ✅ Start Menu shortcuts
  - ✅ Desktop shortcut
  - ✅ Automatic Qt DLL deployment
  - ✅ Clean uninstaller
  - ✅ File associations (optional)

#### **Alternative: Portable ZIP**
- **File:** `diyPresso-GUI-Portable-Windows.zip`
- **Use case:** Users who prefer portable applications
- **Contents:** All files in folder, no installation required

#### **CLI Distribution (Unchanged):**
- **File:** `diyPresso-CLI-Windows.zip`
- **Target:** Developers, automation, power users

### **macOS Distribution**

#### **Primary Distribution: DMG with App Bundle**
- **File:** `diyPresso-GUI-v1.2.0.dmg`
- **Features:**
  - ✅ Standard macOS user experience
  - ✅ Professional drag-to-Applications installation
  - ✅ Self-contained app bundle with Qt dependencies
  - ✅ Custom DMG background and layout
  - ✅ Clear version identification
  - ✅ Future notarization support (when Apple Developer Program available)

#### **Code Signing Strategy:**
- **Phase 1:** Unsigned DMG (initial release with Gatekeeper workaround documentation)
- **Phase 2:** Notarized DMG (future investment for seamless user experience)

#### **CLI Distribution (Unchanged):**
- **File:** `diyPresso-CLI-macOS.zip`
- **Target:** Developers, automation, power users

#### **Build Integration:**
- Use `macdeployqt` for Qt dependency bundling
- Use `create-dmg` tool for professional DMG creation
- Update `build-macos.sh` with DMG generation steps

## 🎯 Target Users & Use Cases

### **GUI Application Users:**
- ☕ **Coffee Enthusiasts** - Want simple firmware updates
- 🏠 **Home Users** - Prefer visual interfaces
- 🔧 **Occasional Users** - Need guided workflows
- 👥 **Non-Technical Users** - Visual feedback and error handling

### **Primary Use Cases:**
1. **One-Click Firmware Update** - Download latest + upload with progress
2. **Device Status Monitoring** - Visual connection and version display
3. **Settings Backup/Restore** - File-based configuration management
4. **Version Information** - Check current vs available versions
5. **Serial Monitor** - View real-time device communication and debug output

### **MVP Scope (Keep Simple):**
**Include:**
- ✅ Device connection status
- ✅ One-click firmware update with progress
- ✅ Settings backup/restore with file dialogs
- ✅ Version checking and display
- ✅ Serial monitor for device communication

**Exclude (Future versions):**
- ❌ Complex device configuration
- ❌ Advanced serial debugging features
- ❌ Custom firmware building
- ❌ Multiple device support

## 🔄 Threading Strategy

### **Long-Running Operations:**
Firmware downloads and uploads require threading to prevent UI blocking:

```cpp
class FirmwareWorker : public QObject {
    Q_OBJECT
public slots:
    void updateFirmware();
signals:
    void progressUpdated(int percentage, QString status);
    void finished(bool success, QString message);
};
```

### **UI Responsiveness:**
- Use QThread for backend operations
- Emit progress signals for UI updates
- Disable UI elements during operations
- Provide cancel functionality where appropriate

## 📈 Success Metrics

### **User Experience Goals:**
- **< 3 clicks** to update firmware
- **Visual feedback** for all operations
- **Clear error messages** with next steps
- **< 30 seconds** for typical firmware update
- **Real-time serial monitoring** for device troubleshooting

### **Technical Goals:**
- **Cross-platform** Qt builds
- **Professional appearance** matching platform conventions
- **Reliable operation** with proper error handling
- **Maintainable code** sharing backend with CLI

## 🚧 Future Considerations

### **Potential Enhancements:**
- **Auto-update** for GUI application itself
- **Device discovery** scanning multiple ports
- **Advanced settings** editor with validation
- **Firmware history** and rollback capability
- **Multiple language** support

### **Distribution Expansion:**
- **Microsoft Store** publishing
- **macOS App Store** consideration
- **Linux AppImage** for broader reach
- **Chocolatey/Homebrew** package managers

---

**Status:** Planning Phase  
**Next Step:** Begin Phase 1 - Foundation & Setup  
**Target:** Qt GUI v1.2.0 alongside CLI v1.1.0 