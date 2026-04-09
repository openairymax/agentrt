# Archive - 已归档的旧版本脚本

此目录存放已废弃或被替代的历史脚本，供参考和回溯使用。

## 📦 归档内容

### deploy.sh.v1.0-legacy
- **原位置**: `scripts/deploy.sh`
- **替代者**: `scripts/install.sh` (Unix) + `scripts/install.ps1` (Windows)
- **归档时间**: 2026-04-08
- **归档原因**: 
  - 仅支持Linux平台（bash-only）
  - 功能已被跨平台版本完全覆盖
  - 保留用于历史参考和对比

## ⚠️ 使用说明

**不建议直接使用这些脚本**，除非您有特殊需求需要参考历史实现。

如需部署功能，请使用：
```bash
# Unix (Linux/macOS)
./install.sh --mode dev --auto

# Windows
.\install.ps1 -Mode dev -Auto
```

## 📚 相关文档

- [新版部署脚本文档](../README.md#部署工具)
- [桌面客户端](../desktop-client/README.md)
