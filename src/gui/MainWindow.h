#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QGroupBox>
#include <QProgressBar>
#include <QStatusBar>
#include "../core/DpcDevice.h"
#include <memory>

QT_BEGIN_NAMESPACE
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void searchForDevice();
    void updateFirmware();
    void checkVersions();
    void backupSettings();
    void restoreSettings();
    void showDeviceInfo();
    void showSerialMonitor();

private:
    void setupUi();
    void updateDeviceStatus();
    void updateConnectionStatus();
    
    // Backend Integration
    std::unique_ptr<DpcDevice> device_;
    
    // UI Elements
    QWidget *centralWidget;
    QVBoxLayout *mainLayout;
    
    // Device Status Section
    QGroupBox *deviceGroup;
    QLabel *deviceStatusLabel;
    QPushButton *searchDeviceButton;
    
    // Firmware Section
    QGroupBox *firmwareGroup;
    QLabel *currentVersionLabel;
    QLabel *latestVersionLabel;
    QPushButton *updateFirmwareButton;
    QPushButton *checkVersionsButton;
    QProgressBar *firmwareProgressBar;
    
    // Settings Section
    QGroupBox *settingsGroup;
    QPushButton *backupSettingsButton;
    QPushButton *restoreSettingsButton;
    
    // Advanced Section
    QGroupBox *advancedGroup;
    QPushButton *serialMonitorButton;
    QPushButton *deviceInfoButton;
    
    // Status and Log
    QTextEdit *logTextEdit;
    QStatusBar *statusBar;
};

#endif // MAINWINDOW_H 