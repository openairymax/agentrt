# AgentOS Windows Setup Script
# Version: 1.0.0
# Description: Windows development environment setup and build script for AgentOS
# Compatible with: Windows 10/11, PowerShell 5.1+

param(
    [switch]$Help,
    [string]$BuildType = "Debug",
    [string]$Generator = "Visual Studio 17 2022",
    [switch]$SkipDeps,
    [switch]$Clean,
    [switch]$Test
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

function Write-Header {
    param([string]$Message)
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host " $Message" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
}

function Test-Command {
    param([string]$Command)
    try {
        $null = Get-Command $Command -ErrorAction Stop
        return $true
    } catch {
        return $false
    }
}

if ($Help) {
    Write-Host @"
AgentOS Windows Setup Script
============================

Usage: .\setup.ps1 [options]

Options:
  -Help              Show this help message
  -BuildType <type>  Build type (Debug|Release|RelWithDebInfo) [Default: Debug]
  -Generator <gen>   CMake generator [Default: Visual Studio 17 2022]
  -SkipDeps          Skip dependency installation
  -Clean             Clean build directory before building
  -Test              Run tests after build

Examples:
  .\setup.ps1                              # Debug build with default settings
  .\setup.ps1 -BuildType Release          # Release build
  .\setup.ps1 -Clean -Test               # Clean build and run tests
"@
    exit 0
}

Write-Host ""
Write-Host "╔══════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║     AgentOS Windows Setup Script v1.0    ║" -ForegroundColor Green
Write-Host "╚══════════════════════════════════════════╝" -ForegroundColor Green
Write-Host ""

$ScriptPath = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = if (Test-Path "$ScriptPath\..\CMakeLists.txt") { Resolve-Path "$ScriptPath\.." } else { $ScriptPath }
$BuildDir = "$ProjectRoot\build"

Write-Host "[INFO] Project root: $ProjectRoot"
Write-Host "[INFO] Build directory: $BuildDir"

# Step 1: Check prerequisites
Write-Header "Step 1: Checking Prerequisites"

$PrerequisitesOK = $true

# Check CMake
if (Test-Command cmake) {
    $cmakeVersion = (cmake --version | Select-String "\d+\.\d+\.\d+").Matches[0].Value
    Write-Host "[✓] CMake found: $cmakeVersion" -ForegroundColor Green
} else {
    Write-Host "[✗] CMake not found. Please install CMake from https://cmake.org/download/" -ForegroundColor Red
    $PrerequisitesOK = $false
}

# Check Visual Studio Build Tools
if (Get-ItemProperty "HKLM:\SOFTWARE\Microsoft\VisualStudio\*\*" -ErrorAction SilentlyContinue) {
    Write-Host "[✓] Visual Studio/Build Tools installed" -ForegroundColor Green
} else {
    Write-Host "[!] Visual Studio not detected. MSVC compiler may not be available." -ForegroundColor Yellow
}

# Check Git
if (Test-Command git) {
    $gitVersion = (git --version)
    Write-Host "[✓] Git found: $gitVersion" -ForegroundColor Green
} else {
    Write-Host "[!] Git not found. Some features may be limited." -ForegroundColor Yellow
}

if (-not $PrerequisitesOK) {
    Write-Host "`n[ERROR] Prerequisites check failed. Please install missing dependencies." -ForegroundColor Red
    exit 1
}

# Step 2: Install dependencies (if needed)
if (-not $SkipDeps) {
    Write-Header "Step 2: Installing Dependencies"

    # Install vcpkg if not present
    $vcpkgRoot = "$env:USERPROFILE\vcpkg"
    if (-not (Test-Path $vcpkgRoot)) {
        Write-Host "[INFO] Installing vcpkg..." -ForegroundColor Yellow
        git clone https://github.com/microsoft/vcpkg.git $vcpkgRoot
        Push-Location $vcpkgRoot
        .\bootstrap-vcpkg.bat
        Pop-Location
        Write-Host "[✓] vcpkg installed successfully" -ForegroundColor Green
    } else {
        Write-Host "[✓] vcpkg already installed at $vcpkgRoot" -ForegroundColor Green
    }

    # Set vcpkg environment
    $env:VCPKG_ROOT = $vcpkgRoot
    $env:Path += ";$vcpkgRoot"
} else {
    Write-Host "[INFO] Skipping dependency installation..." -ForegroundColor Yellow
}

# Step 3: Configure CMake
Write-Header "Step 3: Configuring CMake"

if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "[INFO] Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir -ErrorAction SilentlyContinue
}

if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
}

Push-Location $BuildDir

$cmakeArgs = @(
    "..",
    "-G", $Generator,
    "-A", "x64",
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake",
    "-DBUILD_TESTS=ON",
    "-DBUILD_EXAMPLES=ON",
    "-DENABLE_SANITIZERS=OFF",
    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
)

Write-Host "[INFO] Running CMake configuration..." -ForegroundColor Yellow
Write-Host "[CMD] cmake $($cmakeArgs -join ' ')" -ForegroundColor Gray

try {
    & cmake @cmakeArgs
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configuration failed with exit code $LASTEXITCODE"
    }
    Write-Host "[✓] CMake configuration completed successfully" -ForegroundColor Green
} catch {
    Write-Host "[ERROR] $_" -ForegroundColor Red
    Pop-Location
    exit 1
}

# Step 4: Build project
Write-Header "Step 4: Building Project"

$configArg = "--config", $BuildType
$buildTarget = if ($Test) { "ALL_BUILD" } else { "AgentOS" }

Write-Host "[INFO] Building target: $buildTarget ($BuildType)..." -ForegroundColor Yellow
Write-Host "[CMD] cmake --build . --config $BuildType --target $buildTarget" -ForegroundColor Gray

try {
    & cmake --build . @configArg --target $buildTarget -- /nologo /verbosity:minimal /maxcpucount
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE"
    }
    Write-Host "[✓] Build completed successfully" -ForegroundColor Green
} catch {
    Write-Host "[ERROR] $_" -ForegroundColor Red
    Pop-Location
    exit 1
}

# Step 5: Run tests (if requested)
if ($Test) {
    Write-Header "Step 5: Running Tests"

    Write-Host "[INFO] Running test suite..." -ForegroundColor Yellow

    & ctest --output-on-failure -C $BuildType

    if ($LASTEXITCODE -eq 0) {
        Write-Host "[✓] All tests passed!" -ForegroundColor Green
    } else {
        Write-Host "[WARNING] Some tests failed. Check output above." -ForegroundColor Yellow
    }
}

Pop-Location

# Summary
Write-Header "Setup Complete!"

Write-Host @"
Build Configuration:
  - Type: $BuildType
  - Generator: $Generator
  - Directory: $BuildDir

Next Steps:
  1. Run the daemon: cd build && ctest -R daemon_test -C $BuildType
  2. Run gateway: .\build\bin\$BuildType\gateway_d.exe
  3. Run tests: cd build && ctest -C $BuildType

For more information, see docs/BUILD.md
"@ -ForegroundColor White

Write-Host ""
Write-Host "🎉 AgentOS Windows setup completed successfully!" -ForegroundColor Green
Write-Host ""
