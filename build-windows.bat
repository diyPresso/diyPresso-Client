@echo off
echo ===================================
echo diyPresso Client C++ - Windows Build
echo ===================================

REM Check if vcpkg is available
if not defined VCPKG_ROOT (
    echo ERROR: VCPKG_ROOT environment variable not set.
    echo Please install vcpkg and set VCPKG_ROOT to its installation directory.
    echo Example: set VCPKG_ROOT=C:\vcpkg
    pause
    exit /b 1
)

REM Install dependencies via vcpkg
echo Installing dependencies via vcpkg...
%VCPKG_ROOT%\vcpkg.exe install nlohmann-json:x64-windows
%VCPKG_ROOT%\vcpkg.exe install cli11:x64-windows
%VCPKG_ROOT%\vcpkg.exe install libusbp:x64-windows

REM Create build directory
if not exist build mkdir build
cd build

REM Configure with CMake
echo Configuring with CMake...
cmake .. -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows

if %ERRORLEVEL% neq 0 (
    echo CMake configuration failed!
    pause
    exit /b 1
)

REM Build the project
echo Building project...
cmake --build . --config Release

if %ERRORLEVEL% neq 0 (
    echo Build failed!
    pause
    exit /b 1
)

echo ===================================
echo Build completed successfully!
echo Executable: build\Release\diypresso.exe
echo ===================================



REM Ask user if they want to sign the executable and DLLs
echo.
set /p sign_choice="Do you want to sign the executable and DLLs? (y/N): "
if /i "%sign_choice%"=="y" (
    echo.
    echo Signing executable and DLLs...
    
    REM Check if the executable exists
    if not exist "Release\diypresso.exe" (
        echo ERROR: Executable not found at Release\diypresso.exe
        echo Skipping code signing.
        goto :end_signing
    )
    
    REM Sign the executable
    echo Signing diypresso.exe...
    signtool sign /n "diyPresso B.V." /t http://time.certum.pl/ /fd sha256 /v /a 2F4230F3EE88762E2194FAB43A2AD7DEEC1537A1 "Release\diypresso.exe"
    
    if %ERRORLEVEL% neq 0 (
        echo WARNING: Failed to sign diypresso.exe
    ) else (
        echo Successfully signed diypresso.exe
    )
    
    REM Sign all DLL files in the Release directory
    echo Signing DLL files...
    for %%f in ("Release\*.dll") do (
        echo Signing %%f...
        signtool sign /n "diyPresso B.V." /t http://time.certum.pl/ /fd sha256 /v /a 2F4230F3EE88762E2194FAB43A2AD7DEEC1537A1 "%%f"
        if %ERRORLEVEL% neq 0 (
            echo WARNING: Failed to sign %%f
        ) else (
            echo Successfully signed %%f
        )
    )
    
    echo Code signing process completed!
) else (
    echo Skipping code signing.
)

:end_signing

REM Ask user if they want to create a distribution package
echo.
set /p package_choice="Do you want to create a distribution package? (Y/n): "
if "%package_choice%"=="" set package_choice=y
if /i "%package_choice%"=="y" (
    echo.
    echo Creating distribution package...
    
    REM Create package directory
    if not exist "..\bin\package-win" mkdir "..\bin\package-win"
    
    REM Clean existing package
    del /Q "..\bin\package-win\*" 2>nul
    
    REM Copy executable
    if exist "Release\diypresso.exe" (
        echo Copying diypresso.exe...
        copy "Release\diypresso.exe" "..\bin\package-win\"
    ) else (
        echo ERROR: diypresso.exe not found!
        goto :end_packaging
    )
    
    REM Copy DLLs
    echo Copying DLL files...
    copy "Release\*.dll" "..\bin\package-win\" 2>nul
    
    REM Copy bossac executable
    if exist "..\bin\bossac\bossac.exe" (
        echo Copying bossac.exe...
        copy "..\bin\bossac\bossac.exe" "..\bin\package-win\"
    ) else (
        echo WARNING: bossac not found!
    )
    
    REM Copy firmware.bin
    if exist "..\bin\firmware\firmware.bin" (
        echo Copying firmware.bin...
        copy "..\bin\firmware\firmware.bin" "..\bin\package-win\"
    ) else (
        echo WARNING: firmware.bin not found!
    )
    
    REM Copy LICENSE
    if exist "..\LICENSE" (
        echo Copying LICENSE...
        copy "..\LICENSE" "..\bin\package-win\"
    ) else (
        echo WARNING: LICENSE not found!
    )
    
    REM Create ZIP package
    echo Creating ZIP package...
    powershell -Command "Compress-Archive -Path '..\bin\package-win\*' -DestinationPath '..\bin\diyPresso-Client-Windows.zip' -Force"
    
    if %ERRORLEVEL% neq 0 (
        echo WARNING: Failed to create ZIP package
    ) else (
        echo Successfully created diyPresso-Client-Windows.zip
    )
    
    echo.
    echo ===================================
    echo Package created successfully!
    echo Location: bin\package-win\
    echo ZIP file: bin\diyPresso-Client-Windows.zip
    echo ===================================
) else (
    echo Skipping package creation.
)

:end_packaging
echo.
echo ===================================
echo Build process completed!
echo ===================================

REM Return to project root
cd ..

pause 