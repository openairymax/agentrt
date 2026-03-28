@echo off
REM Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
REM lodges Module Build Script for Windows
REM "From data intelligence emerges."

setlocal EnableDelayedExpansion

REM Configuration
set MODULE_NAME=lodges
set VERSION=1.0.0.6
set SCRIPT_DIR=%~dp0
set PROJECT_ROOT=%SCRIPT_DIR%..
set BUILD_DIR=%PROJECT_ROOT%\build
set BUILD_TYPE=Release
set INSTALL_PREFIX=C:\Program Files\AgentOS
set JOBS=%NUMBER_OF_PROCESSORS%

REM Parse arguments
set COMMAND=all
if not "%1"=="" set COMMAND=%1

REM Parse environment variables
if not "%BUILD_TYPE%"=="" set BUILD_TYPE=%BUILD_TYPE%
if not "%INSTALL_PREFIX%"=="" set INSTALL_PREFIX=%INSTALL_PREFIX%

echo ========================================
echo   AgentOS lodges Build System
echo   Version: %VERSION%
echo   Build Type: %BUILD_TYPE%
echo ========================================
echo.

REM Check prerequisites
:check_prerequisites
echo [INFO] Checking prerequisites...

where cmake >nul 2>&1
if errorlevel 1 (
    echo [ERROR] cmake not found. Please install CMake.
    exit /b 1
)

where cl >nul 2>&1
if errorlevel 1 (
    where gcc >nul 2>&1
    if errorlevel 1 (
        where clang >nul 2>&1
        if errorlevel 1 (
            echo [ERROR] No C compiler found. Please install MSVC, GCC, or Clang.
            exit /b 1
        )
    )
)

echo [SUCCESS] All prerequisites satisfied
echo.

REM Main command dispatcher
if "%COMMAND%"=="configure" goto configure
if "%COMMAND%"=="build" goto build
if "%COMMAND%"=="test" goto test
if "%COMMAND%"=="install" goto install
if "%COMMAND%"=="clean" goto clean
if "%COMMAND%"=="package" goto package
if "%COMMAND%"=="all" goto all
if "%COMMAND%"=="help" goto help
if "%COMMAND%"=="--help" goto help
if "%COMMAND%"=="-h" goto help

echo [ERROR] Unknown command: %COMMAND%
goto help

:configure
echo [INFO] Configuring build...

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

cmake "%PROJECT_ROOT%" ^
    -G "Ninja" ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DCMAKE_INSTALL_PREFIX="%INSTALL_PREFIX%" ^
    -DBUILD_TESTS=ON ^
    -DBUILD_BENCHMARK=ON

if errorlevel 1 (
    echo [ERROR] Configuration failed
    exit /b 1
)

echo [SUCCESS] Configuration complete
goto end

:build
echo [INFO] Building project...

cd /d "%BUILD_DIR%"

cmake --build . --manager %BUILD_TYPE% --parallel %JOBS%

if errorlevel 1 (
    echo [ERROR] Build failed
    exit /b 1
)

echo [SUCCESS] Build complete
goto end

:test
echo [INFO] Running tests...

cd /d "%BUILD_DIR%"

ctest --output-on-failure -C %BUILD_TYPE%

if errorlevel 1 (
    echo [ERROR] Tests failed
    exit /b 1
)

echo [SUCCESS] All tests passed
goto end

:install
echo [INFO] Installing to %INSTALL_PREFIX%...

cd /d "%BUILD_DIR%"

cmake --install . --manager %BUILD_TYPE%

if errorlevel 1 (
    echo [ERROR] Installation failed
    exit /b 1
)

echo [SUCCESS] Installation complete
goto end

:clean
echo [INFO] Cleaning build artifacts...

if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"

echo [SUCCESS] Clean complete
goto end

:package
echo [INFO] Creating distribution package...

set DIST_DIR=%PROJECT_ROOT%\dist
set PACKAGE_NAME=agentos-%MODULE_NAME%-%VERSION%-windows-x64

if not exist "%DIST_DIR%" mkdir "%DIST_DIR%"

cd /d "%BUILD_DIR%"

REM Create zip archive
powershell -Command "Compress-Archive -Path '%BUILD_DIR%\*.lib', '%PROJECT_ROOT%\include\*.h' -DestinationPath '%DIST_DIR%\%PACKAGE_NAME%.zip' -Force"

echo [SUCCESS] Package created: %DIST_DIR%\%PACKAGE_NAME%.zip
goto end

:all
call :check_prerequisites
call :configure
if errorlevel 1 exit /b 1
call :build
if errorlevel 1 exit /b 1
call :test
if errorlevel 1 exit /b 1
goto end

:help
echo Usage: %~nx0 [command] [options]
echo.
echo Commands:
echo   configure    Configure the build system
echo   build        Build the project
echo   test         Run unit tests
echo   install      Install the library
echo   clean        Remove build artifacts
echo   package      Create distribution package
echo   all          Run configure, build, and test
echo   help         Show this help message
echo.
echo Environment Variables:
echo   BUILD_TYPE       Build type (Release^|Debug^|RelWithDebInfo) [default: Release]
echo   INSTALL_PREFIX   Installation prefix [default: C:\Program Files\AgentOS]
echo.
echo Examples:
echo   %~nx0 all                    Configure, build, and test
echo   set BUILD_TYPE=Debug
echo   %~nx0 build                  Debug build
goto end

:end
endlocal

