# ============================================================================
# Airymax AgentRT 一键安装脚本 (Windows PowerShell)
#
# 位置：agentrt 管理仓 scripts/install.ps1（v0.1.2 起自伞仓 scripts/ 迁移，
#       构建系统与安装器属 IRON-9 [IND] 完全独立层；伞仓保留兼容重定向）。
# 用法：
#   powershell -ExecutionPolicy Bypass -Command "irm https://raw.atomgit.com/openairymax/airymaxhub/raw/main/agentrt/scripts/install.ps1 | iex"
#   powershell -ExecutionPolicy Bypass -File install.ps1 -Prefix "$HOME\.airymaxrt"
#   powershell -ExecutionPolicy Bypass -File install.ps1 -Uninstall -Yes
#
# 安装策略（三模式，与 install.sh 对齐）：
#   模式 A 二进制：AIRY_RELEASE_URL 指向完全体 zip（含闭源模块预编译产物），
#      下载解压到 $AIRY_HOME，秒级安装、无需工具链（完全体二进制为主）。
#   模式 B 混合构建：公开源码编译；闭源模块（atoms/memoryrovol）下载预编译包
#      到 $AIRY_HOME\modules 后链接（AIRY_ATOMS_PREBUILT_DIR /
#      MEMORYROVOL_PRO_LIB）。
#   模式 C 全源码构建：本地持有闭源模块源码，全量编译（-Mode source）。
#
# 路径体系（与 platform.h AIRY_HOME 一致，全产物收敛）：
#   $AIRY_HOME = ${AIRY_HOME:-~/.airymaxrt}（-Prefix 覆盖）
#   bin / lib / include / config / run / logs / data / tmp / cache / modules / scripts
#
# 参数：
#   -Prefix <path>  -Mode <auto|binary|hybrid|source>  -BinDir <path>
#   -Uninstall [-KeepData] [-Yes]  -Help
#
# 卸载：install.ps1 -Uninstall 或 airymaxrt.cmd uninstall（停止 daemon +
#       删除 $AIRY_HOME + 移除启动器；-KeepData 保留记忆数据）。
# ============================================================================

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

# ─── 颜色 ────────────────────────────────────────────────────────────────
function Write-Info  { Write-Host "[INFO] $args" -ForegroundColor Cyan }
function Write-OK    { Write-Host "[ OK ] $args" -ForegroundColor Green }
function Write-Warn  { Write-Host "[WARN] $args" -ForegroundColor Yellow }
function Write-Err   { Write-Host "[FAIL] $args" -ForegroundColor Red }

if ($Help) {
    Get-Content $MyInvocation.MyCommand.Path | Select-Object -First 30 |
        Where-Object { $_ -match "^#" } | ForEach-Object { $_ -replace "^# ?","" }
    exit 0
}

# ─── 参数 ────────────────────────────────────────────────────────────────
$AIRY_HOME    = if ($Prefix) { $Prefix } elseif ($env:AIRY_HOME) { $env:AIRY_HOME } else { Join-Path $HOME ".airymaxrt" }
# 版本 SSoT：显式 AIRY_VERSION 优先；源码树内运行读 agentrt/VERSION；
# 否则回退占位默认值（源码构建路径会以 clone 到的 agentrt/VERSION 为准，
# 二进制路径以 manifest/实际包版本为准）。
$AiryVersionSpecified = $false
if ($env:AIRY_VERSION) { $AiryVersionSpecified = $true }
$AIRY_VERSION = if ($env:AIRY_VERSION) { $env:AIRY_VERSION }
                elseif (Test-Path (Join-Path $PSScriptRoot "..\VERSION")) { "v" + ((Get-Content (Join-Path $PSScriptRoot "..\VERSION")).Trim()) }
                else { "v0.1.5" }
$AIRY_REPO_URL = if ($env:AIRY_REPO_URL) { $env:AIRY_REPO_URL } else { "https://atomgit.com/openairymax/airymaxhub.git" }
$AIRY_CHANNEL = if ($Channel) { $Channel } elseif ($env:AIRY_CHANNEL) { $env:AIRY_CHANNEL } else { "stable" }
$AIRY_SRC_DIR = Join-Path $AIRY_HOME "src\airymaxhub"
$MODULES_DIR  = Join-Path $AIRY_HOME "modules"
$BIN_DIR      = if ($BinDir) { $BinDir } elseif ($env:AIRY_BIN_DIR) { $env:AIRY_BIN_DIR } else { Join-Path $HOME ".local\bin" }

# 与 install.sh 的 18 daemon 清单保持一致（含 think_d/cupolas_d/maths_d）
$EXPECTED_DAEMONS = @("monit_d","observe_d","info_d","notify_d","sched_d","channel_d","mem_d",
                      "llm_d","tool_d","hook_d","plugin_d","agent_d","a2a_d","market_d","gateway_d",
                      "think_d","cupolas_d","maths_d")

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

# 工具链仅在源码构建路径要求（二进制模式无需 git/cmake/编译器）
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
    # 与 18 daemon 清单一致，避免卸载/停止残留进程
    foreach ($name in $EXPECTED_DAEMONS) {
        Get-Process -Name $name -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    }
    Start-Sleep -Seconds 1
}

# ─── 一键卸载 ────────────────────────────────────────────────────────────
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

# ─── 模式 A：完全体二进制 zip（优先） ────────────────────────────────────
function Install-Binary {
    param([string]$Url)
    $zip = Join-Path $AIRY_HOME "tmp\agentrt-$AIRY_VERSION.zip"
    $expectSha = ""
    # 来源解析：a) manifest JSON（通道）→ 解析本平台制品 url+sha256；
    #          b) 本地 zip（-FromFile）→ 直用；c) 远程 zip URL → 下载
    if ($Url -like "*.json") {
        $man = Join-Path $AIRY_HOME "tmp\manifest.json"
        Write-Info "下载通道 manifest: $Url"
        curl.exe -fsSL --max-time 60 -o $man $Url
        if ($LASTEXITCODE -ne 0) { Write-Warn "manifest 下载失败，回退源码构建"; return $false }
        # GPG 验签 manifest（权威校验链，与 install.sh/airymaxrt 同源）：
        #   系统装有 gpg → fail-closed（验签失败拒绝安装）；
        #   无 gpg 环境 → 降级 HTTPS + sha256（Windows 最小环境，显式告警）。
        $asc = Join-Path $AIRY_HOME "tmp\manifest.json.asc"
        curl.exe -fsSL --max-time 30 -o $asc "$Url.asc" 2>$null
        if ((Get-Command gpg -ErrorAction SilentlyContinue)) {
            $keyf = Join-Path $AIRY_HOME "tmp\agentrt.asc"
            curl.exe -fsSL --max-time 30 -o $keyf "https://raw.atomgit.com/openairymax/agentrt/raw/main/latest/keys/agentrt.asc" 2>$null
            if (-not (Test-Path $keyf) -and (Test-Path (Join-Path $AIRY_HOME "keys\agentrt.asc"))) {
                Copy-Item (Join-Path $AIRY_HOME "keys\agentrt.asc") $keyf -Force
            }
            if (-not (Test-Path $keyf)) {
                Write-Warn "发布公钥拉取失败，拒绝安装（fail-closed）"
                return $false
            }
            gpg --batch --import $keyf 2>$null
            gpg --batch --verify $asc $man 2>$null
            if ($LASTEXITCODE -ne 0) {
                Write-Err "manifest 验签失败（GPG），拒绝安装"
                return $false
            }
            Write-OK "manifest 验签通过（GPG）"
        } elseif (-not (Test-Path $asc)) {
            Write-Warn "manifest 签名缺失且无 gpg 环境（降级：仅 HTTPS + sha256）"
        }
        $plat = "windows-" + $env:PROCESSOR_ARCHITECTURE.ToLower()
        if ($plat -eq "windows-amd64") { $plat = "windows-x64" }
        $json = Get-Content $man -Raw | ConvertFrom-Json
        $art = $json.releases.($json.latest).artifacts.$plat
        if (-not $art -or -not $art.url) { Write-Warn "manifest 无 $plat 制品，回退源码构建"; return $false }
        $Url = $art.url
        $expectSha = [string]$art.sha256
        Write-Info "通道 $AIRY_CHANNEL 最新制品（$plat）: $($Url.Split('/')[-1])"
    }
    if (Test-Path $Url) {
        Write-Info "使用本地离线包: $Url"
        $zip = $Url
    } else {
        Write-Info "下载完全体二进制包: $Url"
        curl.exe -fsSL --max-time 600 -o $zip $Url
        if ($LASTEXITCODE -ne 0) { Write-Warn "release 下载失败，回退源码构建"; return $false }
    }
    # sha256 校验（manifest 期望值，或相邻 .sha256 文件）
    if (-not $expectSha -and (Test-Path "$zip.sha256")) {
        $expectSha = (Get-Content "$zip.sha256").Split(' ')[0]
    }
    if ($expectSha) {
        $hash = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLower()
        if ($hash -ne $expectSha.ToLower()) {
            Write-Err "sha256 校验失败，拒绝安装"
            return $false
        }
        Write-OK "sha256 校验通过"
    }
    Expand-Archive -Path $zip -DestinationPath (Join-Path $AIRY_HOME "tmp") -Force
    # 包内必须含 agentrt-* 顶层目录（release.yml 打包约定），否则视为异常
    $pkgDir = Get-ChildItem (Join-Path $AIRY_HOME "tmp") -Directory | Where-Object { $_.Name -like "agentrt-*" } | Select-Object -First 1
    if (-not $pkgDir) { Write-Warn "release 包结构异常（缺 agentrt-* 顶层目录），回退源码构建"; return $false }
    $copied = 0
    Get-ChildItem (Join-Path $pkgDir.FullName "bin") -ErrorAction SilentlyContinue | ForEach-Object {
        Copy-Item $_.FullName (Join-Path $AIRY_HOME "bin") -Force -ErrorAction SilentlyContinue
        $copied++
    }
    Get-ChildItem (Join-Path $pkgDir.FullName "lib") -ErrorAction SilentlyContinue | Copy-Item -Destination (Join-Path $AIRY_HOME "lib") -Recurse -Force
    Get-ChildItem (Join-Path $pkgDir.FullName "include") -ErrorAction SilentlyContinue | Copy-Item -Destination (Join-Path $AIRY_HOME "include") -Recurse -Force
    # LICENSE/README（share/）随包分发；config/ 内置模板（secrets.env.example 等）
    Get-ChildItem (Join-Path $pkgDir.FullName "share") -ErrorAction SilentlyContinue | Copy-Item -Destination (Join-Path $AIRY_HOME "share") -Recurse -Force
    Get-ChildItem (Join-Path $pkgDir.FullName "config") -ErrorAction SilentlyContinue | Copy-Item -Destination (Join-Path $AIRY_HOME "config") -Force
    # 签名公钥随包同步
    if (Test-Path (Join-Path $pkgDir.FullName "keys\agentrt.asc")) {
        New-Item -ItemType Directory -Force -Path (Join-Path $AIRY_HOME "keys") | Out-Null
        Copy-Item (Join-Path $pkgDir.FullName "keys\agentrt.asc") (Join-Path $AIRY_HOME "keys") -Force
    }
    # 以实际安装包版本固化（manifest 通道可能高于默认 AIRY_VERSION）
    $verNum = $pkgDir.Name -replace "^agentrt-", ""
    if ($verNum) { $script:AIRY_VERSION = "v$verNum" }
    if ($copied -eq 0) { Write-Warn "release 包 bin/ 为空，回退源码构建"; return $false }
    Write-OK "完全体二进制包安装完成（v$verNum）"
    return $true
}

# ─── 闭源预编译模块下载（模式 B） ────────────────────────────────────────
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

# ─── 源码获取 + 构建（模式 B/C） ─────────────────────────────────────────
function Build-FromSource {
    if (-not (Test-Path (Join-Path $AIRY_SRC_DIR ".git"))) {
        Write-Info "git 拉取 airymaxhub（$AIRY_REPO_URL）…"
        New-Item -ItemType Directory -Force -Path (Split-Path $AIRY_SRC_DIR) | Out-Null
        # 版本来源二选一：显式 AIRY_VERSION → 固定 tag 精确安装；
        # 未指定 → clone 默认分支，随后从 agentrt/VERSION 读取真实版本
        # （SSoT 单一来源），杜绝脚本内置默认版本与当前发布漂移。
        if ($AiryVersionSpecified) {
            git clone --depth 1 -b $AIRY_VERSION $AIRY_REPO_URL $AIRY_SRC_DIR
        } else {
            git clone --depth 1 $AIRY_REPO_URL $AIRY_SRC_DIR
        }
        if ($LASTEXITCODE -ne 0) { Write-Err "git 拉取失败（子仓私有时请配置 AIRY_RELEASE_URL 走二进制模式）"; throw "git clone failed" }
        # --recursive：agentrt 的 7 个核心子仓（atoms/commons/daemons/gateway/
        # cupolas/protocols/heapstore）与 sdk/ecosystem 子仓均为公开仓，必须
        # 一并拉取，否则模式 C 源码构建缺核心源码必然失败。闭源子仓（标
        # update=none）自动跳过。
        git -C $AIRY_SRC_DIR submodule update --init --recursive --depth 1 2>$null
    } else {
        Write-Info "airymaxhub 源码已存在，复用本地源码树"
    }
    # 源码版本 SSoT：以 agentrt/VERSION 为权威（兼容管理仓 submodule 布局）
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
    # 预编译库文件名：Windows 为 .lib（MSVC 静态库），POSIX 为 .a。
    # 与 install.sh 及 products/memoryrovol 的归档命名对齐。
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

# ─── secrets.env 模板（源码模式用 tools 模板，二进制模式回退随包 config/） ─
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
    # agentrt.yaml / model.yaml（二进制模式已由 Install-Binary 拷入 config/）
    $srcYaml = Join-Path $AIRY_SRC_DIR "ecosystem\manager\configs\agentrt.yaml"
    if (Test-Path $srcYaml) { Copy-Item $srcYaml (Join-Path $AIRY_HOME "config") -Force -ErrorAction SilentlyContinue }
    $srcModel = Join-Path $AIRY_SRC_DIR "ecosystem\manager\model\model.yaml"
    if (Test-Path $srcModel) { Copy-Item $srcModel (Join-Path $AIRY_HOME "config") -Force -ErrorAction SilentlyContinue }
}

# ─── 固化安装位置 + 生成运行环境 + 启动器 ────────────────────────────────
function Finalize-Install {
    $envFile = Join-Path $AIRY_HOME "config\install.env"
    $link = Join-Path $BIN_DIR "airymaxrt.cmd"
    # vault 主密钥口令（AES-256-GCM 凭据加密，64 位 hex），对齐 install.sh
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

    # 运行环境脚本（agentrt-env.ps1，含 Agent 工具 ACL 预授权，
    # fail-closed：无 AIRY_AGENT_ACL 时 agent 工具全部拒绝，对齐 install.sh）
    # 路径体系与 install.sh / platform.h 一致（2026-08-25 铁律）：运行时
    # 数据全量统一 $AIRY_HOME\data\agentrt（日志/缓存/临时/工作区），
    # 顶层仅保留分发物、配置与易失 run/。此前 Windows 版把日志放顶层
    # logs\、缓存放 cache\，与 daemon airy_paths_init 实际路径分叉。
    $aclTools = "fs_read,fs_write,fs_list,fs_glob,fs_grep,fs_edit,fs_delete,shell_run,web_search,web_fetch,git_diff,git_exec,git_apply"
    $agents = @("coding_v1","devops_v1","backend_v1","frontend_v1","tester_v1","architect_v1",
                "product_manager_v1","data_engineer_v1","security_v1","reviewer_v1","analyst_v1")
    $aclDefault = ($agents | ForEach-Object { "$_=$aclTools" }) -join ";"
    $envScript = @(
        "# AgentRT 运行环境（由 install.ps1 生成，source 使用）",
        ('$env:AIRY_HOME = "' + $AIRY_HOME + '"'),
        # PowerShell 5.1 兼容：避免 ?? 运算符（PS7+ 才有）
        'if (-not $env:AIRY_RUNTIME_DIR) { $env:AIRY_RUNTIME_DIR = Join-Path $env:AIRY_HOME "run" }',
        'if (-not $env:AIRY_DATA_DIR) { $env:AIRY_DATA_DIR = Join-Path $env:AIRY_HOME "data" }',
        'if (-not $env:AIRY_LOG_DIR) { $env:AIRY_LOG_DIR = Join-Path $env:AIRY_HOME "data\agentrt\logs" }',
        'if (-not $env:AIRY_CACHE_DIR) { $env:AIRY_CACHE_DIR = Join-Path $env:AIRY_HOME "data\agentrt\cache" }',
        'if (-not $env:AIRY_TMP_DIR) { $env:AIRY_TMP_DIR = Join-Path $env:AIRY_HOME "data\agentrt\tmp" }',
        'if (-not $env:AIRY_WORKSPACE_DIR) { $env:AIRY_WORKSPACE_DIR = Join-Path $env:AIRY_HOME "data\agentrt\workspaces" }',
        'if (-not $env:AIRY_CONFIG_DIR) { $env:AIRY_CONFIG_DIR = Join-Path $env:AIRY_HOME "config" }',
        'if (-not $env:AIRY_BIN_DIR) { $env:AIRY_BIN_DIR = Join-Path $env:AIRY_HOME "bin" }',
        'if (-not $env:AIRY_LIB_DIR) { $env:AIRY_LIB_DIR = Join-Path $env:AIRY_HOME "lib" }',
        "# Agent 工具 ACL 预授权（fail-closed：无此变量时 agent 工具全部拒绝）",
        ('if (-not $env:AIRY_AGENT_ACL) { $env:AIRY_AGENT_ACL = "' + $aclDefault + '" }'),
        '$env:PATH = (Join-Path $env:AIRY_HOME "bin") + [IO.Path]::PathSeparator + $env:PATH'
    )
    $envScript | Set-Content -Path (Join-Path $AIRY_HOME "bin\agentrt-env.ps1") -Encoding UTF8

    # 启动器（读 install.env 定位运行时根，任意路径执行 airymaxrt 即启动）
    # Windows zip 不构建 Rust TUI：优先 agentrt-tui.exe，回退 C 实现 airy_cli.exe
    # 启动器头部固化真实 AIRY_HOME（-Prefix 自定义安装的关键：cmd 子进程
    # 不继承父 shell 的 $env:AIRY_HOME，此前硬编码 %USERPROFILE%\.airymaxrt
    # 导致自定义路径安装后启动器失效——历史故障，2026-08-25 修复）。
    # 保留 findstr install.env 覆盖：环境变量显式设置 / install.env 变更时
    # 以权威文件为准（幂等）。
    $launcher = Join-Path $AIRY_HOME "bin\airymaxrt.cmd"
    $escapedHome = $AIRY_HOME.Replace('"','""')
    $cmdContent = @(
        "@echo off",
        "setlocal",
        ("set ""AIRY_HOME={0}""" -f $escapedHome),
        "if exist ""%AIRY_HOME%\config\install.env"" (",
        "  for /f ""tokens=2 delims=="" %%a in ('findstr /b ""AIRY_HOME="" ""%AIRY_HOME%\config\install.env"" 2^>nul') do set ""AIRY_HOME=%%a""",
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

# ─── 完整性校验 ──────────────────────────────────────────────────────────
function Verify-Daemons {
    param([switch]$Strict)
    $missing = @()
    foreach ($d in $EXPECTED_DAEMONS) {
        if (-not (Test-Path (Join-Path $AIRY_HOME "bin\$d.exe"))) { $missing += $d }
    }
    if ($missing.Count -gt 0) {
        if ($Strict) {
            Write-Err "daemon 校验失败，缺失: $($missing -join ' ')（二进制包不完整，请检查 release 制品）"
            exit 1
        }
        Write-Warn "daemon 校验未全通过，缺失: $($missing -join ' ')"
    } else {
        Write-OK "$($EXPECTED_DAEMONS.Count) 个 daemon 全部就位"
    }
}

# ─── 主流程 ──────────────────────────────────────────────────────────────
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
# 发布来源解析：-FromFile 离线包 > AIRY_RELEASE_URL 显式 URL > 官方通道 manifest
# （默认，stable/beta 由 -Channel 决定；-Mode source 除外）
$releaseUrl = $env:AIRY_RELEASE_URL
if (-not $releaseUrl -and $Mode -ne "source") {
    $releaseUrl = "https://raw.atomgit.com/openairymax/agentrt/raw/main/latest/manifest.$AIRY_CHANNEL.json"
}
if ($FromFile) { $installed = Install-Binary $FromFile }
elseif ($Mode -eq "binary" -or ($Mode -eq "auto" -and $releaseUrl)) {
    $installed = Install-Binary $releaseUrl
}
elseif ($Mode -eq "binary") { Write-Err "模式 binary 需要 AIRY_RELEASE_URL"; exit 1 }

if (-not $installed) {
    Write-Info "进入源码构建模式（$Mode）"
    # 工具链仅在源码构建路径要求（二进制模式无需 git/cmake/编译器）
    Check-Toolchain
    # 模式 B：无闭源源码 → 先下载闭源预编译模块（cmake 配置需要）
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
