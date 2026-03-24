// AgentOS Rust SDK - 任务管理模块
// Version: 3.0.0
// Last updated: 2026-03-24
//
// 提供任务的提交、查询、等待、取消、列表等生命周期管理功能。
// 对应 Go SDK: modules/task/manager.go

mod manager;

pub use manager::TaskManager;
