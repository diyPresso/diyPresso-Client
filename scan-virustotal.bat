@echo off
echo VirusTotal Scanner for diyPresso
echo ==================================

set VT_CLI=bin\virus-total\vt.exe
set PACKAGE_DIR=bin\package-win

REM Quick checks
if not exist "%VT_CLI%" (
    echo ERROR: VirusTotal CLI not found at: %VT_CLI%
    pause
    exit /b 1
)

if not exist "%PACKAGE_DIR%" (
    echo ERROR: Package directory not found: %PACKAGE_DIR%
    echo Run 'build-windows.bat' first.
    pause
    exit /b 1
)

REM Upload files
echo.
echo Uploading files to VirusTotal...
for %%f in ("%PACKAGE_DIR%\*.exe" "%PACKAGE_DIR%\*.dll") do (
    if exist "%%f" (
        echo Uploading: %%~nxf
        "%VT_CLI%" scan file "%%f"
    )
)

echo.
echo Upload complete! Use 'check-virustotal.bat' to view results.
pause 