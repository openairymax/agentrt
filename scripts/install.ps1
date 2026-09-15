
param(
    [string]$Prefix,
    [string]$Mode = "auto",
    [string]$BinDir,
    [string]$Channel = "stable",
    [string]$FromFile,
    [switch]$Uninstall,
    [switch]$KeepData,
    [switch]$Yes,
    [switch]$Help
)

$ErrorActionPreference = "Stop"

function Write-Info  { Write-Host "[INFO] $args" -ForegroundColor Cyan }
function Write-OK    { Write-Host "[ OK ] $args" -ForegroundColor Green }
function Write-Warn  { Write-Host "[WARN] $args" -ForegroundColor Yellow }
function Write-Err   { Write-Host "[FAIL] $args" -ForegroundColor Red }

if ($Help) {
    Write-Host "AirymaxRT 安装器 (Windows)"
    Write-Host ""
    Write-Host "用法:"
    Write-Host "  一键安装: powershell -ExecutionPolicy Bypass -Command `"irm https://atomgit.com/openairymax/agentrt/releases/download/latest/install.ps1 | iex`""
    Write-Host "  本地执行: powershell -ExecutionPolicy Bypass -File install.ps1 [参数]"
    Write-Host "  更新/重装: 重跑一键安装命令（镜像覆盖最新版，幂等）"
    Write-Host "  卸载:     powershell -ExecutionPolicy Bypass -File install.ps1 -Uninstall [-KeepData] [-Yes]"
    Write-Host ""
    Write-Host "参数:"
    Write-Host "  -Prefix <path>      安装根目录（默认 ~\.airymaxrt）"
    Write-Host "  -Mode <mode>        安装模式: auto|binary|hybrid|source（默认 auto）"
    Write-Host "  -BinDir <path>      可执行文件链接目录（默认 ~\.local\bin）"
    Write-Host "  -Channel <channel>  发布通道: stable|rc|beta（默认 stable）"
    Write-Host "  -FromFile <file>    从本地制品文件安装"
    Write-Host "  -Uninstall          卸载"
    Write-Host "  -KeepData           卸载时保留用户数据"
    Write-Host "  -Yes                跳过交互确认"
    Write-Host "  -Help               显示本帮助"
    exit 0
}

if ($env:AIRY_HOME -and (Join-Path $env:AIRY_HOME "") -ne (Join-Path $HOME ".airymaxrt\")) {
    Write-Warn "已忽略环境变量 AIRY_HOME=$env:AIRY_HOME（防残留劫持）；安装位置统一为 ~\.airymaxrt，非默认位置请用 -Prefix"
}
$AIRY_HOME    = if ($Prefix) { $Prefix } else { Join-Path $HOME ".airymaxrt" }
$AiryVersionSpecified = $false
if ($env:AIRY_VERSION) { $AiryVersionSpecified = $true }
$AIRY_VERSION = if ($env:AIRY_VERSION) { $env:AIRY_VERSION }
                elseif (Test-Path (Join-Path $PSScriptRoot "..\VERSION")) { "v" + ((Get-Content (Join-Path $PSScriptRoot "..\VERSION")).Trim()) }
                else { "v0.1.13" }
$AIRY_REPO_URL = if ($env:AIRY_REPO_URL) { $env:AIRY_REPO_URL } else { "https://atomgit.com/openairymax/airymaxhub.git" }
$AIRY_CHANNEL = if ($Channel) { $Channel } elseif ($env:AIRY_CHANNEL) { $env:AIRY_CHANNEL } else { "stable" }
if (@('stable', 'rc', 'beta') -notcontains $AIRY_CHANNEL) {
    Write-Err "非法 -Channel: $AIRY_CHANNEL（支持 stable|rc|beta）"
    exit 1
}
$AIRY_SRC_DIR = Join-Path $AIRY_HOME "src\airymaxhub"
$MODULES_DIR  = Join-Path $AIRY_HOME "modules"
$BIN_DIR      = if ($BinDir) { $BinDir } elseif ($env:AIRY_BIN_DIR) { $env:AIRY_BIN_DIR } else { Join-Path $HOME ".local\bin" }

function Get-ExpectedDaemons {
    Get-ChildItem (Join-Path $AIRY_HOME "bin\*_d.exe") -ErrorAction SilentlyContinue |
        ForEach-Object { $_.BaseName }
}

function Require-Cmd {
    param([string]$Name)
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        Write-Err "缺少必要工具: $Name"
        if ($Name -eq "git")   { Write-Warn "请安装 Git for Windows: https://git-scm.com/download/win" }
        if ($Name -eq "cmake") { Write-Warn "请安装 CMake ≥3.20: https://cmake.org/download/" }
        if ($Name -eq "curl")  { Write-Warn "Windows 10+ 自带 curl.exe" }
        throw "缺少 $Name"
    }
}

function Check-Toolchain {
    Require-Cmd "git"
    Require-Cmd "cmake"
    $compilers = @("cl","gcc","clang") | Where-Object { Get-Command $_ -ErrorAction SilentlyContinue }
    if (-not $compilers) {
        Write-Err "未找到 C 编译器（MSVC cl / gcc / clang）"
        throw "no C compiler found"
    }
}

function Init-Home {
    foreach ($sub in @("bin","lib","include","share","run","logs","config","data","tmp","cache","modules","scripts",
                       "data\agentrt\logs","data\agentrt\tmp","data\agentrt\cache","data\agentrt\workspaces")) {
        New-Item -ItemType Directory -Force -Path (Join-Path $AIRY_HOME $sub) | Out-Null
    }
    Write-OK "AIRY_HOME 就绪: $AIRY_HOME"
}

function Stop-Daemons {
    foreach ($name in (Get-ExpectedDaemons)) {
        Get-Process -Name $name -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    }
    Start-Sleep -Seconds 1
}

function Uninstall-All {
    param([string]$Home, [switch]$KeepData, [switch]$Yes)
    $envFile = Join-Path $Home "config\install.env"
    if (Test-Path $envFile) {
        $line = (Get-Content $envFile | Where-Object { $_ -like "AIRY_HOME=*" } | Select-Object -First 1)
        if ($line) { $Home = $line.Substring($line.IndexOf("=") + 1) }
    }
    if (-not (Test-Path $Home)) { Write-Warn "未检测到安装（$Home 不存在），无需卸载"; return }
    $link = ""
    if (Test-Path $envFile) {
        $line = (Get-Content $envFile | Where-Object { $_ -like "AIRY_BIN_LINK=*" } | Select-Object -First 1)
        if ($line) { $link = $line.Substring($line.IndexOf("=") + 1) }
    }
    if (-not $link) { $link = Join-Path $BIN_DIR "airymaxrt.cmd" }

    Write-Warn "将卸载 AirymaxRT：$Home"
    if (-not $Yes) {
        $ans = Read-Host "确认卸载？[y/N]"
        if ($ans -notin @("y","Y","yes","YES")) { Write-Info "已取消卸载"; return }
    }
    Stop-Daemons
    if ($KeepData -and (Test-Path (Join-Path $Home "data"))) {
        Remove-Item -Recurse -Force $Home
        New-Item -ItemType Directory -Force -Path (Join-Path $Home "data") | Out-Null
        Write-OK "已删除 $Home（保留 data/ 记忆数据）"
    } else {
        Remove-Item -Recurse -Force $Home
        Write-OK "已删除 $Home"
    }
    if (Test-Path $link) { Remove-Item -Force $link; Write-OK "已移除启动器 $link" }
    Write-OK "卸载完成"
}

if ($Uninstall) {
    Uninstall-All -Home $AIRY_HOME -KeepData:$KeepData -Yes:$Yes
    exit 0
}

function Fetch-RepoFile {
    param([string]$RepoPath, [string]$Dest)
    $api = "https://api.atomgit.com/api/v5/repos/openairymax/agentrt/contents/$RepoPath?ref=main"
    try {
        $resp = Invoke-RestMethod -Uri $api -Headers @{ "User-Agent" = "agentrt-installer" } -TimeoutSec 60
        if ($null -eq $resp -or $null -eq $resp.content) { return $false }
        $bytes = [Convert]::FromBase64String(($resp.content -replace "\s", ""))
        [System.IO.File]::WriteAllBytes($Dest, $bytes)
        return (Test-Path $Dest) -and ((Get-Item $Dest).Length -gt 0)
    } catch { return $false }
}

function Install-Binary {
    param([string]$Url)
    $zip = Join-Path $AIRY_HOME "tmp\agentrt-$AIRY_VERSION.zip"
    $expectSha = ""
    if ($Url -like "*.json") {
        $man = Join-Path $AIRY_HOME "tmp\manifest.json"
        if (Test-Path $Url) {
            Copy-Item $Url $man -Force
            Write-Info "使用已获取的通道 manifest"
        } else {
            Write-Info "下载通道 manifest: $Url"
            curl.exe -fsSL --max-time 60 -o $man $Url
            if ($LASTEXITCODE -ne 0) { Write-Warn "manifest 下载失败，回退源码构建"; return $false }
        }
        $asc = Join-Path $AIRY_HOME "tmp\manifest.json.asc"
        $ascSrc = "$Url.asc"
        if (Test-Path $ascSrc) { Copy-Item $ascSrc $asc -Force }
        else { curl.exe -fsSL --max-time 30 -o $asc $ascSrc 2>$null }
        if ((Get-Command gpg -ErrorAction SilentlyContinue)) {
            $keyf = Join-Path $AIRY_HOME "tmp\agentrt.asc"
            if (-not (Fetch-RepoFile "latest/keys/agentrt.asc" $keyf)) {
                if (Test-Path (Join-Path $AIRY_HOME "keys\agentrt.asc")) {
                    Copy-Item (Join-Path $AIRY_HOME "keys\agentrt.asc") $keyf -Force
                }
            }
            if (-not (Test-Path $keyf)) {
                Write-Warn "发布公钥拉取失败，拒绝安装（fail-closed）"
                $script:BinaryFatal = $true
                return $false
            }
            gpg --batch --import $keyf 2>$null
            gpg --batch --verify $asc $man 2>$null
            if ($LASTEXITCODE -ne 0) {
                Write-Err "manifest 验签失败（GPG），拒绝安装"
                $script:BinaryFatal = $true
                return $false
            }
            Write-OK "manifest 验签通过（GPG）"
        } elseif (-not (Test-Path $asc)) {
            Write-Err "manifest 签名缺失且无 gpg 环境，拒绝安装（fail-closed）"
            $script:BinaryFatal = $true
            return $false
        }
        $plat = switch ($env:PROCESSOR_ARCHITECTURE) {
            'AMD64' { 'windows-x86-64' }
            'ARM64' { 'windows-arm-64' }
            'x86'   { 'windows-x86-32' }
            default { $null }
        }
        if (-not $plat) {
            Write-Warn "不支持的处理器架构 $($env:PROCESSOR_ARCHITECTURE)，回退源码构建"; return $false
        }
        $json = Get-Content $man -Raw | ConvertFrom-Json
        if ($json.state -eq 'reserved') {
            Write-Err "通道 $AIRY_CHANNEL 为保留通道（state=reserved），官方尚未发布任何制品"
            Write-Err "当前可用：stable（生产）/ rc（候选）——请改用 -Channel stable"
            $script:BinaryFatal = $true
            return $false
        }
        $art = $json.releases.($json.latest).artifacts.$plat
        if (-not $art) {
            foreach ($alt in @('windows-x64', 'win-x86-64', 'win-x64')) {
                $art = $json.releases.($json.latest).artifacts.$alt
                if ($art) { Write-Warn "平台键 $plat 未命中，使用兼容键 $alt"; break }
            }
        }
        if (-not $art -or -not $art.url) { Write-Warn "manifest 无 $plat 制品，回退源码构建"; return $false }
        $Url = $art.url
        $expectSha = [string]$art.sha256
        Write-Info "通道 $AIRY_CHANNEL 最新制品（$plat）: $($Url.Split('/')[-1])"
    }
    Get-ChildItem (Join-Path $AIRY_HOME "tmp") -Directory -Filter "agentrt-*" -ErrorAction SilentlyContinue |
        Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
    if (Test-Path $Url) {
        Write-Info "使用本地离线包: $Url"
        $zip = $Url
    } else {
        Write-Info "下载完全体二进制包: $Url"
        curl.exe -fsSL --max-time 600 -o $zip $Url
        if ($LASTEXITCODE -ne 0) {
            Write-Err "release 下载失败，拒绝回退源码构建（fail-closed）"
            $script:BinaryFatal = $true
            return $false
        }
    }
    if (-not $expectSha -and (Test-Path "$zip.sha256")) {
        $expectSha = (Get-Content "$zip.sha256").Split(' ')[0]
    }
    if ($expectSha) {
        $hash = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLower()
        if ($hash -ne $expectSha.ToLower()) {
            Write-Err "sha256 校验失败，拒绝安装"
            $script:BinaryFatal = $true
            return $false
        }
        Write-OK "sha256 校验通过"
    }
    Expand-Archive -Path $zip -DestinationPath (Join-Path $AIRY_HOME "tmp") -Force
    $pkgDir = Get-ChildItem (Join-Path $AIRY_HOME "tmp") -Directory | Where-Object { $_.Name -like "agentrt-*" } | Select-Object -First 1
    if (-not $pkgDir) {
        Write-Err "release 包结构异常（缺 agentrt-* 顶层目录）"
        $script:BinaryFatal = $true
        return $false
    }
    Stop-Daemons
    $binDst = Join-Path $AIRY_HOME "bin"
    New-Item -ItemType Directory -Force -Path $binDst | Out-Null
    $pkgBinItems = Get-ChildItem (Join-Path $pkgDir.FullName "bin") -ErrorAction SilentlyContinue
    if (-not $pkgBinItems) {
        Write-Err "release 包 bin/ 为空，拒绝安装（制品不完整）"
        $script:BinaryFatal = $true
        return $false
    }
    foreach ($rel in @("bin", "lib", "include", "share")) {
        $pkgSub = Join-Path $pkgDir.FullName $rel
        if (Test-Path $pkgSub) {
            $dstSub = Join-Path $AIRY_HOME $rel
            if (Test-Path $dstSub) {
                Get-ChildItem $dstSub -Force -ErrorAction SilentlyContinue |
                    Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
            }
        }
    }
    foreach ($item in $pkgBinItems) {
        try {
            Copy-Item $item.FullName $binDst -Force -ErrorAction Stop
        } catch {
            Write-Err "bin/ 拷贝失败: $($item.Name) — $($_.Exception.Message)"
            $script:BinaryFatal = $true
            return $false
        }
        if (-not (Test-Path (Join-Path $binDst $item.Name))) {
            Write-Err "bin/ 部署校验失败，缺失: $($item.Name)（检查磁盘/权限）"
            $script:BinaryFatal = $true
            return $false
        }
    }
    Get-ChildItem (Join-Path $pkgDir.FullName "lib") -ErrorAction SilentlyContinue | Copy-Item -Destination (Join-Path $AIRY_HOME "lib") -Recurse -Force
    if (Get-ChildItem (Join-Path $pkgDir.FullName "lib") -Filter "*.dll" -ErrorAction SilentlyContinue) {
        if (-not (Get-ChildItem (Join-Path $AIRY_HOME "lib") -Filter "*.dll" -ErrorAction SilentlyContinue)) {
            Write-Err "lib/ 部署失败（.dll 未就位），二进制将无法启动"
            $script:BinaryFatal = $true
            return $false
        }
    }
    Get-ChildItem (Join-Path $pkgDir.FullName "include") -ErrorAction SilentlyContinue | Copy-Item -Destination (Join-Path $AIRY_HOME "include") -Recurse -Force
    Get-ChildItem (Join-Path $pkgDir.FullName "share") -ErrorAction SilentlyContinue | Copy-Item -Destination (Join-Path $AIRY_HOME "share") -Recurse -Force
    Get-ChildItem (Join-Path $pkgDir.FullName "config") -ErrorAction SilentlyContinue | Copy-Item -Destination (Join-Path $AIRY_HOME "config") -Force
    if (Test-Path (Join-Path $pkgDir.FullName "keys\agentrt.asc")) {
        New-Item -ItemType Directory -Force -Path (Join-Path $AIRY_HOME "keys") | Out-Null
        Copy-Item (Join-Path $pkgDir.FullName "keys\agentrt.asc") (Join-Path $AIRY_HOME "keys") -Force
    }
    $verNum = $pkgDir.Name -replace "^agentrt-", ""
    if ($verNum) { $script:AIRY_VERSION = "v$verNum" }
    Write-OK "完全体二进制包安装完成（v$verNum）"
    return $true
}

function Fetch-PrebuiltModule {
    param([string]$Name, [string]$Url, [string]$DirName)
    if (-not $Url) { Write-Warn "未配置 $Name 预编译包 URL，跳过"; return }
    $dest = Join-Path $MODULES_DIR $DirName
    if (Test-Path $dest) { Write-OK "$Name 预编译模块已就位"; return }
    Write-Info "下载闭源预编译模块 $Name…"
    $zip = Join-Path $AIRY_HOME "tmp\$DirName.zip"
    curl.exe -fsSL --max-time 600 -o $zip $Url
    if ($LASTEXITCODE -ne 0) { Write-Warn "$Name 下载失败"; return }
    New-Item -ItemType Directory -Force -Path $dest | Out-Null
    Expand-Archive -Path $zip -DestinationPath $dest -Force
    Write-OK "$Name 预编译模块就位: $dest"
}

function Build-FromSource {
    if (-not (Test-Path (Join-Path $AIRY_SRC_DIR ".git"))) {
        Write-Info "git 拉取 airymaxhub（$AIRY_REPO_URL）…"
        New-Item -ItemType Directory -Force -Path (Split-Path $AIRY_SRC_DIR) | Out-Null
        if ($AiryVersionSpecified) {
            git clone --depth 1 -b $AIRY_VERSION $AIRY_REPO_URL $AIRY_SRC_DIR
        } else {
            git clone --depth 1 $AIRY_REPO_URL $AIRY_SRC_DIR
        }
        if ($LASTEXITCODE -ne 0) { Write-Err "git 拉取失败（子仓私有时请配置 AIRY_RELEASE_URL 走二进制模式）"; throw "git clone failed" }
        git -C $AIRY_SRC_DIR submodule update --init --recursive --depth 1 2>$null
    } else {
        Write-Info "airymaxhub 源码已存在，复用本地源码树"
    }
    $srcApp = Join-Path $AIRY_SRC_DIR "agent-workload"
    if (-not (Test-Path (Join-Path $srcApp "agentrt\VERSION"))) { $srcApp = $AIRY_SRC_DIR }
    $verFile = Join-Path $srcApp "agentrt\VERSION"
    if (Test-Path $verFile) {
        $realVer = (Get-Content $verFile -Raw).Trim()
        if ($realVer) {
            $AIRY_VERSION = "v$realVer"
            Write-OK "源码版本（SSoT）: $AIRY_VERSION"
        }
    }
    Write-OK "源码就绪: $AIRY_SRC_DIR"

    $buildDir = Join-Path $AIRY_HOME "build"
    $cmakeArgs = "-DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF -DENABLE_SANITIZERS=OFF -DCMAKE_INSTALL_PREFIX=$AIRY_HOME"
    if (Test-Path (Join-Path $MODULES_DIR "atoms")) {
        $cmakeArgs += " -DAIRY_ATOMS_PREBUILT_DIR=$(Join-Path $MODULES_DIR 'atoms')"
    }
    $mrLibName = "libagentrt_memoryrovol.lib"
    $mrLibA = Join-Path $MODULES_DIR "memoryrovol\libagentrt_memoryrovol.a"
    $mrLib = Join-Path $MODULES_DIR "memoryrovol\$mrLibName"
    if (-not (Test-Path $mrLib) -and (Test-Path $mrLibA)) {
        $mrLib = $mrLibA
    }
    if (Test-Path $mrLib) { $cmakeArgs += " -DMEMORYROVOL_PRO_LIB=$mrLib" }

    Write-Info "cmake 配置…"
    cmake -S (Join-Path $AIRY_SRC_DIR "agentrt") -B $buildDir $cmakeArgs
    if ($LASTEXITCODE -ne 0) { Write-Err "cmake 配置失败"; throw "cmake configure failed" }

    Write-Info "构建…"
    cmake --build $buildDir --config Release --parallel
    if ($LASTEXITCODE -ne 0) { Write-Err "构建失败"; throw "build failed" }

    Write-Info "安装到 $AIRY_HOME…"
    cmake --install $buildDir --config Release
    $binDir = Join-Path $buildDir "bin\Release"
    if (Test-Path $binDir) { Copy-Item (Join-Path $binDir "*") (Join-Path $AIRY_HOME "bin") -Recurse -Force -ErrorAction SilentlyContinue }
    Write-OK "源码构建安装完成"
}

function Init-Secrets {
    $secrets = Join-Path $AIRY_HOME "config\secrets.env"
    if (-not (Test-Path $secrets)) {
        $template = Join-Path $AIRY_SRC_DIR "tools\scripts\ops\templates\secrets.env.example"
        if (-not (Test-Path $template)) { $template = Join-Path $AIRY_HOME "config\secrets.env.example" }
        if (Test-Path $template) {
            Copy-Item $template $secrets -Force
            Write-Warn "已生成 $secrets，请填写 LLM API key"
        } else {
            Write-Warn "未找到 secrets.env 模板，跳过"
        }
    } else {
        Write-OK "secrets.env 已存在，跳过"
    }
    $srcYaml = Join-Path $AIRY_SRC_DIR "ecosystem\manager\configs\agentrt.yaml"
    if (Test-Path $srcYaml) { Copy-Item $srcYaml (Join-Path $AIRY_HOME "config") -Force -ErrorAction SilentlyContinue }
    $srcModel = Join-Path $AIRY_SRC_DIR "ecosystem\manager\model\model.yaml"
    if (Test-Path $srcModel) { Copy-Item $srcModel (Join-Path $AIRY_HOME "config") -Force -ErrorAction SilentlyContinue }
}

function Finalize-Install {
    $envFile = Join-Path $AIRY_HOME "config\install.env"
    $link = Join-Path $BIN_DIR "airymaxrt.cmd"
    $vaultPassword = -join (1..64 | ForEach-Object { '{0:x}' -f (Get-Random -Maximum 16) })
    @(
        "# AirymaxRT 安装信息（由 install.ps1 生成，勿手改）",
        "AIRY_HOME=$AIRY_HOME",
        "AIRY_VERSION=$AIRY_VERSION",
        "AIRY_CHANNEL=$AIRY_CHANNEL",
        "AIRY_BIN_LINK=$link",
        "INSTALLED_AT=$(Get-Date -Format o)",
        "AIRY_VAULT_PASSWORD=$vaultPassword"
    ) | Set-Content -Path $envFile -Encoding UTF8

    $aclTools = "fs_read,fs_write,fs_list,fs_glob,fs_grep,fs_edit,fs_delete,shell_run,web_search,web_fetch,git_diff,git_exec,git_apply"
    $agents = @("coding_v1","devops_v1","backend_v1","frontend_v1","tester_v1","architect_v1",
                "product_manager_v1","data_engineer_v1","security_v1","reviewer_v1","analyst_v1")
    $aclDefault = ($agents | ForEach-Object { "$_=$aclTools" }) -join ";"
    $envScript = @(
        "# AgentRT 运行环境（由 install.ps1 生成，source 使用）",
        ('$env:AIRY_HOME = "' + $AIRY_HOME + '"'),
        'if (-not $env:AIRY_RUNTIME_DIR) { $env:AIRY_RUNTIME_DIR = Join-Path $env:AIRY_HOME "run" }',
        'if (-not $env:AIRY_DATA_DIR) { $env:AIRY_DATA_DIR = Join-Path $env:AIRY_HOME "data" }',
        'if (-not $env:AIRY_LOG_DIR) { $env:AIRY_LOG_DIR = Join-Path $env:AIRY_HOME "data\agentrt\logs" }',
        'if (-not $env:AIRY_CACHE_DIR) { $env:AIRY_CACHE_DIR = Join-Path $env:AIRY_HOME "data\agentrt\cache" }',
        'if (-not $env:AIRY_TMP_DIR) { $env:AIRY_TMP_DIR = Join-Path $env:AIRY_HOME "data\agentrt\tmp" }',
        'if (-not $env:AIRY_WORKSPACE_DIR) { $env:AIRY_WORKSPACE_DIR = Join-Path $env:AIRY_HOME "data\agentrt\workspaces" }',
        'if (-not $env:AIRY_CONFIG_DIR) { $env:AIRY_CONFIG_DIR = Join-Path $env:AIRY_HOME "config" }',
        'if (-not $env:AIRY_BIN_DIR) { $env:AIRY_BIN_DIR = Join-Path $env:AIRY_HOME "bin" }',
        'if (-not $env:AIRY_LIB_DIR) { $env:AIRY_LIB_DIR = Join-Path $env:AIRY_HOME "lib" }',
        ('if (-not $env:AIRY_AGENT_ACL) { $env:AIRY_AGENT_ACL = "' + $aclDefault + '" }'),
        '$env:PATH = (Join-Path $env:AIRY_HOME "bin") + [IO.Path]::PathSeparator + $env:PATH'
    )
    $envScript | Set-Content -Path (Join-Path $AIRY_HOME "bin\agentrt-env.ps1") -Encoding UTF8

    $launcher = Join-Path $AIRY_HOME "bin\airymaxrt.cmd"
    $escapedHome = $AIRY_HOME.Replace('"','""')
    $cmdContent = @(
        "@echo off",
        "setlocal",
        ("set ""AIRY_HOME={0}""" -f $escapedHome),
        "if exist ""%AIRY_HOME%\config\install.env"" (",
        "  for /f ""tokens=2 delims=="" %%a in ('findstr /b ""AIRY_HOME="" ""%AIRY_HOME%\config\install.env"" 2^>nul') do set ""AIRY_HOME=%%a""",
        ")",
        "if /i ""%~1""==""uninstall"" (",
        "  powershell -NoProfile -ExecutionPolicy Bypass -Command ""$ErrorActionPreference='Stop'; try { $c=irm 'https://api.atomgit.com/api/v5/repos/openairymax/agentrt/contents/scripts/install.ps1?ref=main' -TimeoutSec 60; $p=Join-Path $env:TEMP 'agentrt-install.ps1'; [IO.File]::WriteAllBytes($p,[Convert]::FromBase64String(($c.content -replace '\\s',''))); & $p -Uninstall -Prefix '%AIRY_HOME%' } catch { Write-Host ('[FAIL] 卸载器自举失败: '+$_.Exception.Message); exit 1 }""",
        "  goto :eof",
        ")",
        "if not exist ""%AIRY_HOME%\bin\agentrt-tui.exe"" goto :notfound",
        "  ""%AIRY_HOME%\bin\agentrt-tui.exe"" %*",
        "  goto :eof",
        ":notfound",
        "if exist ""%AIRY_HOME%\bin\airy_cli.exe"" (",
        "  ""%AIRY_HOME%\bin\airy_cli.exe"" %*",
        ") else (",
        "  echo [FAIL] agentrt-tui / airy_cli not found under %AIRY_HOME%\bin",
        "  exit /b 1",
        ")",
        "endlocal"
    )
    $cmdContent | Set-Content -Path $launcher -Encoding ASCII

    New-Item -ItemType Directory -Force -Path $BIN_DIR | Out-Null
    Copy-Item $launcher $link -Force
    Write-OK "启动器: $link → $launcher"

    Copy-Item $MyInvocation.MyCommand.Path (Join-Path $AIRY_HOME "scripts\install.ps1") -Force -ErrorAction SilentlyContinue
    Write-OK "安装位置已固化: install.env + airymaxrt.cmd"
}

function Verify-Daemons {
    param([switch]$Strict)
    $list = @(Get-ExpectedDaemons)
    if ($list.Count -eq 0) {
        if ($Strict) {
            Write-Err "daemon 校验失败：bin\*_d.exe 不存在（制品不完整）"
            exit 1
        }
        Write-Warn "daemon 校验未全通过：bin\*_d.exe 不存在"
        return
    }
    $missing = @()
    foreach ($d in $list) {
        if (-not (Test-Path (Join-Path $AIRY_HOME "bin\$d.exe"))) { $missing += $d }
    }
    if ($missing.Count -gt 0) {
        if ($Strict) {
            Write-Err "daemon 校验失败，缺失: $($missing -join ' ')（二进制包不完整，请检查 release 制品）"
            exit 1
        }
        Write-Warn "daemon 校验未全通过，缺失: $($missing -join ' ')"
    } else {
        Write-OK "$($list.Count) 个 daemon 全部就位"
    }
}

Write-Host ""
Write-Host "  ┌─────────────────────────────────────────────────────┐" -ForegroundColor Cyan
Write-Host "  │         Airymax Agent Platform Engineering          │" -ForegroundColor Cyan
Write-Host "  │=====================================================│" -ForegroundColor Cyan
Write-Host "  │     Runtime · Frame · SpuerAgent · All-in-one       │" -ForegroundColor Cyan
Write-Host "  │=====================================================│" -ForegroundColor Cyan
Write-Host "  │ `"Agents, To the open air. To OpenAirymax. To hope.`" │" -ForegroundColor Cyan
Write-Host "  └─────────────────────────────────────────────────────┘" -ForegroundColor Cyan
Write-Host ""

Write-Info "Airymax AgentRT 安装程序"
Write-Info "AIRY_HOME = $AIRY_HOME | 模式 = $Mode"

Require-Cmd "curl"
Init-Home

$installed = $false
$script:BinaryFatal = $false
$releaseUrl = $env:AIRY_RELEASE_URL
if (-not $releaseUrl -and $Mode -ne "source") {
    $manLocal = Join-Path $AIRY_HOME "tmp\manifest.$AIRY_CHANNEL.json"
    if (Fetch-RepoFile "latest/manifest.$AIRY_CHANNEL.json" $manLocal) {
        Fetch-RepoFile "latest/manifest.$AIRY_CHANNEL.json.asc" "$manLocal.asc" | Out-Null
        $releaseUrl = $manLocal
        Write-Info "通道 manifest 已获取（$AIRY_CHANNEL）"
    } else {
        Write-Warn "通道 manifest 获取失败：可能是网络/服务异常，也可能该通道暂无制品"
        Write-Warn "当前可用：stable（生产）/ rc（候选），beta 为保留通道；将回退源码构建"
        $releaseUrl = ""
    }
}
if ($FromFile) {
    if (-not (Install-Binary $FromFile)) {
        Write-Err "离线包安装失败，退出（fail-closed）"
        exit 1
    }
    $installed = $true
}
elseif ($Mode -eq "binary" -or ($Mode -eq "auto" -and $releaseUrl)) {
    $installed = Install-Binary $releaseUrl
}
elseif ($Mode -eq "binary") { Write-Err "模式 binary 需要 AIRY_RELEASE_URL"; exit 1 }

if (-not $installed) {
    if ($Mode -eq "binary" -or $script:BinaryFatal) {
        Write-Err "二进制安装失败，拒绝回退源码构建（fail-closed）"
        exit 1
    }
    Write-Info "进入源码构建模式（$Mode）"
    Check-Toolchain
    if ($Mode -ne "source") {
        Fetch-PrebuiltModule "atoms" $env:AIRY_ATOMS_PREBUILT_URL "atoms"
        Fetch-PrebuiltModule "memoryrovol" $env:AIRY_MEMORYROVOL_PREBUILT_URL "memoryrovol"
    }
    Build-FromSource
}

Init-Secrets
Finalize-Install
if ($installed) { Verify-Daemons -Strict } else { Verify-Daemons }

Write-Host ""
Write-Host "安装位置:   $AIRY_HOME" -ForegroundColor Green
Write-Host "可执行文件: $AIRY_HOME\bin\" -ForegroundColor Green
Write-Host "启动器:     $BIN_DIR\airymaxrt.cmd（任意路径输入 airymaxrt 即启动）" -ForegroundColor Green
Write-Host "卸载:       install.ps1 -Uninstall 或 airymaxrt.cmd uninstall" -ForegroundColor Green
Write-OK "安装完成"
