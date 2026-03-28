// AgentOS Go SDK - 根包入口
// Version: 3.0.0
// Last updated: 2026-03-22
//
// SDK 顶层统一入口，提供版本信息并导出所有公共 API。
// 对应 Python SDK: __init__.py
//
// 目录结构映射:
//   agentos/          -> 根包（错误、配置、版本）
//   agentos/types/    -> 类型定义（枚举、领域模型）
//   agentos/utils/    -> 工具函数（helpers、ID生成、验证）
//   agentos/telemetry/ -> 遥测（Meter、Tracer、Span）
//   agentos/client/   -> 客户端层（APIClient接口、HTTP实现）
//   agentos/modules/  -> 业务模块（task、memory、session、skill）

package agentos

const (
	// Version SDK 版本号
	Version = "3.0.0"
	// Author SDK 作者
	Author = "SpharxWorks"
	// License SDK 许可证
	License = "MIT"
)
