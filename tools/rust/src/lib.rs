// AgentOS Rust SDK
// Version: 1.0.0.5
// Last updated: 2026-03-21

//! AgentOS Rust SDK
//! 
//! This SDK provides a Rust interface to interact with the AgentOS system.
//! It includes functionality for task management, memory operations, session management,
//! and skill loading.

mod client;
mod error;
mod task;
mod memory;
mod session;
mod skill;
mod types;

pub use client::Client;
pub use error::AgentOSError;
pub use task::{Task, TaskStatus, TaskResult};
pub use memory::Memory;
pub use session::Session;
pub use skill::{Skill, SkillInfo, SkillResult};
pub use types::{MemoryInfo};

/// SDK version
pub const VERSION: &str = "1.0.0.5";
