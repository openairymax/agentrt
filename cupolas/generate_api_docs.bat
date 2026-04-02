@echo off
REM ---------------------------------------------------------------------------
REM generate_api_docs.bat - cupolas 模块 Doxygen API 文档生成脚本
REM
REM SPDX-FileCopyrightText: 2026 SPHARX Ltd.
REM SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause
REM
REM Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
REM
REM 用途: 一键生成 cupolas 模块的 Doxygen API 文档
REM 前提: 需要先安装 Doxygen (https://www.doxygen.nl/download.html)
REM ---------------------------------------------------------------------------

echo ========================================
echo   cupolas - AgentOS 安全穹顶
echo   Doxygen API 文档生成工具
echo ========================================
echo.

REM 检查 Doxygen 是否已安装
where doxygen >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] 未检测到 Doxygen！
    echo.
    echo 请按以下步骤安装：
    echo   1. 访问 https://www.doxygen.nl/download.html
    echo   2. 下载 Windows 安装包 (doxygen-x.x.x.x-windows.x64.bin.zip)
    echo   3. 解压到任意目录 (如 C:\doxygen)
    echo   4. 将 bin 目录添加到系统 PATH 环境变量
    echo   5. 重启终端后重新运行此脚本
    echo.
    echo 或者使用 Chocolatey 快速安装:
    echo   choco install doxygen
    echo.
    echo 或使用 Scoop 安装:
    echo   scoop install doxygen
    echo.
    pause
    exit /b 1
)

echo [INFO] Doxygen 已就绪
echo.

REM 获取脚本所在目录
set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%"

echo [INFO] 工作目录: %CD%
echo.

REM 清理旧的文档输出
if exist docs/api (
    echo [INFO] 清理旧文档...
    rmdir /s /q docs/api
)

REM 运行 Doxygen
echo [INFO] 开始生成 API 文档...
echo.
doxygen Doxyfile

REM 检查结果
if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo   ✓ API 文档生成成功！
    echo ========================================
    echo.
    echo 输出位置: %CD%\docs\api\html\index.html
    echo.
    echo 正在打开浏览器...
    start "" "%CD%\docs\api\html\index.html"
) else (
    echo.
    echo ========================================
    echo   ✗ 文档生成失败！
    echo ========================================
    echo.
    echo 请检查日志文件:
    echo   %CD%\docs\doxygen_warnings.log
    echo.
    pause
    exit /b 1
)

pause
