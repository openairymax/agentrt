@echo off
REM Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
REM AgentOS Atoms Module Build Script for Windows
REM Version: 1.0.0
REM
REM 本脚本遵循《工程控制论》的反馈闭环原则
REM 提供Windows平台的统一构建入口

setlocal EnableDelayedExpansion

REM ==================== 全局变量 ====================

set SCRIPT_DIR=%~dp0
set ATOMS_ROOT=%SCRIPT_DIR%..
set VERSION=1.0.0.5
set BUILD_TYPE=Release
set BUILD_DIR=build
set INSTALL_PREFIX=%ATOMS_ROOT%\dist
set PARALLEL_JOBS=%NUMBER_OF_PROCESSORS%
set VERBOSE=0

REM ==================== 颜色输出 ====================

REM Windows 10+ 支持ANSI颜色
for /f "tokens=4-5 delims=. " %%i in ('ver') do set VERSION=%%i.%%j
if %VERSION% GEQ 10.0 (
    set RED=[31m
    set GREEN=[32m
    set YELLOW=[33m
    set BLUE=[34m
    set NC=[0m
) else (
    set RED=
    set GREEN=
    set YELLOW=
    set BLUE=
    set NC=
)

REM ==================== 辅助函数 ====================

:log_info
echo %BLUE%[INFO]%NC% %~1
goto :eof

:log_success
echo %GREEN%[SUCCESS]%NC% %~1
goto :eof

:log_warning
echo %YELLOW%[WARNING]%NC% %~1
goto :eof

:log_error
echo %RED%[ERROR]%NC% %~1 >&2
goto :eof

:die
call :log_error "%~1"
exit /b 1

:check_command
where %~1 >nul 2>&1
if errorlevel 1 (
    call :die "Required command '%~1' not found. Please install it first."
)
goto :eof

:detect_arch
if "%PROCESSOR_ARCHITECTURE%"=="AMD64" (
    echo x86_64
) else if "%PROCESSOR_ARCHITECTURE%"=="ARM64" (
    echo arm64
) else (
    echo unknown
)
goto :eof

REM ==================== 构建函数 ====================

:configure_cmake
set MODULE=%~1
set MODULE_DIR=%ATOMS_ROOT%\%MODULE%
set BUILD_DIR_FULL=%MODULE_DIR%\%BUILD_DIR%

call :log_info "Configuring %MODULE%..."

if not exist "%MODULE_DIR%" (
    call :die "Module directory not found: %MODULE_DIR%"
)

if not exist "%BUILD_DIR_FULL%" mkdir "%BUILD_DIR_FULL%"
pushd "%BUILD_DIR_FULL%"

REM 设置MSVC环境（如果可用）
if defined VSINSTALLDIR (
    call :log_info "Using Visual Studio: %VSINSTALLDIR%"
) else (
    REM 尝试查找Visual Studio
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
    ) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
    )
)

REM 配置CMake
cmake "%MODULE_DIR%" ^
    -G "Ninja" ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DCMAKE_INSTALL_PREFIX="%INSTALL_PREFIX%" ^
    -DBUILD_TESTS=ON ^
    -DBUILD_EXAMPLES=OFF ^
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ^
    -DCMAKE_C_FLAGS="/W4 /WX-"

if errorlevel 1 (
    popd
    call :die "CMake configuration failed for %MODULE%"
)

popd
call :log_success "Configuration complete for %MODULE%"
goto :eof

:build_module
set MODULE=%~1
set MODULE_DIR=%ATOMS_ROOT%\%MODULE%
set BUILD_DIR_FULL=%MODULE_DIR%\%BUILD_DIR%

call :log_info "Building %MODULE%..."

pushd "%BUILD_DIR_FULL%"

ninja -j%PARALLEL_JOBS%

if errorlevel 1 (
    popd
    call :die "Build failed for %MODULE%"
)

popd
call :log_success "Build complete for %MODULE%"
goto :eof

:run_tests
set MODULE=%~1
set MODULE_DIR=%ATOMS_ROOT%\%MODULE%
set BUILD_DIR_FULL=%MODULE_DIR%\%BUILD_DIR%

call :log_info "Running tests for %MODULE%..."

pushd "%BUILD_DIR_FULL%"

ctest --output-on-failure --timeout 300 -j%PARALLEL_JOBS%

if errorlevel 1 (
    popd
    call :log_warning "Some tests failed for %MODULE%"
) else (
    popd
    call :log_success "All tests passed for %MODULE%"
)
goto :eof

:install_module
set MODULE=%~1
set MODULE_DIR=%ATOMS_ROOT%\%MODULE%
set BUILD_DIR_FULL=%MODULE_DIR%\%BUILD_DIR%

call :log_info "Installing %MODULE%..."

pushd "%BUILD_DIR_FULL%"

ninja install

if errorlevel 1 (
    popd
    call :die "Installation failed for %MODULE%"
)

popd
call :log_success "Installation complete for %MODULE%"
goto :eof

:package_module
set MODULE=%~1
set MODULE_DIR=%ATOMS_ROOT%\%MODULE%
set BUILD_DIR_FULL=%MODULE_DIR%\%BUILD_DIR%

REM 检测架构
for /f %%i in ('call :detect_arch') do set ARCH=%%i
set PACKAGE_NAME=atoms-%MODULE%-%VERSION%-windows-%ARCH%-msvc

call :log_info "Packaging %MODULE%..."

if not exist "%BUILD_DIR_FULL%\package\lib" mkdir "%BUILD_DIR_FULL%\package\lib"
if not exist "%BUILD_DIR_FULL%\package\include" mkdir "%BUILD_DIR_FULL%\package\include"

REM 复制静态库
for /r "%BUILD_DIR_FULL%" %%f in (*.lib) do copy "%%f" "%BUILD_DIR_FULL%\package\lib\" >nul

REM 复制头文件
if exist "%MODULE_DIR%\include" (
    xcopy "%MODULE_DIR%\include\*" "%BUILD_DIR_FULL%\package\include\" /E /I /Y >nul
)

REM 创建压缩包（需要7-Zip或PowerShell）
pushd "%BUILD_DIR_FULL%\package"
powershell -Command "Compress-Archive -Path * -DestinationPath '%ATOMS_ROOT%\%PACKAGE_NAME%.zip' -Force"

popd
call :log_success "Package created: %PACKAGE_NAME%.zip"
goto :eof

REM ==================== 清理函数 ====================

:clean_module
set MODULE=%~1
set MODULE_DIR=%ATOMS_ROOT%\%MODULE%

call :log_info "Cleaning %MODULE%..."

if exist "%MODULE_DIR%\%BUILD_DIR%" (
    rmdir /s /q "%MODULE_DIR%\%BUILD_DIR%"
)

call :log_success "Clean complete for %MODULE%"
goto :eof

:clean_all
call :log_info "Cleaning all modules..."

for %%m in (corekern coreloopthree memoryrovol syscall utils) do (
    if exist "%ATOMS_ROOT%\%%m" (
        call :clean_module %%m
    )
)

if exist "%INSTALL_PREFIX%" (
    rmdir /s /q "%INSTALL_PREFIX%"
)

call :log_success "Clean complete for all modules"
goto :eof

REM ==================== 主构建流程 ====================

:build_all
call :log_info "Building all atoms modules..."
call :log_info "Build Type: %BUILD_TYPE%"
call :log_info "Parallel Jobs: %PARALLEL_JOBS%"

set FAILED_MODULES=

for %%m in (corekern coreloopthree memoryrovol syscall utils) do (
    if exist "%ATOMS_ROOT%\%%m" (
        call :configure_cmake %%m
        if errorlevel 1 (
            set FAILED_MODULES=!FAILED_MODULES! %%m
        ) else (
            call :build_module %%m
            if errorlevel 1 (
                set FAILED_MODULES=!FAILED_MODULES! %%m
            )
        )
    )
)

if not "%FAILED_MODULES%"=="" (
    call :log_error "Failed modules:%FAILED_MODULES%"
    exit /b 1
)

call :log_success "All modules built successfully!"
goto :eof

REM ==================== 帮助信息 ====================

:show_help
echo AgentOS Atoms Module Build Script for Windows
echo Version: %VERSION%
echo.
echo Usage: %~nx0 [command] [options]
echo.
echo Commands:
echo     configure module    Configure a specific module
echo     build module        Build a specific module
echo     test module         Run tests for a specific module
echo     install module      Install a specific module
echo     package module      Package a specific module
echo     clean module        Clean a specific module
echo     clean-all           Clean all modules
echo     build-all           Build all modules
echo     help                Show this help message
echo.
echo Options:
echo     BUILD_TYPE=Release^|Debug    Set build type (default: Release)
echo     BUILD_DIR=build             Set build directory (default: build)
echo     INSTALL_PREFIX=path         Set install prefix (default: dist\)
echo     PARALLEL_JOBS=N             Set parallel jobs (default: auto-detect)
echo.
echo Examples:
echo     %~nx0 build-all
echo     %~nx0 build corekern
echo     %~nx0 test coreloopthree
echo     set BUILD_TYPE=Debug ^&^& %~nx0 build memoryrovol
echo.
goto :eof

REM ==================== 入口点 ====================

:main
REM 检查必需工具
call :check_command cmake
call :check_command ninja

REM 解析命令
set COMMAND=%1
if "%COMMAND%"=="" set COMMAND=help
shift

REM 处理环境变量
if defined BUILD_TYPE_ENV set BUILD_TYPE=%BUILD_TYPE_ENV%
if defined BUILD_DIR_ENV set BUILD_DIR=%BUILD_DIR_ENV%
if defined INSTALL_PREFIX_ENV set INSTALL_PREFIX=%INSTALL_PREFIX_ENV%
if defined PARALLEL_JOBS_ENV set PARALLEL_JOBS=%PARALLEL_JOBS_ENV%

if "%COMMAND%"=="configure" (
    if "%~1"=="" call :die "Usage: %~nx0 configure module"
    call :configure_cmake %~1
) else if "%COMMAND%"=="build" (
    if "%~1"=="" call :die "Usage: %~nx0 build module"
    call :configure_cmake %~1
    call :build_module %~1
) else if "%COMMAND%"=="test" (
    if "%~1"=="" call :die "Usage: %~nx0 test module"
    call :run_tests %~1
) else if "%COMMAND%"=="install" (
    if "%~1"=="" call :die "Usage: %~nx0 install module"
    call :install_module %~1
) else if "%COMMAND%"=="package" (
    if "%~1"=="" call :die "Usage: %~nx0 package module"
    call :package_module %~1
) else if "%COMMAND%"=="clean" (
    if "%~1"=="" call :die "Usage: %~nx0 clean module"
    call :clean_module %~1
) else if "%COMMAND%"=="clean-all" (
    call :clean_all
) else if "%COMMAND%"=="build-all" (
    call :build_all
) else if "%COMMAND%"=="help" (
    call :show_help
) else if "%COMMAND%"=="--help" (
    call :show_help
) else if "%COMMAND%"=="-h" (
    call :show_help
) else (
    call :die "Unknown command: %COMMAND%. Use 'help' for usage information."
)

endlocal
