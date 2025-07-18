#include "MainWindow.h"
#include <QApplication>
#include <QMessageBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QString>
#include <QThread>
#include <QTimer>
#include <QProgressDialog>
#include <QMetaObject>
#include <memory>
#include "../core/DpcFirmware.h"
#include "../core/DpcDownload.h"
#include "../core/DpcSettings.h" // Added for settings backup/restore

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), device_(std::make_unique<DpcDevice>())
{
    setupUi();
    updateDeviceStatus();
    
    // Set window properties
    setWindowTitle("diyPresso Manager v1.2.0");
    setMinimumSize(600, 500);
    resize(700, 600);
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUi()
{
    // Create central widget
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    
    // diyPresso Status Section
    deviceGroup = new QGroupBox("diyPresso Status", this);
    QHBoxLayout *deviceLayout = new QHBoxLayout(deviceGroup);
    
    deviceStatusLabel = new QLabel("🔴 Not Connected", this);
    deviceStatusLabel->setStyleSheet("font-size: 14px; font-weight: bold;");
    searchDeviceButton = new QPushButton("🔍 Search for diyPresso", this);
    
    deviceLayout->addWidget(deviceStatusLabel);
    deviceLayout->addStretch();
    deviceLayout->addWidget(searchDeviceButton);
    
    // Firmware Section
    firmwareGroup = new QGroupBox("Firmware", this);
    QGridLayout *firmwareLayout = new QGridLayout(firmwareGroup);
    
    currentVersionLabel = new QLabel("Current: v1.6.2", this);
    latestVersionLabel = new QLabel("Latest: v1.7.0-rc1", this);
    updateFirmwareButton = new QPushButton("📥 Update Firmware", this);
    checkVersionsButton = new QPushButton("ℹ️ Check Versions", this);
    firmwareProgressBar = new QProgressBar(this);
    firmwareProgressBar->setVisible(false);
    
    firmwareLayout->addWidget(currentVersionLabel, 0, 0);
    firmwareLayout->addWidget(latestVersionLabel, 0, 1);
    firmwareLayout->addWidget(updateFirmwareButton, 1, 0);
    firmwareLayout->addWidget(checkVersionsButton, 1, 1);
    firmwareLayout->addWidget(firmwareProgressBar, 2, 0, 1, 2);
    
    // Settings Section
    settingsGroup = new QGroupBox("Settings", this);
    QHBoxLayout *settingsLayout = new QHBoxLayout(settingsGroup);
    
    backupSettingsButton = new QPushButton("📋 Backup Settings", this);
    restoreSettingsButton = new QPushButton("📁 Restore Settings", this);
    
    settingsLayout->addWidget(backupSettingsButton);
    settingsLayout->addWidget(restoreSettingsButton);
    
    // Advanced Section
    advancedGroup = new QGroupBox("Advanced", this);
    QHBoxLayout *advancedLayout = new QHBoxLayout(advancedGroup);
    
    serialMonitorButton = new QPushButton("📟 Serial Monitor", this);
    deviceInfoButton = new QPushButton("⚙️ diyPresso Machine Info", this);
    
    advancedLayout->addWidget(serialMonitorButton);
    advancedLayout->addWidget(deviceInfoButton);
    
    // Log Section
    QGroupBox *logGroup = new QGroupBox("Status Log", this);
    QVBoxLayout *logLayout = new QVBoxLayout(logGroup);
    
    logTextEdit = new QTextEdit(this);
    logTextEdit->setMaximumHeight(150);
    logTextEdit->setReadOnly(true);
    logTextEdit->append("diyPresso Manager started successfully.");
    logTextEdit->append("Ready to connect to the diyPresso.");
    
    logLayout->addWidget(logTextEdit);
    
    // Add all sections to main layout
    mainLayout->addWidget(deviceGroup);
    mainLayout->addWidget(firmwareGroup);
    mainLayout->addWidget(settingsGroup);
    mainLayout->addWidget(advancedGroup);
    mainLayout->addWidget(logGroup);
    mainLayout->addStretch();
    
    // Status bar
    statusBar = new QStatusBar(this);
    statusBar->showMessage("Ready");
    setStatusBar(statusBar);
    
    // Connect button signals to slots
    connect(searchDeviceButton, &QPushButton::clicked, this, &MainWindow::searchForDevice);
    connect(updateFirmwareButton, &QPushButton::clicked, this, &MainWindow::updateFirmware);
    connect(checkVersionsButton, &QPushButton::clicked, this, &MainWindow::checkVersions);
    connect(backupSettingsButton, &QPushButton::clicked, this, &MainWindow::backupSettings);
    connect(restoreSettingsButton, &QPushButton::clicked, this, &MainWindow::restoreSettings);
    connect(serialMonitorButton, &QPushButton::clicked, this, &MainWindow::showSerialMonitor);
    connect(deviceInfoButton, &QPushButton::clicked, this, &MainWindow::showDeviceInfo);
    
    // Initially disable buttons that require device connection
    updateFirmwareButton->setEnabled(false);
    checkVersionsButton->setEnabled(false);
    serialMonitorButton->setEnabled(false);
    deviceInfoButton->setEnabled(false);
    backupSettingsButton->setEnabled(false);
    restoreSettingsButton->setEnabled(false);
}

void MainWindow::updateDeviceStatus()
{
    updateConnectionStatus();
}

void MainWindow::updateConnectionStatus()
{
    if (device_->is_connected()) {
        auto deviceInfo = device_->get_device_info();
        
        // Update status label with connection info
        QString statusText = QString("🟢 Connected to %1").arg(QString::fromStdString(deviceInfo.port));
        deviceStatusLabel->setText(statusText);
        
        // Update status bar
        QString statusBarText = QString("Connected: %1 | Firmware: %2")
            .arg(QString::fromStdString(deviceInfo.port))
            .arg(QString::fromStdString(deviceInfo.firmware_version));
        statusBar->showMessage(statusBarText);
        
        // Update firmware version labels
        currentVersionLabel->setText(QString("Current: %1").arg(QString::fromStdString(deviceInfo.firmware_version)));
        
        // Update button text to show disconnect option
        searchDeviceButton->setText("❌ Disconnect");
        
        // Enable firmware and other buttons
        updateFirmwareButton->setEnabled(true);
        checkVersionsButton->setEnabled(true);
        serialMonitorButton->setEnabled(true);
        deviceInfoButton->setEnabled(true);
        backupSettingsButton->setEnabled(true);
        restoreSettingsButton->setEnabled(true);
        
        logTextEdit->append(QString("Connected to diyPresso machine on %1")
            .arg(QString::fromStdString(deviceInfo.port)));
        
    } else {
        // Not connected
        deviceStatusLabel->setText("🔴 Not Connected");
        statusBar->showMessage("No diyPresso connected");
        
        // Reset firmware version display
        currentVersionLabel->setText("Current: Not connected");
        
        // Update button text to show search option
        searchDeviceButton->setText("🔍 Search for diyPresso");
        
        // Disable buttons that require connection
        updateFirmwareButton->setEnabled(false);
        checkVersionsButton->setEnabled(false);
        serialMonitorButton->setEnabled(false);
        deviceInfoButton->setEnabled(false);
        backupSettingsButton->setEnabled(false);
        restoreSettingsButton->setEnabled(false);
    }
}

// Slot implementations (placeholders for now)
void MainWindow::searchForDevice()
{
    if (device_->is_connected()) {
        // Device is connected, so disconnect it
        logTextEdit->append("Disconnecting from diyPresso machine...");
        statusBar->showMessage("Disconnecting...");
        
        device_->disconnect();
        updateConnectionStatus();
        
        logTextEdit->append("Disconnected from diyPresso machine.");
        
    } else {
        // Device is not connected, so search and connect
        logTextEdit->append("Searching for diyPresso machine...");
        statusBar->showMessage("Searching for diyPresso machine...");
        
        // Disable the search button while searching
        searchDeviceButton->setEnabled(false);
        searchDeviceButton->setText("⏳ Searching...");
        
        // Enable verbose mode for detailed feedback
        device_->set_verbose(false); // Keep it quiet for GUI
        
        if (device_->find_and_connect()) {
            // Successfully connected
            updateConnectionStatus();
            logTextEdit->append("Successfully connected to diyPresso machine!");
            
        } else {
            // Connection failed
            logTextEdit->append("❌ No diyPresso machine found. Please check:");
            logTextEdit->append("  • diyPresso is powered on");
            logTextEdit->append("  • USB cable is connected");
            
            statusBar->showMessage("No diyPresso machine found");
            
            // Re-enable search button
            searchDeviceButton->setEnabled(true);
            searchDeviceButton->setText("🔍 Search for diyPresso");
        }
        
        // Re-enable search button if not connected
        if (!device_->is_connected()) {
            searchDeviceButton->setEnabled(true);
        }
    }
}

void MainWindow::updateFirmware()
{
    if (!device_->is_connected()) {
        QMessageBox::warning(this, "Update Firmware", "Please connect to diyPresso machine first.");
        return;
    }
    
    logTextEdit->append("Starting firmware update...");
    statusBar->showMessage("Updating firmware...");
    
    // Disable buttons during update
    updateFirmwareButton->setEnabled(false);
    searchDeviceButton->setEnabled(false);
    
    // Show progress bar
    firmwareProgressBar->setVisible(true);
    firmwareProgressBar->setValue(0);
    
    // Create a progress dialog for detailed progress
    QProgressDialog progressDialog("Updating firmware...", "Cancel", 0, 100, this);
    progressDialog.setWindowModality(Qt::WindowModal);
    progressDialog.setAutoClose(false);
    progressDialog.setAutoReset(false);
    progressDialog.show();
    
    // Run firmware update in a separate thread to avoid blocking the GUI
    QThread* updateThread = QThread::create([this, &progressDialog]() {
        try {
            // Create firmware manager
            DpcFirmware firmwareManager(true); // Enable verbose mode for detailed feedback
            
            // Update progress for each step
            progressDialog.setValue(10);
            progressDialog.setLabelText("Downloading latest firmware...");
            QApplication::processEvents();
            
            // Start firmware upload (this will download latest firmware automatically)
            bool success = firmwareManager.uploadFirmware(device_.get());
            
            if (success) {
                progressDialog.setValue(100);
                progressDialog.setLabelText("Firmware update completed successfully!");
                
                // Update the GUI on the main thread
                QMetaObject::invokeMethod(this, [this]() {
                    firmwareProgressBar->setValue(100);
                    logTextEdit->append("✅ Firmware update completed successfully!");
                    statusBar->showMessage("Firmware update completed");
                    
                    // Re-enable buttons
                    updateFirmwareButton->setEnabled(true);
                    searchDeviceButton->setEnabled(true);
                    
                    // Hide progress bar after a delay
                    QTimer::singleShot(2000, [this]() {
                        firmwareProgressBar->setVisible(false);
                    });
                    
                    // Update device info to show new firmware version
                    updateConnectionStatus();
                    
                    QMessageBox::information(this, "Update Firmware", 
                        "Firmware update completed successfully!\n\n"
                        "The diyPresso machine will now restart with the new firmware.");
                }, Qt::QueuedConnection);
                
            } else {
                // Update the GUI on the main thread for error
                QMetaObject::invokeMethod(this, [this]() {
                    firmwareProgressBar->setVisible(false);
                    logTextEdit->append("❌ Firmware update failed!");
                    statusBar->showMessage("Firmware update failed");
                    
                    // Re-enable buttons
                    updateFirmwareButton->setEnabled(true);
                    searchDeviceButton->setEnabled(true);
                    
                    QMessageBox::critical(this, "Update Firmware", 
                        "Firmware update failed!\n\n"
                        "Please check the log for details and try again.");
                }, Qt::QueuedConnection);
            }
            
        } catch (const std::exception& e) {
            // Handle exceptions
            QMetaObject::invokeMethod(this, [this, errorMsg = QString::fromStdString(e.what())]() {
                firmwareProgressBar->setVisible(false);
                logTextEdit->append(QString("❌ Firmware update error: %1").arg(errorMsg));
                statusBar->showMessage("Firmware update error");
                
                // Re-enable buttons
                updateFirmwareButton->setEnabled(true);
                searchDeviceButton->setEnabled(true);
                
                QMessageBox::critical(this, "Update Firmware", 
                    QString("Firmware update failed with error:\n\n%1").arg(errorMsg));
            }, Qt::QueuedConnection);
        }
    });
    
    // Connect thread finished signal
    connect(updateThread, &QThread::finished, [&progressDialog, updateThread]() {
        progressDialog.close();
        updateThread->deleteLater();
    });
    
    // Start the update thread
    updateThread->start();
}

void MainWindow::backupSettings()
{
    if (!device_->is_connected()) {
        QMessageBox::warning(this, "Backup Settings", "Please connect to diyPresso machine first.");
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(this, 
        "Save Settings Backup", 
        "settings_backup.json", 
        "JSON files (*.json)");
    
    if (!fileName.isEmpty()) {
        logTextEdit->append(QString("Backing up settings to: %1").arg(fileName));
        statusBar->showMessage("Backing up settings...");
        
        try {
            // Create settings manager
            DpcSettings settingsManager;
            
            // Get settings from device
            auto settings = settingsManager.get_settings(*device_);
            
            // Save to file
            settingsManager.save_to_file(settings, fileName.toStdString());
            
            logTextEdit->append(QString("✅ Settings backup completed successfully!"));
            statusBar->showMessage("Settings backup completed");
            
            QMessageBox::information(this, "Backup Settings", 
                QString("Settings backup completed successfully!\n\n"
                       "Saved %1 settings to:\n%2")
                .arg(settings.size())
                .arg(fileName));
                
        } catch (const std::exception& e) {
            QString errorMsg = QString::fromStdString(e.what());
            logTextEdit->append(QString("❌ Settings backup failed: %1").arg(errorMsg));
            statusBar->showMessage("Settings backup failed");
            
            QMessageBox::critical(this, "Backup Settings", 
                QString("Settings backup failed:\n\n%1").arg(errorMsg));
        }
    }
}

void MainWindow::restoreSettings()
{
    if (!device_->is_connected()) {
        QMessageBox::warning(this, "Restore Settings", "Please connect to diyPresso machine first.");
        return;
    }
    
    QString fileName = QFileDialog::getOpenFileName(this, 
        "Load Settings Backup", 
        "", 
        "JSON files (*.json)");
    
    if (!fileName.isEmpty()) {
        logTextEdit->append(QString("Restoring settings from: %1").arg(fileName));
        statusBar->showMessage("Restoring settings...");
        
        try {
            // Create settings manager
            DpcSettings settingsManager;
            
            // Load settings from file
            auto settings = settingsManager.load_from_file(fileName.toStdString());
            
            // Validate settings before uploading
            if (settings.empty()) {
                throw std::runtime_error("No valid settings found in the backup file");
            }
            
            // Confirm with user
            QMessageBox::StandardButton reply = QMessageBox::question(this, "Restore Settings",
                QString("This will overwrite the current settings on the diyPresso machine.\n\n"
                       "Found %1 settings in the backup file.\n\n"
                       "Do you want to continue?").arg(settings.size()),
                QMessageBox::Yes | QMessageBox::No);
            
            if (reply == QMessageBox::Yes) {
                // Upload settings to device
                settingsManager.put_settings(*device_, settings);
                
                logTextEdit->append(QString("✅ Settings restore completed successfully!"));
                statusBar->showMessage("Settings restore completed");
                
                QMessageBox::information(this, "Restore Settings", 
                    QString("Settings restore completed successfully!\n\n"
                           "Restored %1 settings to the diyPresso machine.")
                    .arg(settings.size()));
            } else {
                logTextEdit->append("Settings restore cancelled by user");
                statusBar->showMessage("Settings restore cancelled");
            }
                
        } catch (const std::exception& e) {
            QString errorMsg = QString::fromStdString(e.what());
            logTextEdit->append(QString("❌ Settings restore failed: %1").arg(errorMsg));
            statusBar->showMessage("Settings restore failed");
            
            QMessageBox::critical(this, "Restore Settings", 
                QString("Settings restore failed:\n\n%1").arg(errorMsg));
        }
    }
}

void MainWindow::showDeviceInfo()
{
    if (!device_->is_connected()) {
        QMessageBox::warning(this, "diyPresso Machine Info", "Please connect to diyPresso machine first.");
        return;
    }
    
    logTextEdit->append("Displaying diyPresso machine information...");
    
    auto deviceInfo = device_->get_device_info();
    
    QString infoText = QString(
        "diyPresso Machine Information\n\n"
        "Port: %1\n"
        "Firmware Version: %2\n"
        "Mode: %3\n"
        "Vendor ID: 0x%4\n"
        "Product ID: 0x%5\n"
        "API Support: %6"
    ).arg(QString::fromStdString(deviceInfo.port))
     .arg(QString::fromStdString(deviceInfo.firmware_version))
     .arg(deviceInfo.bootloader_mode ? "Bootloader" : "Normal")
     .arg(deviceInfo.vendor_id, 4, 16, QChar('0'))
     .arg(deviceInfo.product_id, 4, 16, QChar('0'))
     .arg(device_->supports_api() ? "Yes" : "No (pre-1.6.2)");
    
    QMessageBox::information(this, "diyPresso Machine Info", infoText);
}

void MainWindow::checkVersions()
{
    logTextEdit->append("Checking firmware versions...");
    statusBar->showMessage("Checking versions...");
    
    // Disable button during check
    checkVersionsButton->setEnabled(false);
    
    // Run version check in a separate thread
    QThread* versionThread = QThread::create([this]() {
        try {
            // Create download manager
            DpcDownload downloader(true); // Enable verbose mode
            
            // Get latest version info
            auto latestRelease = downloader.getLatestRelease();
            
            if (latestRelease.empty()) {
                throw std::runtime_error("Failed to get latest release information");
            }
            
            // Extract version information
            std::string latestVersion = latestRelease.value("tag_name", "unknown");
            std::string releaseDate = latestRelease.value("published_at", "unknown");
            std::string releaseNotes = latestRelease.value("body", "No release notes available");
            
            // Get current version from device (if connected)
            std::string currentVersion = "Not connected";
            if (device_->is_connected()) {
                currentVersion = device_->get_device_info().firmware_version;
            }
            
            // Update GUI on main thread
            QMetaObject::invokeMethod(this, [this, latestVersion, currentVersion, releaseDate, releaseNotes]() {
                // Update the latest version label
                latestVersionLabel->setText(QString("Latest: %1").arg(QString::fromStdString(latestVersion)));
                
                // Show detailed version information
                QString versionInfo = QString(
                    "Firmware Version Information\n\n"
                    "Current Version: %1\n"
                    "Latest Version: %2\n"
                    "Release Date: %3\n\n"
                    "Release Notes:\n%4"
                ).arg(QString::fromStdString(currentVersion))
                 .arg(QString::fromStdString(latestVersion))
                 .arg(QString::fromStdString(releaseDate))
                 .arg(QString::fromStdString(releaseNotes));
                
                logTextEdit->append("✅ Version check completed");
                statusBar->showMessage("Version check completed");
                
                QMessageBox::information(this, "Firmware Versions", versionInfo);
                
                // Re-enable button
                checkVersionsButton->setEnabled(true);
                
            }, Qt::QueuedConnection);
            
        } catch (const std::exception& e) {
            // Handle errors
            QMetaObject::invokeMethod(this, [this, errorMsg = QString::fromStdString(e.what())]() {
                logTextEdit->append(QString("❌ Version check failed: %1").arg(errorMsg));
                statusBar->showMessage("Version check failed");
                
                QMessageBox::critical(this, "Version Check", 
                    QString("Failed to check firmware versions:\n\n%1").arg(errorMsg));
                
                // Re-enable button
                checkVersionsButton->setEnabled(true);
            }, Qt::QueuedConnection);
        }
    });
    
    // Connect thread finished signal
    connect(versionThread, &QThread::finished, [versionThread]() {
        versionThread->deleteLater();
    });
    
    // Start the version check thread
    versionThread->start();
}

void MainWindow::showSerialMonitor()
{
    logTextEdit->append("Opening serial monitor...");
    // TODO: Create separate SerialMonitorDialog or integrate with DpcSerial
    QMessageBox::information(this, "Serial Monitor", "Serial monitor functionality will be integrated with backend.");
} 