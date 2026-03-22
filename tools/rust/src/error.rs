// AgentOS Rust SDK Error
// Version: 1.0.0.5
// Last updated: 2026-03-21

use thiserror::Error;
use reqwest;

/// AgentOS error type
#[derive(Error, Debug)]
pub enum AgentOSError {
    /// Network error
    #[error("Network error: {0}")]
    Network(String),
    
    /// HTTP error
    #[error("HTTP error: {0}")]
    Http(String),
    
    /// JSON error
    #[error("JSON error: {0}")]
    Json(String),
    
    /// Task error
    #[error("Task error: {0}")]
    Task(String),
    
    /// Memory error
    #[error("Memory error: {0}")]
    Memory(String),
    
    /// Session error
    #[error("Session error: {0}")]
    Session(String),
    
    /// Skill error
    #[error("Skill error: {0}")]
    Skill(String),
    
    /// Timeout error
    #[error("Timeout error: {0}")]
    Timeout(String),
    
    /// Invalid response error
    #[error("Invalid response: {0}")]
    InvalidResponse(String),
    
    /// Other error
    #[error("Other error: {0}")]
    Other(String),
}

impl From<reqwest::Error> for AgentOSError {
    fn from(err: reqwest::Error) -> Self {
        if err.is_timeout() {
            AgentOSError::Timeout(err.to_string())
        } else if err.is_request() {
            AgentOSError::Network(err.to_string())
        } else if err.is_response() {
            AgentOSError::Http(err.to_string())
        } else {
            AgentOSError::Other(err.to_string())
        }
    }
}

impl From<serde_json::Error> for AgentOSError {
    fn from(err: serde_json::Error) -> Self {
        AgentOSError::Json(err.to_string())
    }
}
