# SPDX-FileCopyrightText: 2026 SPHARX Ltd.
# SPDX-License-Identifier: GPL-3.0-or-later
#
# AgentOS SDK One-Click Build Verification Script (Windows PowerShell)
# Runs: tsc --noEmit + cargo build + go build ./... + pytest
# Output: 4 status lines (PASS/FAIL) + error summary
# Exit code: 0 if all pass, 1 if any fail

$ErrorActionPreference = "Continue"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RootDir = Split-Path -Parent $ScriptDir

$TsDir = Join-Path $RootDir "agentos\toolkit\typescript"
$RustDir = Join-Path $RootDir "agentos\toolkit\rust"
$GoDir = Join-Path $RootDir "agentos\toolkit\go"
$PythonDir = Join-Path $RootDir "agentos\toolkit\python"

$script:Pass = 0
$script:Fail = 0
$script:Errors = ""

function Run-Check {
    param(
        [string]$Name,
        [string]$Command,
        [string]$Directory
    )

    Write-Host -NoNewline "[......] $Name"

    $prevDir = Get-Location
    try {
        Set-Location $Directory
        $output = & cmd /c "$Command 2>&1" 2>&1
        $exitCode = $LASTEXITCODE

        if ($exitCode -eq 0) {
            Write-Host "`r[ PASS ] $Name"
            $script:Pass += 1
        } else {
            Write-Host "`r[ FAIL ] $Name"
            $script:Fail += 1
            $script:Errors += "--- $Name FAILED ---`r`n"
            $script:Errors += ($output | Select-Object -First 20 | Out-String)
            $script:Errors += "`r`n"
        }
    } catch {
        Write-Host "`r[ FAIL ] $Name"
        $script:Fail += 1
        $script:Errors += "--- $Name FAILED ---`r`n"
        $script:Errors += $_.Exception.Message
        $script:Errors += "`r`n"
    } finally {
        Set-Location $prevDir
    }
}

Write-Host "========================================"
Write-Host "  AgentOS SDK Build Verification"
Write-Host "========================================"
Write-Host ""

Run-Check -Name "TypeScript" -Command "npx tsc --noEmit" -Directory $TsDir
Run-Check -Name "Rust" -Command "cargo build" -Directory $RustDir
Run-Check -Name "Go" -Command "go build ./..." -Directory $GoDir
Run-Check -Name "Python" -Command "python -m pytest tests/test_plugin_lifecycle.py tests/test_integration_e2e.py tests/test_cross_platform.py -q -o addopts=" -Directory $PythonDir

Write-Host ""
Write-Host "========================================"
Write-Host "  Results: $($script:Pass) PASS / $($script:Fail) FAIL"
Write-Host "========================================"

if ($script:Fail -gt 0) {
    Write-Host ""
    Write-Host "Error Summary:"
    Write-Host ""
    Write-Host $script:Errors
    exit 1
}

exit 0
