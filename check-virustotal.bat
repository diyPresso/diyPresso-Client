@echo off
setlocal enabledelayedexpansion
echo VirusTotal Results Checker
echo ==========================

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
    pause
    exit /b 1
)

echo.
echo Checking scan status and results...
echo.

REM Initialize issues tracking
set ISSUES_FOUND=0
set ISSUE_FILES=

REM Check each file
for %%f in ("%PACKAGE_DIR%\*.exe" "%PACKAGE_DIR%\*.dll") do (
    if exist "%%f" (
        echo =====================================
        echo File: %%~nxf
        echo =====================================
        echo Getting file hash...
        for /f %%h in ('certutil -hashfile "%%f" SHA256 ^| findstr /r "^[0-9a-f]*$"') do set file_hash=%%h
        echo Hash: !file_hash!
        echo https://www.virustotal.com/gui/file/!file_hash!
        echo.
        
        REM Get VirusTotal analysis using hash
        "%VT_CLI%" file !file_hash! > temp_full_result.txt 2>&1
        
        if exist temp_full_result.txt (
            REM Find last_analysis_stats and show next 9 lines using PowerShell
            powershell -Command "$lines = Get-Content temp_full_result.txt; $index = $lines | Select-String -Pattern 'last_analysis_stats:' | Select-Object -First 1 | ForEach-Object {$_.LineNumber - 1}; if ($index -ne $null) { $lines[$index..($index+8)] }"
            
            REM Check for issues in this file - look for non-zero malicious/suspicious
            findstr "    malicious: 0" temp_full_result.txt >nul 2>&1
            set malicious_zero=!ERRORLEVEL!
            findstr "    suspicious: 0" temp_full_result.txt >nul 2>&1
            set suspicious_zero=!ERRORLEVEL!
            
            if !malicious_zero! neq 0 (
                set ISSUES_FOUND=1
                set ISSUE_FILES=!ISSUE_FILES! %%~nxf
            )
            if !suspicious_zero! neq 0 (
                set ISSUES_FOUND=1
                if "!ISSUE_FILES!" == "!ISSUE_FILES:%%~nxf=!" set ISSUE_FILES=!ISSUE_FILES! %%~nxf
            )
        )
        
        :skip_vt_query
        REM Clean up temp files
        if exist temp_stats.txt del temp_stats.txt
        if exist temp_full_result.txt del temp_full_result.txt
        if exist temp_auth.txt del temp_auth.txt
        echo.
    )
)

echo =====================================
echo Summary
echo =====================================
if !ISSUES_FOUND! equ 1 (
    echo Files with issues:
    echo !ISSUE_FILES!
) else (
    echo No issues found.
)
echo.
echo For detailed web view: https://www.virustotal.com/
pause 