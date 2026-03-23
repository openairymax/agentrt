# AgentOS README.md 批量更新脚本
# 版本：v1.0.0.6
# 日期：2026-03-21
# 用途：系统性更新所有 README.md 文档的版本号、路径引用和一致性

$ErrorActionPreference = "Stop"
$basePath = "D:\Spharx\SpharxWorks\AgentOS"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "AgentOS README.md 批量更新工具 v1.0.0.6" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 排除 node_modules 目录，只处理项目自己的 README
$readmeFiles = Get-ChildItem -Path $basePath -Recurse -Filter "README.md" | 
    Where-Object { $_.FullName -notmatch "node_modules" } |
    Select-Object -ExpandProperty FullName

Write-Host "找到 $($readmeFiles.Count) 个 README.md 文件需要处理" -ForegroundColor Yellow
Write-Host ""

# 统计信息
$stats = @{
    Total = $readmeFiles.Count
    Updated = 0
    Skipped = 0
    Errors = 0
}

foreach ($file in $readmeFiles) {
    $relativePath = $file.Replace($basePath, "").TrimStart("\")
    Write-Host "处理：$relativePath" -ForegroundColor Gray
    
    try {
        $content = Get-Content -Path $file -Raw -Encoding UTF8
        
        # 1. 更新版本号徽章 (多种格式)
        $content = $content -replace 'version-\d+\.\d+\.\d+\.\d+', 'version-1.0.0.6'
        $content = $content -replace 'Version: v?\d+\.\d+\.\d+\.\d+', 'Version: v1.0.0.6'
        $content = $content -replace '\*\*版本\*\*: v?\d+\.\d+\.\d+\.\d+', '**版本**: v1.0.0.6'
        
        # 2. 更新路径引用（core → corekern）
        $content = $content -replace 'atoms/core/', 'atoms/corekern/'
        $content = $content -replace 'atoms/coreprop/', 'atoms/corekern/'
        $content = $content -replace '`atoms/core/`', '`atoms/corekern/`'
        $content = $content -replace '`atoms/coreprop/`', '`atoms/corekern/`'
        
        # 3. 更新 partdocs 路径下的架构文档引用
        $content = $content -replace 'partdocs/architecture/architect_handbook.md', 'partdocs/architecture/ARCHITECT_HANDBOOK.md'
        $content = $content -replace 'partdocs/guides/architect_handbook.md', 'partdocs/guides/ARCHITECT_HANDBOOK.md'
        
        # 4. 统一版权声明格式
        if ($content -match 'Copyright.*SPHARX') {
            # 已包含 SPHARX 版权，保持不变
        } elseif ($content -match 'Copyright') {
            # 有其他版权声明，需要替换
            $content = $content -replace 'Copyright.*?SPHARX.*?All Rights Reserved\.', '© 2026 SPHARX Ltd. All Rights Reserved.'
        }
        
        # 保存修改
        Set-Content -Path $file -Value $content -Encoding UTF8 -NoNewline
        
        $stats.Updated++
        Write-Host "  ✓ 更新完成" -ForegroundColor Green
    }
    catch {
        $stats.Errors++
        Write-Host "  ✗ 错误：$($_.Exception.Message)" -ForegroundColor Red
    }
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "更新完成统计:" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "总文件数：$($stats.Total)" -ForegroundColor White
Write-Host "成功更新：$($stats.Updated)" -ForegroundColor Green
Write-Host "跳过：$($stats.Skipped)" -ForegroundColor Yellow
Write-Host "错误：$($stats.Errors)" -ForegroundColor Red
Write-Host "========================================" -ForegroundColor Cyan
