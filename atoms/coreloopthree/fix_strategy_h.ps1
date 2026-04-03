# coreloopthree 编译错误修复脚本
# 用途：修复 strategy.h 文件中缺少 agentos_llm_service_t 类型前向声明的问题

$sourceFile = "d:\Spharx\SpharxWorks\AgentOS\atoms\coreloopthree\src\cognition\coordinator\strategy.h"
$tempFile = "d:\Spharx\SpharxWorks\AgentOS\atoms\coreloopthree\src\cognition\coordinator\strategy.h.tmp"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "coreloopthree 编译错误修复脚本" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 检查临时文件是否存在
if (-not (Test-Path $tempFile)) {
    Write-Host "错误：临时文件不存在！" -ForegroundColor Red
    Write-Host "请先运行修复准备脚本。" -ForegroundColor Yellow
    exit 1
}

Write-Host "步骤 1：检查临时文件..." -ForegroundColor Yellow
Write-Host "临时文件已就绪：$tempFile" -ForegroundColor Green
Write-Host ""

Write-Host "步骤 2：备份原文件..." -ForegroundColor Yellow
$backupFile = $sourceFile + ".backup"
if (Test-Path $sourceFile) {
    Copy-Item -Path $sourceFile -Destination $backupFile -Force
    Write-Host "备份已创建：$backupFile" -ForegroundColor Green
} else {
    Write-Host "原文件不存在，跳过备份。" -ForegroundColor Yellow
}
Write-Host ""

Write-Host "步骤 3：应用修复..." -ForegroundColor Yellow
Write-Host "请确保已关闭所有打开 strategy.h 文件的程序（IDE、编辑器等）" -ForegroundColor Red
Write-Host ""
Write-Host "按任意键继续修复..." -ForegroundColor Yellow
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")

try {
    # 尝试复制文件
    Copy-Item -Path $tempFile -Destination $sourceFile -Force
    Write-Host ""
    Write-Host "✓ 修复成功！" -ForegroundColor Green
    Write-Host ""
    
    # 验证修复
    Write-Host "步骤 4：验证修复..." -ForegroundColor Yellow
    $content = Get-Content $sourceFile -Raw -Encoding UTF8
    if ($content -match "struct llm_service;" -and $content -match "typedef struct llm_service agentos_llm_service_t;") {
        Write-Host "✓ 验证成功：前向声明已正确添加" -ForegroundColor Green
        Write-Host ""
        Write-Host "修复内容：" -ForegroundColor Cyan
        Write-Host "  - 添加了 struct llm_service 前向声明" -ForegroundColor White
        Write-Host "  - 添加了 agentos_llm_service_t 类型定义" -ForegroundColor White
        Write-Host ""
        Write-Host "下一步：重新编译 coreloopthree 模块" -ForegroundColor Yellow
        Write-Host "命令：cd d:\Spharx\SpharxWorks\AgentOS\atoms\build && cmake --build . --config Release" -ForegroundColor White
    } else {
        Write-Host "✗ 验证失败：前向声明未找到" -ForegroundColor Red
        Write-Host "请检查文件内容。" -ForegroundColor Yellow
    }
} catch {
    Write-Host ""
    Write-Host "✗ 修复失败！" -ForegroundColor Red
    Write-Host "错误：$($_.Exception.Message)" -ForegroundColor Red
    Write-Host ""
    Write-Host "可能的原因：" -ForegroundColor Yellow
    Write-Host "  1. 文件仍被其他程序锁定" -ForegroundColor White
    Write-Host "  2. 文件权限不足" -ForegroundColor White
    Write-Host ""
    Write-Host "解决方法：" -ForegroundColor Yellow
    Write-Host "  1. 关闭所有 IDE 和编辑器" -ForegroundColor White
    Write-Host "  2. 以管理员身份运行此脚本" -ForegroundColor White
    Write-Host "  3. 手动复制临时文件到原文件" -ForegroundColor White
    Write-Host "     源文件：$tempFile" -ForegroundColor White
    Write-Host "     目标文件：$sourceFile" -ForegroundColor White
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "按任意键退出..." -ForegroundColor Yellow
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
