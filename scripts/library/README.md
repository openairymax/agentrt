# Shell 脚本公共库

`scripts/library/`

## 概述

`library/` 目录包含 AgentOS 各 Shell 脚本间共享的公共函数库，提供日志输出、错误处理、平台检测等通用功能，减少脚本间的代码重复。

## 模块列表

| 模块 | 说明 |
|------|------|
| `common.sh` | 通用函数库：日志、错误处理、配置文件加载 |
| `log.sh` | 日志输出：结构化日志，支持多级别输出 |
| `error.sh` | 错误处理：错误码定义、错误捕获和报告 |
| `platform.sh` | 平台检测：操作系统和架构识别 |

## 引用方式

```bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/../library/common.sh"
source "${SCRIPT_DIR}/../library/log.sh"
source "${SCRIPT_DIR}/../library/error.sh"
source "${SCRIPT_DIR}/../library/platform.sh"
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
