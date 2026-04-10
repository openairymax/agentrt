# AgentOS 一键安装设置脚本 (Windows PowerShell)

[CmdletBinding()]
param(
    [Parameter()]
    [switch]$InstallDeps,
    
    [Parameter()]
    [switch]$InstallVcpkg,
    
    [Parameter()]
    [switch]$BuildProject,
    
    [Parameter()]
    [switch]$Help
)

# 设置错误处理
$ErrorActionPreference = "Stop"

function Write-Info {
    Write-Host "[INFO] " -NoNewline -ForegroundColor Green
    Write-Host $args
}

function Write-Warn {
    Write-Host "[WARN] " -NoNewline -ForegroundColor Yellow
    Write-Host $args
}

function Write-Error {
    Write-Host "[ERROR] " -NoNewline -ForegroundColor Red
    Write-Host $args
}

function Show-Help {
    Write-Host "AgentOS 一键安装设置脚本 (Windows)"
    Write-Host "用法: .\setup.ps1 [选项]"
    Write-Host ""
    Write-Host "选项:"
    Write-Host "  -InstallDeps    安装系统依赖 (需要管理员权限)"
    Write-Host "  -InstallVcpkg   安装 vcpkg 和依赖"
    Write-Host "  -BuildProject   构建项目"
    Write-Host "  -Help           显示此帮助信息"
    Write-Host ""
    Write-Host "示例:"
    Write-Host "  .\setup.ps1 -InstallVcpkg -BuildProject"
    Write-Host "  .\setup.ps1 -InstallDeps"
}

if ($Help) {
    Show-Help
    exit 0
}

# 检查是否为管理员
function Test-Administrator {
    $currentUser = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($currentUser)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

# 安装系统依赖
function Install-SystemDependencies {
    Write-Info "正在安装系统依赖..."
    
    # 检查 Chocolatey 是否安装
    if (!(Get-Command choco -ErrorAction SilentlyContinue)) {
        Write-Info "正在安装 Chocolatey..."
        Set-ExecutionPolicy Bypass -Scope Process -Force
        [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072
        Invoke-Expression ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))
    }
    
    # 安装所需软件包
    $packages = @(
        "git",
        "cmake",
        "visualstudio2022-buildtools",
        "openssl",
        "sqlite",
        "curl",
        "yaml-cpp",
        "cjson",
        "libmicrohttpd",
        "libwebsockets",
        "python3"
    )
    
    foreach ($package in $packages) {
        Write-Info "正在安装 $package..."
        choco install $package -y
    }
    
    Write-Info "系统依赖安装完成"
}

# 安装 vcpkg
function Install-Vcpkg {
    Write-Info "正在安装 vcpkg..."
    
    if (Test-Path "vcpkg") {
        Write-Info "vcpkg 已存在，跳过安装"
        return
    }
    
    # 克隆 vcpkg 仓库
    git clone https://github.com/Microsoft/vcpkg.git
    
    # 运行引导脚本
    Push-Location vcpkg
    try {
        .\bootstrap-vcpkg.bat
    } finally {
        Pop-Location
    }
    
    # 设置环境变量
    $vcpkgPath = (Get-Item "vcpkg").FullName
    [Environment]::SetEnvironmentVariable("VCPKG_ROOT", $vcpkgPath, "User")
    [Environment]::SetEnvironmentVariable("PATH", "$env:PATH;$vcpkgPath", "User")
    
    Write-Info "vcpkg 安装完成"
    Write-Info "VCPKG_ROOT 已设置为: $vcpkgPath"
}

# 安装 vcpkg 依赖
function Install-VcpkgDependencies {
    Write-Info "正在安装 vcpkg 依赖..."
    
    if (!(Test-Path "vcpkg.json")) {
        Write-Error "vcpkg.json 不存在"
        exit 1
    }
    
    if (!(Test-Path "vcpkg\vcpkg.exe")) {
        Write-Error "vcpkg 未找到"
        exit 1
    }
    
    Push-Location vcpkg
    try {
        .\vcpkg install ..\vcpkg.json
    } finally {
        Pop-Location
    }
    
    Write-Info "vcpkg 依赖安装完成"
}

# 构建项目
function Build-Project {
    Write-Info "正在构建项目..."
    
    if (!(Test-Path "build.ps1")) {
        Write-Error "build.ps1 未找到"
        exit 1
    }
    
    .\build.ps1 -BuildType Release -Clean
    
    Write-Info "项目构建完成"
}

# 主函数
function Main {
    Write-Info "AgentOS 一键安装设置脚本 v1.0.0"
    Write-Info "======================================"
    
    Write-Info "操作系统: Windows"
    Write-Info "主机名: $env:COMPUTERNAME"
    Write-Info "用户名: $env:USERNAME"
    
    # 安装系统依赖
    if ($InstallDeps) {
        if (!(Test-Administrator)) {
            Write-Error "安装系统依赖需要管理员权限"
            Write-Info "请以管理员身份运行 PowerShell"
            exit 1
        }
        Install-SystemDependencies
    }
    
    # 安装 vcpkg
    if ($InstallVcpkg) {
        Install-Vcpkg
    }
    
    # 安装 vcpkg 依赖
    if ($InstallVcpkg -and (Test-Path "vcpkg")) {
        Install-VcpkgDependencies
    }
    
    # 构建项目
    if ($BuildProject) {
        Build-Project
    }
    
    # 如果未指定任何选项，显示交互式菜单
    if (!$InstallDeps -and !$InstallVcpkg -and !$BuildProject) {
        Write-Info "请选择要执行的操作:"
        Write-Host "1) 安装系统依赖 (需要管理员权限)" -ForegroundColor Cyan
        Write-Host "2) 安装 vcpkg 和依赖" -ForegroundColor Cyan
        Write-Host "3) 构建项目" -ForegroundColor Cyan
        Write-Host "4) 全部执行" -ForegroundColor Cyan
        Write-Host "5) 退出" -ForegroundColor Cyan
        
        $choice = Read-Host "请输入选项 (1-5)"
        
        switch ($choice) {
            "1" {
                if (!(Test-Administrator)) {
                    Write-Error "需要管理员权限"
                    Write-Info "请以管理员身份重新运行此脚本"
                    exit 1
                }
                Install-SystemDependencies
            }
            "2" {
                Install-Vcpkg
                Install-VcpkgDependencies
            }
            "3" {
                Build-Project
            }
            "4" {
                if (!(Test-Administrator)) {
                    Write-Error "需要管理员权限"
                    Write-Info "请以管理员身份重新运行此脚本"
                    exit 1
                }
                Install-SystemDependencies
                Install-Vcpkg
                Install-VcpkgDependencies
                Build-Project
            }
            "5" {
                exit 0
            }
            default {
                Write-Error "无效选项"
                exit 1
            }
        }
    }
    
    Write-Info "设置完成！"
    Write-Info "======================================"
    Write-Info "下一步："
    Write-Info "1. 运行 .\build.ps1 进行构建"
    Write-Info "2. 查看 README.md 获取使用说明"
    Write-Info "3. 运行示例程序: .\build_windows_*\bin\*.exe"
    
    if (Test-Path "vcpkg") {
        Write-Info "vcpkg 已安装，环境变量已设置"
        Write-Info "VCPKG_ROOT: $(Get-Item vcpkg).FullName"
    }
}

# 执行主函数
Main