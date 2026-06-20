# AgentRT Installer Script (Windows)
# Version: 0.1.1
# Usage: Invoke-WebRequest -Uri https://raw.githubusercontent.com/spharx/agentrt/main/scripts/get-agentrt.ps1 | Invoke-Expression
#        or: .\get-agentrt.ps1

param(
    [string]$Version = $env:AGENTRT_VERSION,
    [string]$InstallDir = $env:AGENTRT_INSTALL_DIR,
    [string]$BinDir = $env:AGENTRT_BIN_DIR
)

$ErrorActionPreference = "Stop"

# ── Defaults ──────────────────────────────────────────────
if (-not $Version) { $Version = "0.1.1" }
if (-not $InstallDir) { $InstallDir = "$env:USERPROFILE\.agentrt" }
if (-not $BinDir) { $BinDir = "$env:USERPROFILE\.agentrt\bin" }
$RepoUrl = "https://github.com/spharx/agentrt"

# ── Helpers ───────────────────────────────────────────────
function Write-Info  { Write-Host "[INFO] $args" -ForegroundColor Cyan }
function Write-OK    { Write-Host "[OK]   $args" -ForegroundColor Green }
function Write-Warn  { Write-Host "[WARN] $args" -ForegroundColor Yellow }
function Write-Fail  { Write-Host "[FAIL] $args" -ForegroundColor Red; exit 1 }

Write-Host ""
Write-Host "═══════════════════════════════════════════════" -ForegroundColor White
Write-Host "   AgentRT v$Version Installer (Windows)" -ForegroundColor White
Write-Host "═══════════════════════════════════════════════" -ForegroundColor White
Write-Host ""

# ── Step 1: Detect platform ───────────────────────────────
Write-Info "Step 1/6: Detecting platform..."

if (-not ($IsWindows -or $env:OS -eq "Windows_NT")) {
    Write-Fail "This script is for Windows only. Use get-agentrt.sh for Linux/macOS."
}

$Arch = $env:PROCESSOR_ARCHITECTURE
switch ($Arch) {
    "AMD64" { $Arch = "amd64" }
    "ARM64" { $Arch = "arm64" }
    default { Write-Fail "Unsupported architecture: $Arch" }
}

Write-OK "Platform: windows-$Arch"
Write-Host ""

# ── Step 2: Check dependencies ────────────────────────────
Write-Info "Step 2/6: Checking dependencies..."

$MissingDeps = @()

function Check-Dep {
    param([string]$Name, [string]$CheckCmd)
    try {
        $null = Invoke-Expression $CheckCmd 2>$null
        Write-OK "$Name found"
    } catch {
        Write-Warn "$Name not found"
        $script:MissingDeps += $Name
    }
}

Check-Dep "git" "git --version"
Check-Dep "gcc" "gcc --version"
Check-Dep "cmake" "cmake --version"
Check-Dep "cargo" "cargo --version"
Check-Dep "python3" "python3 --version"

if ($MissingDeps.Count -gt 0) {
    Write-Warn "Missing dependencies: $($MissingDeps -join ', ')"
    Write-Info "Please install the missing dependencies manually:"
    Write-Info "  - Git:       https://git-scm.com/download/win"
    Write-Info "  - GCC:       https://winlibs.com or via MSYS2/MinGW"
    Write-Info "  - CMake:     https://cmake.org/download/"
    Write-Info "  - Rust:      https://rustup.rs"
    Write-Info "  - Python:    https://python.org/downloads/"
    Write-Info "Or use: winget install Git.Git CMake.CMake Rustlang.Rustup Python.Python.3"
}
Write-Host ""

# ── Step 3: Download source ───────────────────────────────
Write-Info "Step 3/6: Downloading AgentRT source..."

$SourceDir = Join-Path $InstallDir "AgentRT"

if (Test-Path $SourceDir) {
    Write-Info "Existing source found, running git pull..."
    Push-Location $SourceDir
    try {
        git pull --ff-only 2>$null
    } catch {
        Write-Warn "git pull failed, using existing code"
    }
    Pop-Location
} else {
    New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
    Push-Location $InstallDir
    try {
        git clone --depth 1 --branch "v$Version" "$RepoUrl.git" AgentRT 2>$null
    } catch {
        try {
            git clone --depth 1 "$RepoUrl.git" AgentRT 2>$null
        } catch {
            Write-Warn "git clone failed, please download manually from $RepoUrl"
        }
    }
    Pop-Location
}

Write-OK "Source ready: $SourceDir"
Write-Host ""

# ── Step 4: Build ─────────────────────────────────────────
Write-Info "Step 4/6: Building AgentRT..."

Push-Location $SourceDir

# Build CLI
if (Test-Path "sdk/cli/Cargo.toml") {
    Write-Info "Building CLI tool..."
    Push-Location sdk/cli
    try {
        cargo build --release 2>$null
    } catch {
        Write-Warn "CLI build failed"
    }
    Pop-Location
}

# Build TUI
if (Test-Path "sdk/tui/Cargo.toml") {
    Write-Info "Building TUI tool..."
    Push-Location sdk/tui
    try {
        cargo build --release 2>$null
    } catch {
        Write-Warn "TUI build failed"
    }
    Pop-Location
}

Pop-Location

Write-OK "Build complete"
Write-Host ""

# ── Step 5: Install ───────────────────────────────────────
Write-Info "Step 5/6: Installing to system..."

New-Item -ItemType Directory -Path $BinDir -Force | Out-Null

# Install CLI
$CliExe = Join-Path $SourceDir "sdk\cli\target\release\agentrt.exe"
if (Test-Path $CliExe) {
    Copy-Item $CliExe -Destination (Join-Path $BinDir "agentrt.exe") -Force
    Write-OK "CLI installed: $BinDir\agentrt.exe"
}

# Install TUI
$TuiExe = Join-Path $SourceDir "sdk\tui\target\release\agentrt-tui.exe"
if (Test-Path $TuiExe) {
    Copy-Item $TuiExe -Destination (Join-Path $BinDir "agentrt-tui.exe") -Force
    Write-OK "TUI installed: $BinDir\agentrt-tui.exe"
}

# Ensure BinDir is in PATH
$UserPath = [Environment]::GetEnvironmentVariable("PATH", "User")
if ($UserPath -notlike "*$BinDir*") {
    Write-Info "Adding $BinDir to user PATH..."
    [Environment]::SetEnvironmentVariable(
        "PATH",
        "$BinDir;$UserPath",
        "User"
    )
    $env:PATH = "$BinDir;$env:PATH"
    Write-Warn "Please restart your terminal or run 'refreshenv' to update PATH"
}

Write-Host ""

# ── Step 6: Verify ────────────────────────────────────────
Write-Info "Step 6/6: Verifying installation..."

try {
    $agentrtPath = Join-Path $BinDir "agentrt.exe"
    if (Test-Path $agentrtPath) {
        Write-OK "agentrt command available"
        & $agentrtPath --version 2>$null
    } else {
        Write-Warn "agentrt not found in PATH yet, please restart your terminal"
    }
} catch {
    Write-Warn "agentrt command not yet in PATH, please restart your terminal"
}

Write-Host ""
Write-Host "═══════════════════════════════════════════════" -ForegroundColor White
Write-Host "   Installation Complete!" -ForegroundColor Green
Write-Host "═══════════════════════════════════════════════" -ForegroundColor White
Write-Host ""
Write-Host "Quick Start:"
Write-Host ""
Write-Host "  1. Create your first Agent:"
Write-Host "     agentrt init my-first-agent" -ForegroundColor Cyan
Write-Host ""
Write-Host "  2. Run the Agent:"
Write-Host "     cd my-first-agent && agentrt run `"Hello, World!`"" -ForegroundColor Cyan
Write-Host ""
Write-Host "  3. View help:"
Write-Host "     agentrt --help" -ForegroundColor Cyan
Write-Host ""
Write-Host "More info:"
Write-Host "  Source: $RepoUrl" -ForegroundColor Cyan
Write-Host "  Version: v$Version" -ForegroundColor Cyan
Write-Host "  Install dir: $InstallDir" -ForegroundColor Cyan
Write-Host ""