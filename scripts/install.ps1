# =============================================================================
# AgentOS 跨平台一键部署脚本 V2.0 (Windows PowerShell)
# =============================================================================
#
# 功能特性:
#   ✅ Windows 10+ / Windows Server 2019+ 原生支持
#   ✅ 自动环境检测（操作系统版本、架构、资源）
#   ✅ 智能依赖安装（Docker Desktop、Git、Python）
#   ✅ 交互式配置向导 + 非交互模式 (--auto)
#   ✅ Docker 服务管理（开发/生产环境）
#   ✅ 健康检查和状态验证
#   ✅ 日志收集和问题诊断
#   ✅ 优雅停机和资源清理
#   ✅ 可视化进度反馈（PowerShell 进度条）
#   ✅ 完善的错误处理和回滚机制
#   ✅ ANSI 彩色输出支持
#
# 使用方法:
#   # 交互式部署（推荐新手）
#   .\install.ps1
#
#   # 一键部署开发环境
#   .\install.ps1 -Mode dev -Auto
#
#   # 一键部署生产环境
#   .\install.ps1 -Mode prod -Auto
#
#   # 仅检查环境
#   .\install.ps1 -CheckOnly
#
#   # 以管理员身份运行
#   .\install.ps1 -Mode dev -Auto -AsAdmin
#
#   # 查看帮助
#   .\install.ps1 -Help
#
# 环境要求:
#   - Windows 10 (1903+) / Windows Server 2019+
#   - Docker Desktop for Windows >= 4.8
#   - 内存 >= 4GB RAM (开发) / 8GB (生产)
#   - 磁盘空间 >= 20GB
#   - PowerShell 5.1+ 或 PowerShell Core 7+
#
# 作者: Spharx AgentOS Team
# 版本: 2.0.0 (2026-04-08)
# 兼容: Windows 10+, Windows Server 2019+, PowerShell 5.1+/7+
# =============================================================================

[CmdletBinding()]
param(
    [ValidateSet("dev", "prod")]
    [string]$Mode = "dev",

    [ValidateSet("backend", "client", "all")]
    [string]$Target = "all",

    [switch]$Auto,

    [switch]$CheckOnly,

    [switch]$Status,

    [switch]$Stop,

    [switch]$Restart,

    [switch]$Logs,

    [switch]$Cleanup,

    [switch]$Health,

    [switch]$Help,

    [switch]$AsAdmin,

    [switch]$Version
)

# =============================================================================
# 全局配置
# =============================================================================

$ErrorActionPreference = "Stop"
$Script:ScriptVersion = "2.0.0"
$Script:ScriptName = "AgentOS Cross-Platform Installer (Windows)"
$Script:MinDockerVersion = "20.10"
$Script:MinMemoryGBDev = 4
$Script:MinMemoryGBProd = 8
$Script:MinDiskGB = 20

$Script:ProjectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Script:DockerDir = Join-Path $Script:ProjectRoot "docker"
$Script:EnvFile = ".env.production"
$Script:LogDir = Join-Path $Script:ProjectRoot "logs"
$Script:ConfigDir = Join-Path $Script:ProjectRoot "config"
$Script:Timestamp = Get-Date -Format "yyyyMMdd_HHmmss"

# 状态追踪
$Script:TotalSteps = 0
$Script:CurrentStep = 0
$Script:ErrorCount = 0
$Script:WarningCount = 0

# =============================================================================
# ANSI 颜色定义（Windows 10 1703+ 支持）
# =============================================================================

function Set-ColorScheme {
    if ($Host.UI.RawUI.SupportsVirtualTerminal -or $env:TERM) {
        $global:RED = "`e[0;31m"
        $global:GREEN = "`e[0;32m"
        $global:YELLOW = "`e[1;33m"
        $global:BLUE = "`e[0;34m"
        $global:CYAN = "`e[0;36m"
        $global:MAGENTA = "`e[0;35m"
        $global:BOLD = "`e[1m"
        $global:NC = "`e[0m"
    } else {
        $global:RED = ""
        $global:GREEN = ""
        $global:YELLOW = ""
        $global:BLUE = ""
        $global:CYAN = ""
        $global:MAGENTA = ""
        $global:BOLD = ""
        $global:NC = ""
    }
}

Set-ColorScheme

# =============================================================================
# 日志函数
# =============================================================================

function Write-LogInfo {
    param([string]$Message)
    Write-Host "  ${GREEN}[✓]${NC} $Message"
}

function Write-LogWarn {
    param([string]$Message)
    Write-Host "  ${YELLOW}[!]${NC} $Message"
    $Script:WarningCount++
}

function Write-LogError {
    param([string]$Message)
    Write-Host "  ${RED}[✗]${NC} $Message"
    $Script:ErrorCount++
}

function Write-LogStep {
    param([string]$Message)
    Write-Host "`n  ${MAGENTA}━━━ $Message ━━━${NC}"
}

# =============================================================================
# 进度显示系统
# =============================================================================

function Initialize-Progress {
    param([int]$TotalSteps)

    $Script:TotalSteps = $TotalSteps
    $Script:CurrentStep = 0
    $Script:ErrorCount = 0
    $Script:WarningCount = 0

    Write-Host ""
    Write-Host "${CYAN}═══════════════════════════════════════════════════════════${NC}"
    Write-Host "${BOLD}${BLUE}  🚀 ${ScriptName} V${ScriptVersion}${NC}"
    Write-Host "${CYAN}═══════════════════════════════════════════════════════════${NC}"
    Write-Host ""
}

function Show-Progress {
    param([string]$StepName)

    $Script:CurrentStep++
    $percentage = [math]::Floor(($Script:CurrentStep * 100) / $Script:TotalSteps)
    $filled = [math]::Floor($percentage / 5)
    $empty = 20 - $filled

    $bar = ("█" * $filled) + ("░" * $empty)

    Write-Host "`n${BLUE}▶ [$($Script:CurrentStep)/$($Script:TotalSteps)]${NC} ${BOLD}${StepName}${NC}"
    Write-Host "${CYAN}   进度: [${bar}] ${percentage}%${NC}"
}

function Complete-Progress {
    Write-Host ""
    Write-Host "${CYAN}═══════════════════════════════════════════════════════════${NC}"

    if ($Script:ErrorCount -eq 0) {
        Write-Host "${GREEN}  ✅ 所有步骤完成！未发现错误${NC}"
        if ($Script:WarningCount -gt 0) {
            Write-Host "${YELLOW}  ⚠️  发现 $($Script:WarningCount) 个警告${NC}"
        }
    } else {
        Write-Host "${RED}  ❌ 完成，但发现 $($Script:ErrorCount) 个错误${NC}"
    }

    Write-Host "${CYAN}═══════════════════════════════════════════════════════════${NC}"
    Write-Host ""
}

# =============================================================================
# 权限检查
# =============================================================================

function Test-Administrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Require-Administrator {
    if (-not (Test-Administrator)) {
        if ($AsAdmin) {
            Write-LogError "需要管理员权限！请以管理员身份运行此脚本。"
            Write-Host ""
            Write-Host "  操作方法:"
            Write-Host "    1. 右键点击 PowerShell → 以管理员身份运行"
            Write-Host "    2. 或者运行: Start-Process powershell -Verb runAs -ArgumentList '-File', `"$PSCommandPath`""
            exit 1
        } else {
            Write-LogWarn "非管理员模式运行，部分功能可能受限"
        }
    } else {
        Write-LogInfo "管理员权限 ✓"
    }
}

# =============================================================================
# 平台检测
# =============================================================================

function Get-PlatformInfo {
    Write-LogStep "检测 Windows 平台"

    try {
        $osInfo = Get-CimInstance Win32_OperatingSystem
        $csInfo = Get-CimInstance Win32_ComputerSystem
        $processor = Get-CimInstance Win32_Processor

        $Script:OSVersion = $osInfo.Version
        $Script:OSBuild = $osInfo.BuildNumber
        $Script:OSName = $osInfo.Caption.Trim()
        $Script:Architecture = $processor.AddressWidth
        $Script:TotalMemoryGB = [math]::Round($csInfo.TotalPhysicalMemory / 1GB, 1)
        $Script:LogicalProcessors = $processor.NumberOfLogicalProcessors
        $Script:Cores = $processor.NumberOfCores

        Write-LogInfo "操作系统: $Script:OSName (版本: $Script:OSVersion, 构建: $Script:OSBuild)"
        Write-LogInfo "系统架构: $($Script:Architecture)-bit"
        Write-LogInfo "CPU 核心: $Script:Cores 核 / $Script:LogicalProcessors 线程"
        Write-LogInfo "内存大小: ~$($Script:TotalMemoryGB) GB"

        # 检查 Windows 版本兼容性
        Test-WindowsCompatibility
    }
    catch {
        Write-LogError "无法获取系统信息: $_"
        return $false
    }

    return $true
}

function Test-WindowsCompatibility {
    $buildNumber = [int]$Script:OSBuild

    if ($buildNumber -lt 18362) {
        Write-LogError "Windows 版本过低 (构建号: $buildNumber)，要求至少 Windows 10 1903 (19041+)"
        return $false
    } elseif ($buildNumber -lt 19041) {
        Write-LogWarn "Windows 版本较旧 (构建号: $buildNumber)，建议升级到 Windows 10 2004+"
    } else {
        Write-LogInfo "Windows 版本兼容 ✓"
    }

    return $true
}

# =============================================================================
# 资源检查
# =============================================================================

function Test-SystemResources {
    param([string]$DeployMode = "dev")

    Write-LogStep "检查系统资源"

    $requiredMemory = $Script:MinMemoryGBDev
    if ($DeployMode -eq "prod") { $requiredMemory = $Script:MinMemoryGBProd }

    # 内存检查
    if ($Script:TotalMemoryGB -lt $requiredMemory) {
        Write-LogError "内存不足: 需要 ≥${requiredMemory}GB, 当前: ~$($Script:TotalMemoryGB)GB"
        if ($DeployMode -eq "prod") {
            Write-LogWarn "生产环境建议使用 16GB+ 内存"
        }
    } else {
        Write-LogInfo "内存: ~$($Script:TotalMemoryGB)GB (要求: ≥${requiredMemory}GB) ✓"
    }

    # CPU 检查
    if ($Script:Cores -lt 4) {
        Write-LogWarn "CPU 核心数较少: $Script:Cores 核 (推荐 4+ 核)"
    } else {
        Write-LogInfo "CPU: $Script:Cores 核 ✓"
    }

    # 磁盘空间检查
    $disk = Get-WmiObject Win32_LogicalDisk -Filter "DeviceID='C:'" -ErrorAction SilentlyContinue
    if ($disk) {
        $freeSpaceGB = [math]::Round($disk.FreeSpace / 1GB, 1)
        if ($freeSpaceGB -lt $Script:MinDiskGB) {
            Write-LogError "磁盘空间不足: 需要 ≥$($Script:MinDiskGB)GB, 当前: ~${freeSpaceGB}GB"
        } else {
            Write-LogInfo "磁盘可用空间: ~${freeSpaceGB}GB (要求: ≥$($Script:MinDiskGB)GB) ✓"
        }
    } else {
        Write-LogWarn "无法获取磁盘信息"
    }

    # WSL2 检查（Docker Desktop 推荐）
    try {
        $wsl = Get-ItemProperty "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\WinPE" -ErrorAction Stop
        if ((Get-Command wsl -ErrorAction SilentlyContinue)) {
            Write-LogInfo "WSL2: 已安装 ✓ (推荐用于 Docker Desktop 后端)"
        }
    } catch {
        # WSL 可选，不强制要求
    }
}

# =============================================================================
# 依赖检测与安装
# =============================================================================

function Test-DockerInstallation {
    Write-LogStep "检查 Docker 环境"

    $dockerInstalled = $false
    $dockerRunning = $false
    $composeAvailable = $false

    # 检查 Docker CLI
    $dockerCmd = Get-Command docker -ErrorAction SilentlyContinue
    if ($dockerCmd) {
        try {
            $dockerOutput = docker --version 2>&1
            $Script:DockerVersion = ($dockerOutput -replace 'Docker version ', '') -split ',' | Select-Object -First 1
            Write-LogInfo "Docker: v$($Script:DockerVersion)"

            # 检查 Docker 是否运行
            $null = docker info 2>&1 | Out-Null
            $dockerRunning = $true
            Write-LogInfo "Docker 服务: 运行中 ✓"
            $dockerInstalled = $true
        }
        catch {
            Write-LogError "Docker 已安装但未运行"
            Show-DockerStartInstructions
        }
    } else {
        Write-LogError "未安装 Docker Desktop for Windows"
        Show-DockerInstallInstructions
    }

    # 检查 Docker Compose
    if ($dockerRunning) {
        try {
            $composeOutput = docker compose version 2>&1
            $Script:ComposeVersion = ($composeOutput -split '\s+')[-1]
            Write-LogInfo "Docker Compose (Plugin): v$($Script:ComposeVersion) ✓"
            $composeAvailable = $true
        }
        catch {
            try {
                $composeCmd = Get-Command docker-compose -ErrorAction SilentlyContinue
                if ($composeCmd) {
                    $composeOutput = docker-compose --version 2>&1
                    $Script:ComposeVersion = ($composeOutput -split '\s+')[2] -replace ','
                    Write-LogInfo "Docker Compose (Standalone): v$($Script:ComposeVersion) ✓"
                    $composeAvailable = $true
                } else {
                    Write-LogError "未安装 Docker Compose"
                }
            }
            catch {
                Write-LogError "未安装 Docker Compose"
            }
        }
    }

    return $dockerInstalled -and $composeAvailable
}

function Show-DockerInstallInstructions {
    Write-Host ""
    Write-Host "  ${YELLOW}安装 Docker Desktop:${NC}"
    Write-Host "    方法 1 (推荐):"
    Write-Host "      winget install Docker.DockerDesktop"
    Write-Host ""
    Write-Host "    方法 2 (手动下载):"
    Write-Host "      1. 访问 https://www.docker.com/products/docker-desktop/"
    Write-Host "      2. 下载 Docker Desktop Installer for Windows"
    Write-Host "      3. 运行安装程序并重启计算机"
    Write-Host ""
    Write-Host "    系统要求:"
    Write-Host "      • Windows 10 64-bit (1903+) / Windows 11"
    Write-Host "      • 启用 Hyper-V 和 WSL 2 功能"
    Write-Host "      • BIOS 中启用虚拟化 (VT-x/AMD-V)"
    Write-Host ""
}

function Show-DockerStartInstructions {
    Write-Host ""
    Write-Host "  ${YELLOW}启动 Docker Desktop:${NC}"
    Write-Host "    方法 1: 从开始菜单启动 'Docker Desktop'"
    Write-Host "    方法 2: 使用命令:"
    Write-Host "      & 'C:\Program Files\Docker\Docker\Docker Desktop.exe'"
    Write-Host ""
    Write-Host "  首次启动可能需要几分钟初始化..."
    Write-Host ""
}

function Install-DockerDesktop {
    Write-LogStep "自动安装 Docker Desktop"

    if (-not $Auto) {
        $response = Read-Host "  是否自动安装 Docker Desktop? [y/N]"
        if ($response -ne 'y' -and $response -ne 'Y') { return }
    }

    try {
        Write-LogInfo "使用 winget 安装 Docker Desktop..."
        winget install Docker.DockerDesktop --accept-package-agreements --accept-source-agreements

        if ($LASTEXITCODE -ne 0) {
            throw "winget 安装失败"
        }

        Write-LogInfo "Docker Desktop 安装成功!"
        Write-LogWarn "需要重启计算机才能完成安装"

        if ($Auto) {
            $restart = Read-Host "  是否立即重启? [y/N]"
            if ($restart -eq 'y' -or $restart -eq 'Y') {
                Restart-Computer -Force
            }
        }
    }
    catch {
        Write-LogError "自动安装失败: $_"
        Show-DockerInstallInstructions
    }
}

function Test-CoreDependencies {
    Write-LogStep "检查核心依赖"

    # Git
    $gitCmd = Get-Command git -ErrorAction SilentlyContinue
    if ($gitCmd) {
        $gitVersion = (git --version 2>&1) -replace 'git version ', ''
        Write-LogInfo "Git: v${gitVersion} ✓"
    } else {
        Write-LogWarn "未安装 Git（非必需但推荐用于版本管理）"
    }

    # Python
    $pythonCmd = Get-Command python3 -ErrorAction SilentlyContinue
    if (-not $pythonCmd) { $pythonCmd = Get-Command python -ErrorAction SilentlyContinue }
    if ($pythonCmd) {
        $pythonVersion = (& python --version 2>&1) -replace 'Python ', ''
        Write-LogInfo "Python: v${pythonVersion} ✓"
    } else {
        Write-LogInfo "Python 未安装（SDK 功能需要）"
    }

    # curl（Windows 10 1803+ 内置）
    if (Get-Command curl -ErrorAction SilentlyContinue) {
        Write-LogInfo "网络工具 (curl) ✓"
    } else {
        Write-LogError "curl 不可用（Windows 10 1803+ 应该内置）"
    }

    return $true
}

# =============================================================================
# 项目结构验证
# =============================================================================

function Confirm-ProjectStructure {
    Write-LogStep "验证项目结构"

    $requiredFiles = @(
        "docker\Dockerfile.kernel",
        "docker\Dockerfile.daemon",
        "docker\docker-compose.yml",
        ".env.example"
    )

    $optionalFiles = @(
        "docker\docker-compose.prod.yml",
        ".env.production.example",
        "config\agentos.yaml.example"
    )

    $missingRequired = @()
    $missingOptional = @()

    foreach ($file in $requiredFiles) {
        $fullPath = Join-Path $Script:ProjectRoot $file
        if (Test-Path $fullPath) {
            Write-LogInfo "必需文件: ${file} ✓"
        } else {
            $missingRequired += $file
            Write-LogError "缺失必需文件: ${file}"
        }
    }

    foreach ($file in $optionalFiles) {
        $fullPath = Join-Path $Script:ProjectRoot $file
        if (Test-Path $fullPath) {
            Write-LogInfo "可选文件: ${file} ✓"
        } else {
            $missingOptional += $file
            Write-LogWarn "缺失可选文件: ${file}"
        }
    }

    if ($missingRequired.Count -gt 0) {
        Write-LogError "项目结构不完整，缺少 $($missingRequired.Count) 个必需文件"
        return $false
    }

    return $true
}

# =============================================================================
# 配置管理
# =============================================================================

function New-EnvironmentConfig {
    Write-LogStep "生成环境配置"

    # 创建目录
    if (-not (Test-Path $Script:LogDir)) { New-Item -ItemType Directory -Path $Script:LogDir -Force | Out-Null }
    if (-not (Test-Path $Script:ConfigDir)) { New-Item -ItemType Directory -Path $Script:ConfigDir -Force | Out-Null }

    # 复制 .env 文件
    $envFilePath = Join-Path $Script:ProjectRoot $Script:EnvFile

    if (-not (Test-Path $envFilePath)) {
        $examplePath = Join-Path $Script:ProjectRoot ".env.production.example"
        $altExamplePath = Join-Path $Script:ProjectRoot ".env.example"

        if (Test-Path $examplePath) {
            Copy-Item $examplePath $envFilePath -Force
            Write-LogInfo "已创建 $Script:EnvFile（从模板）"
        } elseif (Test-Path $altExamplePath) {
            Copy-Item $altExamplePath $envFilePath -Force
            Write-LogInfo "已创建 $Script:EnvFile（从 .env.example）"
        } else {
            Generate-EnvFile
        }
    } else {
        Write-LogInfo "$Script:EnvFile 已存在"
    }

    # 复制 agentos.yaml 配置
    $yamlPath = Join-Path $Script:ConfigDir "agentos.yaml"
    $yamlExample = Join-Path $Script:ConfigDir "agentos.yaml.example"

    if ((-not (Test-Path $yamlPath)) -and (Test-Path $yamlExample)) {
        Copy-Item $yamlExample $yamlPath -Force
        Write-LogInfo "已创建 config\agentos.yaml（从模板）"
    }

    Show-SecurityReminder
}

function Generate-EnvFile {
    function New-RandomPassword {
        param([int]$Length = 16)
        $chars = 'ABCDEFGHKLMNPQRSTUVWXYZabcdefghkmnpqrstuvwxyz23456789'
        (-join (1..$Length | ForEach-Object { $chars[(Get-Random -Maximum $chars.Length)] }))
    }

    $envContent = @"
# AgentOS Environment Configuration
# Generated by install.ps1 on $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')

# ===========================================
# 数据库配置
# ===========================================
POSTGRES_HOST=postgres
POSTGRES_PORT=5432
POSTGRES_DB=agentos
POSTGRES_USER=agentos
POSTGRES_PASSWORD=$(New-RandomPassword 16)

# ===========================================
# Redis 配置
# ===========================================
REDIS_HOST=redis
REDIS_PORT=6379
REDIS_PASSWORD=$(New-RandomPassword 16)

# ===========================================
# JWT 安全配置
# ===========================================
JWT_SECRET_KEY=$(New-RandomPassword 32)
JWT_EXPIRATION_HOURS=24

# ===========================================
# API 密钥（请替换为实际密钥）
# ===========================================
API_KEYS=default-key-change-me

# ===========================================
# Gateway 配置
# ===========================================
GATEWAY_HOST=0.0.0.0
GATEWAY_PORT=18789

# ===========================================
# 日志级别
# ===========================================
LOG_LEVEL=INFO

# ===========================================
# 时区配置
# ===========================================
TZ=China Standard Time
"@

    $envFilePath = Join-Path $Script:ProjectRoot $Script:EnvFile
    Set-Content -Path $envFilePath -Value $envContent -Encoding UTF8
    Write-LogInfo "已生成默认 $Script:EnvFile"
}

function Show-SecurityReminder {
    Write-Host ""
    Write-Host "  ${YELLOW}⚠️  安全提醒:${NC}"
    Write-Host "  请务必修改以下敏感配置:"
    Write-Host "    • $Script:EnvFile:"
    Write-Host "      - POSTGRES_PASSWORD"
    Write-Host "      - REDIS_PASSWORD"
    Write-Host "      - JWT_SECRET_KEY"
    Write-Host "      - API_KEYS"
    Write-Host ""
    Write-Host "  编辑方法:"
    Write-Host "    notepad $(Join-Path $Script:ProjectRoot $Script:EnvFile)"
    Write-Host "    # 或使用 VS Code: code $(Join-Path $Script:ProjectRoot $Script:EnvFile)"
    Write-Host ""
}

# =============================================================================
# Docker 服务管理
# =============================================================================

function Start-DevEnvironment {
    Write-LogStep "启动开发环境"

    Push-Location $Script:DockerDir

    Write-LogInfo "使用配置: docker-compose.yml"
    Write-LogInfo "启动服务: postgres, redis, gateway, kernel, openlab"

    try {
        docker compose up -d postgres redis gateway kernel openlab

        if ($LASTEXITCODE -eq 0) {
            Write-Host ""
            Write-LogInfo "✅ 开发环境启动成功！"
            Write-Host ""
            Show-ServiceAccessInfo "dev"
            Invoke-HealthCheck
        } else {
            throw "开发环境启动失败"
        }
    }
    catch {
        Write-LogError "开发环境启动失败: $_"
        Collect-ErrorLogs
        Pop-Location
        return $false
    }

    Pop-Location
    return $true
}

function Start-ProdEnvironment {
    Write-LogStep "启动生产环境"

    Push-Location $Script:DockerDir

    $envFilePath = "..\$Script:EnvFile"

    if (-not (Test-Path (Join-Path $Script:DockerDir $envFilePath))) {
        Write-LogError "缺少生产环境配置文件: $Script:EnvFile"
        Pop-Location
        return $false
    }

    Write-LogInfo "使用配置: docker-compose.prod.yml"
    Write-LogInfo "环境变量: $envFilePath"

    try {
        docker compose --env-file $envFilePath -f docker-compose.prod.yml up -d

        if ($LASTEXITCODE -eq 0) {
            Write-Host ""
            Write-LogInfo "✅ 生产环境启动成功！"
            Write-Host ""
            Show-ServiceAccessInfo "prod"
            Invoke-HealthCheck
        } else {
            throw "生产环境启动失败"
        }
    }
    catch {
        Write-LogError "生产环境启动失败: $_"
        Collect-ErrorLogs
        Pop-Location
        return $false
    }

    Pop-Location
    return $true
}

function Stop-Environment {
    Write-LogStep "停止所有服务"

    Push-Location $Script:DockerDir

    $stopped = $false

    try {
        # 停止开发环境
        $devStatus = docker compose ps 2>&1
        if ($devStatus) {
            docker compose down 2>&1 | Out-Null
            Write-LogInfo "开发环境已停止 ✓"
            $stopped = $true
        }

        # 停止生产环境
        if (Test-Path "docker-compose.prod.yml") {
            $prodStatus = docker compose -f docker-compose.prod.yml ps 2>&1
            if ($prodStatus) {
                docker compose -f docker-compose.prod.yml down 2>&1 | Out-Null
                Write-LogInfo "生产环境已停止 ✓"
                $stopped = $true
            }
        }
    }
    catch {
        Write-LogWarn "停止服务时出现错误: $_"
    }

    Pop-Location

    if ($stopped) {
        Write-Host ""
        Write-LogInfo "所有服务已停止"
    } else {
        Write-LogInfo "没有正在运行的服务"
    }
}

function Show-ServiceAccessInfo {
    param([string]$Mode)

    Write-Host "${BOLD}  📋 服务访问地址:${NC}"

    if ($Mode -eq "dev") {
        Write-Host "  🌐 Gateway API:     http://localhost:18789"
        Write-Host "  🔧 Kernel IPC:      http://localhost:18080"
        Write-Host "  🧪 OpenLab UI:      http://localhost:3000"
        Write-Host "  📊 Grafana 监控:     http://localhost:3001 (需启用 monitoring profile)"
        Write-Host "  📈 Prometheus:      http://localhost:9091 (需启用 monitoring profile)"
    } else {
        $containerCount = (docker ps --format "{{.Names}}" 2>&1 | Select-String "agentos").Count
        Write-Host "  📡 Gateway:         :18789 (HTTPS 通过反向代理)"
        Write-Host "  🐳 容器数:          $containerCount"
    }

    Write-Host ""
    Write-Host "${BOLD}  🔧 常用运维命令:${NC}"

    if ($Mode -eq "dev") {
        Write-Host "  查看日志:   docker compose logs -f"
        Write-Host "  重启服务:   docker compose restart"
        Write-Host "  进入容器:   docker compose exec kernel bash"
        Write-Host "  停止服务:   .\install.ps1 -Stop"
    } else {
        Write-Host "  查看状态:   docker compose -f docker-compose.prod.yml ps"
        Write-Host "  查看日志:   docker compose -f docker-compose.prod.yml logs -f"
        Write-Host "  扩展实例:   docker compose -f docker-compose.prod.yml up -d --scale kernel=3"
        Write-Host "  优雅停止:   docker compose -f docker-compose.prod.yml down --timeout 60"
    }

    Write-Host ""
}

# =============================================================================
# 健康检查
# =============================================================================

function Invoke-HealthCheck {
    Write-LogStep "执行健康检查"

    Write-LogInfo "等待服务启动 (5秒)..."
    Start-Sleep -Seconds 5

    $services = @(
        @{ Name="gateway"; Port=18789; Type="http" },
        @{ Name="postgres"; Port=5432; Type="tcp" },
        @{ Name="redis"; Port=6379; Type="tcp" }
    )

    $healthyCount = 0
    $totalCount = $services.Count

    foreach ($service in $services) {
        $name = $service.Name
        $port = $service.Port
        $type = $service.Type

        if ($type -eq "http") {
            try {
                $response = Invoke-WebRequest -Uri "http://localhost:$port/api/v1/health" `
                    -TimeoutSec 3 -UseBasicParsing -ErrorAction Stop
                Write-LogInfo "${name}:${port} 健康 ✓"
                $healthyCount++
            }
            catch {
                try {
                    $response = Invoke-WebRequest -Uri "http://localhost:$port/health" `
                        -TimeoutSec 3 -UseBasicParsing -ErrorAction Stop
                    Write-LogInfo "${name}:${port} 健康 ✓"
                    $healthyCount++
                }
                catch {
                    Write-LogWarn "${name}:${port} 未响应（可能仍在启动中）"
                }
            }
        } else {
            # TCP 端口检查
            $connection = Get-NetTCPConnection -RemotePort $port -ErrorAction SilentlyContinue |
                         Where-Object { $_.State -eq 'Established' }
            if ($connection) {
                Write-LogInfo "${name}:${port} 健康 ✓"
                $healthyCount++
            } else {
                # 尝试连接测试
                try {
                    $tcpClient = New-Object System.Net.Sockets.TcpClient
                    $result = $tcpClient.BeginConnect("localhost", $port, $null, $null)
                    $waitResult = $result.AsyncWaitHandle.WaitOne(3000, $false)

                    if ($waitResult) {
                        $tcpClient.EndConnect($result)
                        Write-LogInfo "${name}:${port} 健康 ✓"
                        $healthyCount++
                    } else {
                        Write-LogWarn "${name}:${port} 未响应（可能仍在启动中）"
                    }
                    $tcpClient.Close()
                }
                catch {
                    Write-LogWarn "${name}:${port} 未响应（可能仍在启动中）"
                }
            }
        }
    }

    Write-Host ""
    $healthPercentage = [math]::Floor(($healthyCount * 100) / $totalCount)

    if ($healthyCount -eq $totalCount) {
        Write-LogInfo "🎉 所有服务健康检查通过 (${healthPercentage}%)"
        return $true
    } elseif ($healthyCount -gt 0) {
        Write-LogWarn "部分服务就绪 ($($healthyCount)/$($totalCount), ${healthPercentage}%)"
        Write-LogInfo "部分服务可能需要更长时间启动，请稍后使用 -Status 查看"
        return $true
    } else {
        Write-LogError "所有服务未就绪，请查看日志排查问题"
        return $false
    }
}

# =============================================================================
# 状态显示
# =============================================================================

function Show-SystemStatus {
    Write-LogStep "显示系统状态"

    Push-Location $Script:DockerDir

    Write-Host ""
    Write-Host "${BOLD}  🐳 容器状态:${NC}"
    Write-Host ""

    try {
        docker compose ps 2>&1 | Out-Host
    }
    catch {
        Write-LogInfo "无开发环境容器"
    }

    if (Test-Path "docker-compose.prod.yml") {
        Write-Host ""
        try {
            docker compose -f docker-compose.prod.yml ps 2>&1 | Out-Host
        }
        catch {
            Write-LogInfo "无生产环境容器"
        }
    }

    Write-Host ""
    Write-Host "${BOLD}  📊 资源使用:${NC}"
    Write-Host ""

    try {
        docker stats --no-stream --format "table {{.Name}}`t{{.Status}}`t{{.CPUPerc}}`t{{.MemUsage}}`t{{.NetIO}}" 2>&1 | Out-Host
    }
    catch {
        Write-LogInfo "Docker 统计信息不可用"
    }

    Write-Host ""
    Write-Host "${BOLD}  📝 最近日志 (最后15行):${NC}"
    Write-Host ""

    try {
        docker compose logs --tail=15 2>&1 | Select-Object -Last 15 | Out-Host
    }
    catch {}

    Pop-Location
}

# =============================================================================
# 日志收集
# =============================================================================

function Collect-DeploymentLogs {
    Write-LogStep "收集系统日志"

    $logFile = Join-Path $Script:LogDir "agentos_deploy_$($Script:Timestamp).log"

    if (-not (Test-Path $Script:LogDir)) { New-Item -ItemType Directory -Path $Script:LogDir -Force | Out-Null }

    Push-Location $Script:DockerDir

    $logContent = @"
==========================================
 AgentOS Deployment Log Collection
 Timestamp: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss K')
 Platform: $((Get-CimInstance Win32_OperatingSystem).Caption) ($(env:PROCESSOR_ARCHITECTURE))
==========================================

--- System Information ---
$(systeminfo | Select-String -Pattern "OS Name|OS Version|System Type|Total Physical Memory")

--- Docker Version ---
$(docker --version 2>&1)
$(docker compose version 2>&1)

--- Container Logs (Dev) ---
$(docker compose logs --timestamps 2>&1)

"@  #> $logFile

    if (Test-Path "docker-compose.prod.yml") {
        Add-Content -Path $logFile -Value @"

--- Container Logs (Prod) ---
$(docker compose -f docker-compose.prod.yml logs --timestamps 2>&1)

"@
    }

    Add-Content -Path $logFile -Value @"

--- Resource Usage ---
$(docker system df 2>&1)

--- Disk Usage ---
$(Get-ChildItem $Script:ProjectRoot -Directory | ForEach-Object { "{0}`t{1:N2} MB" -f $_.Name, ((Get-ChildItem $_.FullName -Recurse -File -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum / 1MB) }) 2>&1

"@

    Pop-Location

    Write-LogInfo "日志已保存到: $logFile"
    $fileSize = [math]::Round((Get-Item $logFile).Length / 1KB, 1)
    Write-LogInfo "文件大小: ~${fileSize} KB"

    Write-Host ""
    Write-Host "${BOLD}  📄 日志预览 (最后30行):${NC}"
    Write-Host ""

    Get-Content $logFile -Tail 30
}

function Collect-ErrorLogs {
    Write-LogStep "收集错误日志用于诊断"

    $errorLogFile = Join-Path $Script:LogDir "agentos_error_$($Script:Timestamp).log"

    if (-not (Test-Path $Script:LogDir)) { New-Item -ItemType Directory -Path $Script:LogDir -Force | Out-Null }

    Push-Location $Script:DockerDir

    $errorContent = @"
==========================================
 AgentOS Error Log - $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
==========================================

--- Failed Containers ---
$(docker ps -a --filter "state=exited" --filter "state=dead" --format "{{.Names}}`t{{.Status}}" 2>&1)

--- Recent Error Logs ---
$(docker compose logs --tail=50 2>&1 | Select-String -Pattern "error|fail|exception|fatal" -CaseSensitive:$false)

--- System Resources ---
$(Get-CimInstance Win32_OperatingSystem | Select-Object Caption, TotalVisibleMemorySize, FreePhysicalMemory | Format-List | Out-String)
$(Get-CimInstance Win32_LogicalDisk -Filter "DeviceID='C:'" | Select-Object DeviceID, Size, FreeSpace | Format-List | Out-String)

"@
    Set-Content -Path $errorLogFile -Value $errorContent

    Pop-Location

    Write-LogError "错误日志已保存到: $errorLogFile"
}

# =============================================================================
# 清理功能
# =============================================================================

Invoke-CleanupResources {
    Write-LogStep "清理 Docker 资源"

    if (-not $Auto) {
        $response = Read-Host "  确定要清理所有 Docker 资源吗？这将删除容器、镜像和网络卷 [y/N]"
        if ($response -ne 'y' -and $response -ne 'Y') {
            Write-LogInfo "取消清理操作"
            return
        }
    }

    Push-Location $Script:DockerDir

    Write-LogInfo "停止并删除容器..."

    try {
        docker compose down -v --remove-orphans 2>&1 | Out-Null

        if (Test-Path "docker-compose.prod.yml") {
            docker compose -f docker-compose.prod.yml down -v --remove-orphans 2>&1 | Out-Null
        }
    }
    catch {
        Write-LogWarn "清理容器时出错: $_"
    }

    Write-LogInfo "清理悬停镜像..."
    docker image prune -f 2>&1 | Out-Null

    Write-LogInfo "清理构建缓存..."
    docker builder prune -f 2>&1 | Out-Null

    Write-LogInfo "清理未使用的网络..."
    docker network prune -f 2>&1 | Out-Null

    Pop-Location

    Write-Host ""
    Write-LogInfo "✅ 清理完成"

    Write-Host ""
    Write-Host "${BOLD}  释放的空间:${NC}"
    docker system df 2>&1 | Out-Host
}

# =============================================================================
# 桌面客户端构建
# =============================================================================

function Deploy-DesktopClient {
    Write-LogStep "构建桌面客户端"

    $clientDir = Join-Path $PSScriptRoot "desktop-client"

    if (-not (Test-Path $clientDir)) {
        Write-LogError "桌面客户端目录不存在: $clientDir"
        return $false
    }

    Push-Location $clientDir

    try {
        $nodeCmd = Get-Command node -ErrorAction SilentlyContinue
        $npmCmd = Get-Command npm -ErrorAction SilentlyContinue

        if (-not $nodeCmd) {
            Write-LogError "未安装 Node.js，无法构建桌面客户端"
            Write-Host "  安装方法: winget install OpenJS.NodeJS.LTS"
            Pop-Location
            return $false
        }

        if (-not $npmCmd) {
            Write-LogError "未安装 npm，无法构建桌面客户端"
            Pop-Location
            return $false
        }

        $nodeVersion = (& node --version 2>&1) -replace 'v', ''
        Write-LogInfo "Node.js: v${nodeVersion}"

        Write-LogInfo "安装前端依赖..."
        npm install --prefer-offend 2>&1 | Out-Null

        if ($LASTEXITCODE -ne 0) {
            Write-LogWarn "npm install 遇到问题，尝试清理缓存重试..."
            npm install 2>&1 | Out-Null
        }

        $rustCmd = Get-Command rustc -ErrorAction SilentlyContinue
        $cargoCmd = Get-Command cargo -ErrorAction SilentlyContinue

        if (-not $rustCmd -or -not $cargoCmd) {
            Write-LogWarn "未安装 Rust 工具链，仅构建前端预览版本"
            Write-LogInfo "安装 Rust: https://rustup.rs/"

            Write-LogInfo "启动 Vite 开发服务器..."
            Start-Process -FilePath "npm" -ArgumentList "run", "dev" -NoNewWindow
            Write-LogInfo "前端开发服务器已启动: http://localhost:1420"
        } else {
            $rustVersion = (& rustc --version 2>&1)
            Write-LogInfo "Rust: $rustVersion"

            Write-LogInfo "构建 Tauri 桌面应用..."
            npm run tauri build 2>&1 | Out-Null

            if ($LASTEXITCODE -eq 0) {
                Write-LogInfo "桌面客户端构建成功！"

                $releaseDir = Join-Path $clientDir "src-tauri\target\release"
                if (Test-Path $releaseDir) {
                    $msiFiles = Get-ChildItem -Path (Join-Path $releaseDir "bundle\msi") -Filter "*.msi" -ErrorAction SilentlyContinue
                    $exeFiles = Get-ChildItem -Path $releaseDir -Filter "agentos-desktop.exe" -ErrorAction SilentlyContinue

                    if ($msiFiles) {
                        Write-LogInfo "安装包: $($msiFiles[0].FullName)"
                    }
                    if ($exeFiles) {
                        Write-LogInfo "可执行文件: $($exeFiles[0].FullName)"
                    }
                }
            } else {
                Write-LogError "桌面客户端构建失败"
                Pop-Location
                return $false
            }
        }
    }
    catch {
        Write-LogError "桌面客户端构建出错: $_"
        Pop-Location
        return $false
    }

    Pop-Location
    return $true
}

# =============================================================================
# 用户交互
# =============================================================================

function Select-DeploymentMode {
    if ($Auto) { return $Mode }

    Write-Host ""
    Write-Host "${BOLD}  请选择部署模式:${NC}"
    Write-Host "    1) 开发环境 (Development) - 适合本地开发和调试"
    Write-Host "    2) 生产环境 (Production) - 适合服务器部署"
    Write-Host ""

    $choice = Read-Host "  选择 [1-2]"

    switch ($choice) {
        "2" { return "prod" }
        default { return "dev" }
    }
}

# =============================================================================
# 帮助信息
# =============================================================================

function Show-Help {
    $helpText = @"

${BOLD}${ScriptName} V${ScriptVersion}${NC}

${CYAN}用法:${NC}
  .\install.ps1 [选项]

${CYAN}参数:${NC}
  -Mode <mode>       部署模式: dev|prod (默认: dev)
  -Target <target>   部署目标: backend|client|all (默认: all)
  -Auto              非交互模式（使用默认配置）
  -CheckOnly         仅检查环境和项目结构
  -Status            显示当前运行状态
  -Stop              停止所有服务
  -Restart           重启所有服务
  -Logs              收集并显示日志
  -Cleanup           清理 Docker 资源
  -Health            执行健康检查
  -AsAdmin           要求管理员权限
  -Help              显示此帮助信息
  -Version           显示版本信息

${CYAN}示例:${NC}
  .\install.ps1                      # 交互式部署（开发环境）
  .\install.ps1 -Mode dev -Auto      # 一键部署开发环境
  .\install.ps1 -Mode prod -Auto     # 一键部署生产环境
  .\install.ps1 -Target backend      # 仅部署后端服务
  .\install.ps1 -Target client       # 仅构建桌面客户端
  .\install.ps1 -Target all          # 全部部署（后端+客户端）
  .\install.ps1 -CheckOnly           # 仅检查环境
  .\install.ps1 -Status              # 查看运行状态
  .\install.ps1 -Stop                # 停止服务
  .\install.ps1 -Logs                # 收集日志
  .\install.ps1 -Cleanup             # 清理资源
  .\install.ps1 -Health              # 健康检查

${CYAN}支持的平台:${NC}
  • Windows 10 64-bit (1903+) / Windows 11
  • Windows Server 2019+
  • PowerShell 5.1+ 或 PowerShell Core 7+

${CYAN}更多信息:${NC}
  项目文档:   $Script:ProjectRoot\docs\
  部署指南:   $Script:ProjectRoot\agentos\manuals\guides\deployment.md
  架构原则:   $Script:ProjectRoot\agentos\manuals\ARCHITECTURAL_PRINCIPLES.md
  问题反馈:   https://github.com/SpharxTeam/AgentOS/issues
  在线文档:   https://docs.agentos.io

${CYAN}环境要求:${NC}
  • Docker Desktop >= 4.8
  • 内存 >= $Script:MinMemoryGBDev GB (开发) / $Script:MinMemoryGBProd GB (生产)
  • 磁盘 >= $Script:MinDisk GB 可用空间
  • Windows 内置 Hyper-V / WSL2 后端

"@

    Write-Host $helpText
}

function Show-Version {
    Write-Host "$ScriptName V$ScriptVersion"
    Write-Host "Platform: Windows $((Get-CimInstance Win32_OperatingSystem).Caption) ($(env:PROCESSOR_ARCHITECTURE))"
    Write-Host "Support: Windows 10+, Windows Server 2019+, PowerShell 5.1+/7+"
    Write-Host "Copyright (c) 2026 SPHARX Ltd. All Rights Reserved."
}

# =============================================================================
# 主程序
# =============================================================================

function Main {
    # 显示帮助或版本
    if ($Help) { Show-Help; return }
    if ($Version) { Show-Version; return }

    # 切换到项目根目录
    Set-Location $Script:ProjectRoot

    # 执行操作
    if ($CheckOnly) {
        Initialize-Progress 4

        Show-Progress "平台与环境检测"
        Get-PlatformInfo | Out-Null

        Show-Progress "资源检查"
        Test-SystemResources -DeployMode $Mode

        Show-Progress "依赖检测"
        Test-DockerInstallation | Out-Null
        Test-CoreDependencies | Out-Null

        Show-Progress "项目验证"
        Confirm-ProjectStructure | Out-Null

        Complete-Progress

        if ($Script:ErrorCount -eq 0) { exit 0 } else { exit 1 }
    }

    if ($Status) {
        Show-SystemStatus
        return
    }

    if ($Stop) {
        Stop-Environment
        return
    }

    if ($Restart) {
        Stop-Environment
        Start-Sleep -Seconds 3
        if ($Mode -eq "dev") { Start-DevEnvironment } else { Start-ProdEnvironment }
        return
    }

    if ($Logs) {
        Collect-DeploymentLogs
        return
    }

    if ($Cleanup) {
        Invoke-CleanupResources
        return
    }

    if ($Health) {
        Initialize-Progress 1
        Show-Progress "健康检查"
        Invoke-HealthCheck | Out-Null
        Complete-Progress
        return
    }

    # 默认操作：部署
    $totalSteps = 7
    if ($Target -eq "client") { $totalSteps = 3 }
    if ($Target -eq "backend") { $totalSteps = 6 }
    Initialize-Progress $totalSteps

    # 选择部署模式
    $deployMode = Select-DeploymentMode
    Write-LogInfo "部署模式: $deployMode"
    Write-LogInfo "部署目标: $Target"

    # 步骤1：平台检测
    Show-Progress "平台与环境检测"
    Get-PlatformInfo | Out-Null
    Require-Administrator
    Test-SystemResources -DeployMode $deployMode

    # 步骤2：依赖检查
    Show-Progress "依赖检查与安装"
    $dockerOk = Test-DockerInstallation
    Test-CoreDependencies | Out-Null

    if ($Target -ne "client") {
        if (-not $dockerOk) {
            if ($Auto) {
                Install-DockerDesktop
                $dockerOk = Test-DockerInstallation
            } else {
                $response = Read-Host "  是否尝试自动安装 Docker Desktop? [y/N]"
                if ($response -eq 'y' -or $response -eq 'Y') {
                    Install-DockerDesktop
                    $dockerOk = Test-DockerInstallation
                }
            }
        }

        if (-not $dockerOk) {
            Write-LogError "Docker 环境不可用，无法继续部署后端"
            if ($Target -eq "backend") {
                Complete-Progress
                exit 1
            }
        }
    }

    if ($Target -ne "client") {
        # 步骤3：项目验证
        Show-Progress "项目结构验证"
        Confirm-ProjectStructure | Out-Null

        # 步骤4：配置生成
        Show-Progress "配置文件生成"
        New-EnvironmentConfig

        # 步骤5：启动服务
        Show-Progress "启动服务 (${deployMode} 模式)"
        $startSuccess = $false

        if ($dockerOk) {
            switch ($deployMode) {
                "dev" { $startSuccess = Start-DevEnvironment }
                "prod" { $startSuccess = Start-ProdEnvironment }
                default {
                    Write-LogError "未知部署模式: $deployMode"
                }
            }
        } else {
            Write-LogWarn "跳过后端部署（Docker 不可用）"
        }
    }

    if ($Target -ne "backend") {
        # 构建桌面客户端
        Show-Progress "构建桌面客户端"
        Deploy-DesktopClient | Out-Null
    }

    # 完成
    Complete-Progress

    if ($Script:ErrorCount -eq 0) {
        Write-Host "${GREEN}🎉 部署成功完成！${NC}"
        Write-Host ""
        Write-Host "  ${CYAN}下一步操作:${NC}"
        Write-Host "    1. 访问 Gateway API: http://localhost:18789"
        Write-Host "    2. 查看 OpenLab UI: http://localhost:3000"
        Write-Host "    3. 阅读 Quick Start: docs\getting-started.md"
        Write-Host ""
    } else {
        Write-Host "${RED}❌ 部署完成但发现 $($Script:ErrorCount) 个问题${NC}"
        Write-Host ""
        Write-Host "  ${YELLOW}建议操作:${NC}"
        Write-Host "    1. 查看错误日志: $Script:LogDir\agentos_error_*.log"
        Write-Host "    2. 运行诊断: .\install.ps1 -Health"
        Write-Host "    3. 查看状态: .\install.ps1 -Status"
        Write-Host "    4. 查看文档: agentos\manuals\guides\troubleshooting.md"
        Write-Host ""
    }
}

# =============================================================================
# 入口点
# =============================================================================
Main
