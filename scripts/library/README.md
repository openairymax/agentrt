# 公共库脚本

`scripts/library/`

## 概述

`library/` 目录包含 AgentOS 各脚本间共享的公共函数库，提供日志输出、错误处理、配置文件解析、颜色输出等通用功能，减少脚本间的代码重复。

## 模块列表

| 模块 | 说明 |
|------|------|
| `common.sh` | 通用函数库，包含日志、错误处理、配置文件加载 |
| `colors.sh` | 终端彩色输出定义 |
| `logging.sh` | 结构化日志输出，支持 JSON 和文本格式 |
| `config_parser.sh` | YAML/INI 配置文件解析器 |
| `utils.sh` | 工具函数集合（字符串处理、路径操作等） |

## 引用方式

```bash
# 在脚本中引入公共库
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/../library/common.sh"
source "${SCRIPT_DIR}/../library/logging.sh"

# 使用日志函数
log_info "系统初始化开始"
log_warn "磁盘使用率超过 80%"
log_error "服务启动失败" "exit_code=1"

# 彩色输出
source "${SCRIPT_DIR}/../library/colors.sh"
echo -e "${GREEN}成功${NC}"
echo -e "${RED}失败${NC}"
```

## 通用函数

| 函数 | 说明 |
|------|------|
| `log_info` | 信息级日志 |
| `log_warn` | 警告级日志 |
| `log_error` | 错误级日志 |
| `check_dependency` | 检查依赖命令是否可用 |
| `parse_yaml` | YAML 文件解析 |
| `confirm_action` | 交互式操作确认 |
| `measure_time` | 命令执行耗时统计 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
