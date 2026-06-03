# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# AgentOS 安装脚本 (Windows PowerShell)
# 遵循 AgentOS 架构设计原则：反馈闭环、最小特权、安全内生

<#
.SYNOPSIS
    AgentOS Windows 安装脚本

.DESCRIPTION
    此脚本用于在 Windows 系统上安装 AgentOS，包含依赖检查、目录创建、
    文件安装、注册表配置等完整流程。

.PARAMETER InstallPath
    安装路径，默认为 C:\Program Files\AgentOS

.PARAMETER UserPath
    是否将安装路径添加到用户 PATH 环境变量

.PARAMETER SystemPath
    是否将安装路径添加到系统 PATH 环境变量（需要管理员权限）

.PARAMETER SkipDeps
    跳过依赖检查

.PARAMETER Force
    强制安装，覆盖已有文件

.PARAMETER Uninstall
    卸载 AgentOS

.EXAMPLE
    .\install.ps1

.EXAMPLE
    .\install.ps1 -InstallPath "D:\AgentOS" -SystemPath

.EXAMPLE
    .\install.ps1 -Uninstall
#>

[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [string]$InstallPath = "$env:ProgramFiles\AgentOS",

    [switch]$UserPath,

    [switch]$SystemPath,

    [switch]$SkipDeps,

    [switch]$Force,

    [switch]$Uninstall,

    [switch]$NoVerify
)

###############################################################################
# 版本信息
###############################################################################
$AGENTOS_VERSION = "1.0.0.6"
$AGENTOS_INSTALLER_VERSION = "1.0.0"

###############################################################################
# 全局变量
###############################################################################
$Script:ErrorCount = 0
$Script:WarningCount = 0
$Script:InstallStartTime = Get-Date

###############################################################################
# 颜色定义
###############################################################################
$COLOR_RESET = "`e[0m"
$COLOR_BOLD = "`e[1m"
$COLOR_DIM = "`e[2m"
$COLOR_RED = "`e[0;31m"
$COLOR_GREEN = "`e[0;32m"
$COLOR_YELLOW = "`e[1;33m"
$COLOR_BLUE = "`e[0;34m"
$COLOR_CYAN = "`e[0;36m"

###############################################################################
# 打印函数
###############################################################################
function Write-Banner {
    $host.UI.RawUI.WindowTitle = "AgentOS Installer"

    Write-Host ""
    Write-Host "${COLOR_CYAN}   ____          _        __  __                                                   _ ${COLOR_RESET}" -NoNewline
    Write-Host ""
    Write-Host "${COLOR_CYAN}  / __ \        | |      |  \/  |                                                 | | ${COLOR_RESET}"
    Write-Host "${COLOR_CYAN} | |  | |_ __   | | __ _| \  / | __ _ _ __   __ _  ___ _ __ ___   ___  _ __  __ _| |_${COLOR_RESET}"
    Write-Host "${COLOR_CYAN} | |  | | '_ \ / _` |/ _` | |\/| |/ _` | '_ \ / _` |/ _ \ '_ ` _ \ / _ \| '_ \/ _` | __|${COLOR_RESET}"
    Write-Host "${COLOR_CYAN} | |__| | |_) | (_| | (_| | |  | | (_| | | | | (_| |  __/ | | | | | (_) | | | | (_| | |_${COLOR_RESET}"
    Write-Host "${COLOR_CYAN}  \____/| .__/ \__,_|\__,_|_|  |_|\__,_|_| |_|\__, |\___|_| |_| |_|\___/|_| |_|\__,_|\__|${COLOR_RESET}"
    Write-Host "${COLOR_CYAN}        | |                                     __/ | ${COLOR_RESET}"
    Write-Host "${COLOR_CYAN}        |_|                                    |___/ ${COLOR_RESET}"
    Write-Host ""
    Write-Host "${COLOR_BOLD}AgentOS Installer v${AGENTOS_INSTALLER_VERSION}${COLOR_RESET}"
    Write-Host ""
}

function Write-Section {
    param([string]$Title)
    Write-Host ""
    Write-Host "${COLOR_BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${COLOR_RESET}"
    Write-Host "${COLOR_BLUE}▶ ${Title}${COLOR_RESET}"
    Write-Host "${COLOR_BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${COLOR_RESET}"
    Write-Host ""
}

function Write-Status {
    param(
        [ValidateSet("OK", "FAIL", "INFO", "WARN", "SKIP")]
        [string]$Status,
        [string]$Message
    )

    switch ($Status) {
        "OK"   { Write-Host "${COLOR_GREEN}[✓]${COLOR_RESET} $Message" }
        "FAIL" { Write-Host "${COLOR_RED}[✗]${COLOR_RESET} $Message"; $Script:ErrorCount++ }
        "INFO" { Write-Host "${COLOR_BLUE}[•]${COLOR_RESET} $Message" }
        "WARN" { Write-Host "${COLOR_YELLOW}[!]${COLOR_RESET} $Message"; $Script:WarningCount++ }
        "SKIP" { Write-Host "${COLOR_DIM}[-]${COLOR_RESET} $Message" }
    }
}

###############################################################################
# 依赖检查函数
###############################################################################
function Test-PowerShellVersion {
    if ($PSVersionTable.PSVersion.Major -lt 5) {
        Write-Status "FAIL" "需要 PowerShell 5.0 或更高版本，当前版本: $($PSVersionTable.PSVersion)"
        return $false
    }
    Write-Status "INFO" "PowerShell 版本: $($PSVersionTable.PSVersion)"
    return $true
}

function Test-AdminRights {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Test-RequiredCommands {
    param([string[]]$Commands)

    $missing = @()
    foreach ($cmd in $Commands) {
        if (-not (Get-Command $cmd -ErrorAction SilentlyContinue)) {
            $missing += $cmd
        }
    }

    if ($missing.Count -gt 0) {
        Write-Status "FAIL" "缺少必需命令: $($missing -join ', ')"
        return $false
    }

    return $true
}

function Test-DotNetRuntime {
    $dotnet = Get-Command dotnet -ErrorAction SilentlyContinue
    if ($dotnet) {
        $version = dotnet --version 2>$null
        Write-Status "INFO" ".NET Runtime 版本: $version"
        return $true
    }
    Write-Status "WARN" "未检测到 .NET Runtime，部分功能可能不可用"
    return $true
}

function Test-Git {
    $git = Get-Command git -ErrorAction SilentlyContinue
    if ($git) {
        $version = git --version 2>$null
        Write-Status "INFO" "Git 版本: $version"
        return $true
    }
    Write-Status "WARN" "未检测到 Git"
    return $true
}

function Test-VisualStudio {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $instances = & $vswhere -latest -property installationPath 2>$null
        if ($instances) {
            Write-Status "INFO" "Visual Studio 安装路径: $instances"
            return $true
        }
    }
    Write-Status "INFO" "未检测到 Visual Studio"
    return $true
}

###############################################################################
# 安装步骤函数
###############################################################################
function New-DirectoryStructure {
    param([string]$BasePath)

    Write-Section "创建目录结构"

    $dirs = @(
        $BasePath,
        "$BasePath\bin",
        "$BasePath\lib",
        "$BasePath\lib\agentos",
        "$BasePath\lib\backends",
        "$BasePath\lib\modules",
        "$BasePath\include",
        "$BasePath\include\agentos",
        "$BasePath\config",
        "$BasePath\log",
        "$BasePath\run",
        "$BasePath\scripts"
    )

    foreach ($dir in $dirs) {
        if (Test-Path $dir) {
            Write-Status "SKIP" "目录已存在: $dir"
        }
        else {
            if ($PSCmdlet.ShouldProcess($dir, "创建目录")) {
                New-Item -ItemType Directory -Path $dir -Force | Out-Null
                Write-Status "OK" "创建目录: $dir"
            }
            else {
                Write-Status "INFO" "[WHATIF] 创建目录: $dir"
            }
        }
    }
}

function Copy-Binaries {
    param(
        [string]$SourcePath,
        [string]$DestPath
    )

    Write-Section "安装二进制文件"

    $binaries = @(
        "agentos.exe",
        "agentosd.exe",
        "agentos-cli.exe"
    )

    $sourceBin = Join-Path $SourcePath "bin"
    if (-not (Test-Path $sourceBin)) {
        Write-Status "WARN" "构建目录不存在，跳过二进制文件安装"
        return
    }

    foreach ($binary in $binaries) {
        $srcFile = Join-Path $sourceBin $binary
        if (Test-Path $srcFile) {
            $destFile = Join-Path $DestPath "bin\$binary"
            if ($PSCmdlet.ShouldProcess($destFile, "复制二进制文件")) {
                Copy-Item -Path $srcFile -Destination $destFile -Force
                Write-Status "OK" "安装: $binary"
            }
        }
    }
}

function Copy-Libraries {
    param(
        [string]$SourcePath,
        [string]$DestPath
    )

    Write-Section "安装库文件"

    $libs = @(
        "agentos_corekern.dll",
        "agentos_coreloopthree.dll",
        "agentos_memoryrovol.dll",
        "agentos_syscall.dll"
    )

    $sourceLib = Join-Path $SourcePath "lib"
    if (-not (Test-Path $sourceLib)) {
        Write-Status "WARN" "构建目录不存在，跳过库文件安装"
        return
    }

    foreach ($lib in $libs) {
        $srcFile = Get-ChildItem -Path $sourceLib -Filter "*$lib*" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($srcFile) {
            $destFile = Join-Path $DestPath "lib\$($srcFile.Name)"
            if ($PSCmdlet.ShouldProcess($destFile, "复制库文件")) {
                Copy-Item -Path $srcFile.FullName -Destination $destFile -Force
                Write-Status "OK" "安装: $($srcFile.Name)"
            }
        }
    }
}

function Copy-Headers {
    param(
        [string]$SourcePath,
        [string]$DestPath
    )

    Write-Section "安装头文件"

    $sourceInclude = Join-Path $SourcePath "atoms"
    if (-not (Test-Path $sourceInclude)) {
        Write-Status "SKIP" "未找到头文件目录"
        return
    }

    $headers = Get-ChildItem -Path $sourceInclude -Filter "*.h" -Recurse -ErrorAction SilentlyContinue
    foreach ($header in $headers) {
        $relativePath = $header.FullName.Substring($sourceInclude.Length)
        $destFile = Join-Path $DestPath "include\agentos$relativePath"
        $destDir = Split-Path $destFile -Parent

        if ($PSCmdlet.ShouldProcess($destFile, "复制头文件")) {
            if (-not (Test-Path $destDir)) {
                New-Item -ItemType Directory -Path $destDir -Force | Out-Null
            }
            Copy-Item -Path $header.FullName -Destination $destFile -Force
            Write-Status "OK" "安装: $relativePath"
        }
    }
}

function Copy-ConfigFiles {
    param([string]$DestPath)

    Write-Section "安装配置文件"

    $configFiles = @(
        "agentos.conf",
        "logging.conf",
        "memory.conf"
    )

    $sourceConfig = Join-Path $PSScriptRoot "..\..\config"
    if (-not (Test-Path $sourceConfig)) {
        Write-Status "SKIP" "未找到配置文件目录"
        return
    }

    foreach ($config in $configFiles) {
        $srcFile = Join-Path $sourceConfig $config
        if (Test-Path $srcFile) {
            $destFile = Join-Path $DestPath "config\$config"
            if ((Test-Path $destFile) -and (-not $Force)) {
                Write-Status "SKIP" "配置文件已存在: $config"
            }
            elseif ($PSCmdlet.ShouldProcess($destFile, "复制配置文件")) {
                Copy-Item -Path $srcFile -Destination $destFile -Force
                Write-Status "OK" "安装: $config"
            }
        }
    }
}

function Copy-Scripts {
    param([string]$DestPath)

    Write-Section "安装脚本工具"

    $scripts = @(
        "build\build.sh",
        "ops\benchmark.py",
        "ops\doctor.py",
        "ops\validate_contracts.py",
        "dev\generate_docs.py",
        "dev\update_registry.py",
        "init\init_config.py"
    )

    $scriptsDest = Join-Path $DestPath "scripts"

    foreach ($script in $scripts) {
        $srcFile = Join-Path $PSScriptRoot "..\$script"
        if (Test-Path $srcFile) {
            $destFile = Join-Path $scriptsDest (Split-Path $script -Leaf)
            if ($PSCmdlet.ShouldProcess($destFile, "复制脚本")) {
                if (-not (Test-Path $scriptsDest)) {
                    New-Item -ItemType Directory -Path $scriptsDest -Force | Out-Null
                }
                Copy-Item -Path $srcFile -Destination $destFile -Force
                Write-Status "OK" "安装: $(Split-Path $script -Leaf)"
            }
        }
    }
}

function Add-ToPath {
    param([string]$PathToAdd)

    if ($UserPath) {
        $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
        if ($userPath -notlike "*$PathToAdd*") {
            if ($PSCmdlet.ShouldProcess("用户 PATH", "添加安装路径")) {
                [Environment]::SetEnvironmentVariable("Path", "$userPath;$PathToAdd", "User")
                $env:Path = "$userPath;$PathToAdd"
                Write-Status "OK" "已添加到用户 PATH"
            }
        }
        else {
            Write-Status "SKIP" "用户 PATH 已包含安装路径"
        }
    }

    if ($SystemPath) {
        if (-not (Test-AdminRights)) {
            Write-Status "WARN" "需要管理员权限添加到系统 PATH"
            return
        }

        $systemPath = [Environment]::GetEnvironmentVariable("Path", "Machine")
        if ($systemPath -notlike "*$PathToAdd*") {
            if ($PSCmdlet.ShouldProcess("系统 PATH", "添加安装路径")) {
                [Environment]::SetEnvironmentVariable("Path", "$systemPath;$PathToAdd", "Machine")
                Write-Status "OK" "已添加到系统 PATH"
            }
        }
        else {
            Write-Status "SKIP" "系统 PATH 已包含安装路径"
        }
    }
}

function New-VersionMarker {
    param([string]$InstallPath)

    Write-Section "创建版本标记"

    $markerPath = Join-Path $InstallPath "lib\agentos\agentos.version"

    if ($PSCmdlet.ShouldProcess($markerPath, "创建版本文件")) {
        @"
$AGENTOS_VERSION
Install date: $(Get-Date -Format "yyyy-MM-ddTHH:mm:sszzz")
Install path: $InstallPath
"@ | Set-Content -Path $markerPath -Force

        Write-Status "OK" "版本标记已创建"
    }
}

function Test-Installation {
    param([string]$InstallPath)

    if ($NoVerify) {
        return $true
    }

    Write-Section "验证安装"

    $errors = 0

    if (-not (Test-Path (Join-Path $InstallPath "lib\agentos"))) {
        Write-Status "FAIL" "库目录不存在"
        $errors++
    }

    if (-not (Test-Path (Join-Path $InstallPath "config"))) {
        Write-Status "FAIL" "配置目录不存在"
        $errors++
    }

    if ($errors -eq 0) {
        Write-Status "OK" "安装验证通过"
        return $true
    }
    else {
        Write-Status "FAIL" "安装验证失败 ($errors 个错误)"
        return $false
    }
}

###############################################################################
# 卸载函数
###############################################################################
function Remove-AgentOS {
    Write-Section "卸载 AgentOS"

    if (-not (Test-Path $InstallPath)) {
        Write-Status "INFO" "AgentOS 未安装"
        return
    }

    $confirm = Read-Host "确定要卸载 AgentOS 吗? (y/N)"
    if ($confirm -ne "y" -and $confirm -ne "Y") {
        Write-Status "INFO" "取消卸载"
        return
    }

    Write-Status "INFO" "移除安装目录..."
    if ($PSCmdlet.ShouldProcess($InstallPath, "删除目录")) {
        Remove-Item -Path $InstallPath -Recurse -Force -ErrorAction SilentlyContinue
    }

    if ($UserPath) {
        $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
        if ($userPath -like "*$InstallPath*") {
            $newPath = ($userPath -split ';' | Where-Object { $_ -notlike "*AgentOS*" }) -join ';'
            [Environment]::SetEnvironmentVariable("Path", $newPath, "User")
            Write-Status "OK" "已从用户 PATH 移除"
        }
    }

    if ($SystemPath) {
        if (Test-AdminRights) {
            $systemPath = [Environment]::GetEnvironmentVariable("Path", "Machine")
            if ($systemPath -like "*$InstallPath*") {
                $newPath = ($systemPath -split ';' | Where-Object { $_ -notlike "*AgentOS*" }) -join ';'
                [Environment]::SetEnvironmentVariable("Path", $newPath, "Machine")
                Write-Status "OK" "已从系统 PATH 移除"
            }
        }
    }

    Write-Status "OK" "卸载完成"
}

###############################################################################
# 主流程
###############################################################################
function Main {
    $host.UI.RawUI.WindowTitle = "AgentOS Installer v${AGENTOS_INSTALLER_VERSION}"

    Write-Banner

    if ($Uninstall) {
        Remove-AgentOS
        return
    }

    Write-Section "前置检查"

    if (-not (Test-PowerShellVersion)) {
        exit 1
    }

    if (-not $SkipDeps) {
        Write-Status "INFO" "检查系统环境..."
        Test-DotNetRuntime | Out-Null
        Test-Git | Out-Null
        Test-VisualStudio | Out-Null
    }

    Write-Host ""

    if ($SystemPath -and -not (Test-AdminRights)) {
        Write-Status "WARN" "添加系统 PATH 需要管理员权限"
        Write-Host "请使用 '以管理员身份运行' PowerShell 后重试"
        exit 1
    }

    $buildPath = Join-Path $PSScriptRoot "..\..\build"

    New-DirectoryStructure -BasePath $InstallPath

    if (Test-Path $buildPath) {
        Copy-Binaries -SourcePath $buildPath -DestPath $InstallPath
        Copy-Libraries -SourcePath $buildPath -DestPath $InstallPath
    }

    $sourcePath = Join-Path $PSScriptRoot "..\.."
    Copy-Headers -SourcePath $sourcePath -DestPath $InstallPath
    Copy-ConfigFiles -DestPath $InstallPath
    Copy-Scripts -DestPath $InstallPath

    if ($UserPath -or $SystemPath) {
        $binPath = Join-Path $InstallPath "bin"
        Add-ToPath -PathToAdd $binPath
    }

    New-VersionMarker -InstallPath $InstallPath

    $installSuccess = Test-Installation -InstallPath $InstallPath

    Write-Section "安装完成"

    $duration = (Get-Date) - $Script:InstallStartTime

    Write-Host "${COLOR_GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${COLOR_RESET}"
    Write-Host "${COLOR_BOLD}  安装成功!${COLOR_RESET}"
    Write-Host "${COLOR_DIM}  版本: ${AGENTOS_VERSION}${COLOR_RESET}"
    Write-Host "${COLOR_DIM}  安装路径: ${InstallPath}${COLOR_RESET}"
    Write-Host "${COLOR_DIM}  耗时: $($duration.TotalSeconds) 秒${COLOR_RESET}"
    Write-Host "${COLOR_GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${COLOR_RESET}"
    Write-Host ""

    if ($Script:WarningCount -gt 0) {
        Write-Host "${COLOR_YELLOW}警告: $($Script:WarningCount) 个${COLOR_RESET}"
    }

    if ($Script:ErrorCount -gt 0) {
        Write-Host "${COLOR_RED}错误: $($Script:ErrorCount) 个${COLOR_RESET}"
        exit 1
    }
}

###############################################################################
# 执行入口
###############################################################################
Main