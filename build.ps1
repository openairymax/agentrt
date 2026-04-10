# AgentOS 跨平台构建脚本 (Windows PowerShell)

[CmdletBinding()]
param(
    [Parameter(Position=0)]
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$BuildType = "Release",
    
    [Parameter()]
    [switch]$Clean,
    
    [Parameter()]
    [switch]$Install,
    
    [Parameter()]
    [switch]$Help
)

# 颜色定义
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
    Write-Host "AgentOS 构建脚本 (Windows)"
    Write-Host "用法: .\build.ps1 [选项]"
    Write-Host ""
    Write-Host "选项:"
    Write-Host "  -BuildType <类型>    构建类型 (Debug|Release|RelWithDebInfo|MinSizeRel) [默认: Release]"
    Write-Host "  -Clean                清理构建目录"
    Write-Host "  -Install              安装构建结果"
    Write-Host "  -Help                 显示此帮助信息"
    Write-Host ""
    Write-Host "示例:"
    Write-Host "  .\build.ps1 -BuildType Release"
    Write-Host "  .\build.ps1 -BuildType Debug -Clean"
    Write-Host "  .\build.ps1 -BuildType Release -Install"
}

if ($Help) {
    Show-Help
    exit 0
}

# 主函数
function Main {
    Write-Info "AgentOS 构建脚本 v1.0.0"
    Write-Info "================================"
    
    # 检测系统信息
    $OS = "windows"
    $Arch = if ([Environment]::Is64BitOperatingSystem) { "x64" } else { "x86" }
    $HostName = [System.Net.Dns]::GetHostName()
    $UserName = [Environment]::UserName
    
    Write-Info "操作系统: Windows"
    Write-Info "架构: $Arch"
    Write-Info "主机名: $HostName"
    Write-Info "用户名: $UserName"
    
    # 构建目录
    $BuildDir = "build_windows_${Arch}_${BuildType}"
    Write-Info "构建目录: $BuildDir"
    
    # 清理选项
    if ($Clean -and (Test-Path $BuildDir)) {
        Write-Info "清理构建目录: $BuildDir"
        Remove-Item -Recurse -Force $BuildDir -ErrorAction SilentlyContinue
    }
    
    # 创建构建目录
    if (!(Test-Path $BuildDir)) {
        New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
    }
    
    # 进入构建目录
    Push-Location $BuildDir
    
    try {
        # CMake 配置
        Write-Info "正在配置 CMake..."
        
        $CMakeOptions = @(
            "-DCMAKE_BUILD_TYPE=$BuildType",
            "-DBUILD_TESTS=OFF",
            "-DBUILD_EXAMPLES=ON",
            "-DBUILD_DOCS=OFF",
            "-DENABLE_COVERAGE=OFF",
            # Windows 特定选项
            "-DCMAKE_CXX_FLAGS=/DWIN32 /D_WINDOWS /W4 /WX-",
            "-DCMAKE_C_FLAGS=/DWIN32 /D_WINDOWS /W4 /WX-",
            # 禁用有问题的模块（第一阶段）
            "-DBUILD_MANAGER=OFF",
            "-DBUILD_DAEMON=OFF",
            "-DBUILD_OPENLAB=OFF",
            "-DBUILD_TOOLKIT=OFF",
            "-DBUILD_HEAPSTORE=OFF",
            "-DBUILD_GATEWAY=OFF"
        )
        
        # 运行 CMake
        $CMakeCommand = "cmake"
        foreach ($option in $CMakeOptions) {
            $CMakeCommand += " $option"
        }
        $CMakeCommand += " .."
        
        Write-Info "CMake 命令: $CMakeCommand"
        Invoke-Expression $CMakeCommand
        
        if ($LASTEXITCODE -ne 0) {
            Write-Error "CMake 配置失败 (退出码: $LASTEXITCODE)"
            exit $LASTEXITCODE
        }
        
        # 构建
        Write-Info "正在构建..."
        $Cores = [System.Environment]::ProcessorCount
        Write-Info "使用 $Cores 个核心进行构建"
        
        $BuildCommand = "cmake --build . --config $BuildType --parallel $Cores"
        Write-Info "构建命令: $BuildCommand"
        Invoke-Expression $BuildCommand
        
        if ($LASTEXITCODE -ne 0) {
            Write-Error "构建失败 (退出码: $LASTEXITCODE)"
            exit $LASTEXITCODE
        }
        
        # 安装（可选）
        if ($Install) {
            Write-Info "正在安装..."
            $InstallDir = "../install"
            $InstallCommand = "cmake --install . --config $BuildType --prefix $InstallDir"
            Write-Info "安装命令: $InstallCommand"
            Invoke-Expression $InstallCommand
            
            if ($LASTEXITCODE -ne 0) {
                Write-Warn "安装失败 (退出码: $LASTEXITCODE)"
            } else {
                Write-Info "安装完成: $InstallDir"
            }
        }
        
        Write-Info "构建完成！"
        Write-Info "构建目录: $(Get-Location)"
        Write-Info "构建类型: $BuildType"
        
        # 显示构建结果
        $BinDir = if (Test-Path "bin") { "bin" } elseif (Test-Path "$BuildType") { $BuildType } else { "." }
        Write-Info "二进制文件目录: $BinDir"
        
        if (Test-Path $BinDir) {
            $ExeFiles = Get-ChildItem -Path $BinDir -Filter "*.exe" -ErrorAction SilentlyContinue
            $DllFiles = Get-ChildItem -Path $BinDir -Filter "*.dll" -ErrorAction SilentlyContinue
            $LibFiles = Get-ChildItem -Path $BinDir -Filter "*.lib" -ErrorAction SilentlyContinue
            
            if ($ExeFiles.Count -gt 0) {
                Write-Info "可执行文件:"
                $ExeFiles | ForEach-Object { Write-Host "  - $($_.Name)" -ForegroundColor Cyan }
            }
            
            if ($DllFiles.Count -gt 0) {
                Write-Info "动态库文件:"
                $DllFiles | ForEach-Object { Write-Host "  - $($_.Name)" -ForegroundColor Cyan }
            }
            
            if ($LibFiles.Count -gt 0) {
                Write-Info "静态库文件:"
                $LibFiles | ForEach-Object { Write-Host "  - $($_.Name)" -ForegroundColor Cyan }
            }
        }
        
    } finally {
        # 返回原始目录
        Pop-Location
    }
}

# 执行主函数
Main @args