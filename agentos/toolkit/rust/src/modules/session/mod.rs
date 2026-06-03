// AgentOS Rust SDK - 会话管理模块
// Version: 0.1.0
// Last updated: 2026-03-24
//
// 提供会话的创建、查询、更新、删除及上下文管理功能。
// 对应 Go SDK: modules/session/manager.go

mod manager;

pub use manager::SessionManager;
