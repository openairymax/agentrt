use std::process::Command;
use std::time::{Duration, Instant};
use anyhow::{Context, Result};
use serde::{Deserialize, Serialize};

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct CliCommandResult {
    pub success: bool,
    pub stdout: String,
    pub stderr: String,
    pub exit_code: i32,
    pub duration_ms: u128,
}

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct CliConfig {
    pub agentos_cli_path: Option<String>,
    pub project_root: Option<String>,
    pub docker_compose_path: Option<String>,
    pub timeout_seconds: u64,
    pub gateway_url: Option<String>,
    pub api_key: Option<String>,
}

impl Default for CliConfig {
    fn default() -> Self {
        Self {
            agentos_cli_path: None,
            project_root: None,
            docker_compose_path: None,
            timeout_seconds: 300,
            gateway_url: Some("http://localhost:18789".to_string()),
            api_key: None,
        }
    }
}

impl CliConfig {
    pub fn detect_agentos_cli(&self) -> Result<String> {
        if let Some(ref path) = self.agentos_cli_path {
            if std::path::Path::new(path).exists() {
                return Ok(path.clone());
            }
        }

        let possible_paths = vec![
            "agentos-cli".to_string(),
            "agentos".to_string(),
            "/usr/local/bin/agentos-cli".to_string(),
            "/usr/bin/agentos-cli".to_string(),
        ];

        for path in &possible_paths {
            if which::which(path).is_ok() {
                return Ok(path.clone());
            }
        }

        Err(anyhow::anyhow!("AgentOS CLI not found in PATH"))
    }

    pub fn detect_project_root(&self) -> Result<String> {
        if let Some(ref root) = self.project_root {
            if std::path::Path::new(root).exists() {
                return Ok(root.clone());
            }
        }

        let current_dir = std::env::current_dir()
            .context("Failed to get current directory")?;

        for parent in current_dir.ancestors() {
            let docker_compose = parent.join("docker").join("docker-compose.yml");
            if docker_compose.exists() {
                return Ok(parent.to_string_lossy().to_string());
            }
        }

        Err(anyhow::anyhow!(
            "AgentOS project root not found (looking for docker/docker-compose.yml)"
        ))
    }

    pub fn get_docker_dir(&self) -> Result<String> {
        let root = self.detect_project_root()?;
        Ok(std::path::Path::new(&root)
            .join("docker")
            .to_string_lossy()
            .to_string())
    }
}

pub fn execute_command(
    program: &str,
    args: &[&str],
    working_dir: Option<&str>,
    timeout_secs: u64,
) -> Result<CliCommandResult> {
    let start = Instant::now();

    let mut cmd = Command::new(program);
    cmd.args(args);
    cmd.stdout(std::process::Stdio::piped());
    cmd.stderr(std::process::Stdio::piped());

    if let Some(dir) = working_dir {
        cmd.current_dir(dir);
    }

    log::debug!("Executing command: {} {}", program, args.join(" "));

    let output = match cmd.output() {
        Ok(output) => output,
        Err(e) => {
            return Ok(CliCommandResult {
                success: false,
                stdout: String::new(),
                stderr: format!("Failed to execute command: {}", e),
                exit_code: -1,
                duration_ms: start.elapsed().as_millis(),
            });
        }
    };

    let duration_ms = start.elapsed().as_millis();
    let stdout = String::from_utf8_lossy(&output.stdout).to_string();
    let stderr = String::from_utf8_lossy(&output.stderr).to_string();

    let result = CliCommandResult {
        success: output.status.success(),
        stdout,
        stderr,
        exit_code: output.status.code().unwrap_or(-1),
        duration_ms,
    };

    if result.success {
        log::info!(
            "Command completed successfully in {}ms",
            result.duration_ms
        );
    } else {
        log::warn!(
            "Command failed with exit code {} in {}ms: {}",
            result.exit_code,
            result.duration_ms,
            result.stderr
        );
    }

    Ok(result)
}

pub async fn execute_command_async(
    program: &str,
    args: Vec<String>,
    working_dir: Option<String>,
    timeout_secs: u64,
) -> Result<CliCommandResult> {
    use tokio::process::Command;

    let start = Instant::now();

    let mut cmd = Command::new(program);
    cmd.args(&args);
    cmd.stdout(std::process::Stdio::piped());
    cmd.stderr(std::process::Stdio::piped());

    if let Some(dir) = working_dir {
        cmd.current_dir(dir);
    }

    log::debug!("Executing async command: {} {}", program, args.join(" "));

    let timeout = Duration::from_secs(timeout_secs);

    match tokio::time::timeout(timeout, cmd.output()).await {
        Ok(Ok(output)) => {
            let duration_ms = start.elapsed().as_millis();
            let stdout = String::from_utf8_lossy(&output.stdout).to_string();
            let stderr = String::from_utf8_lossy(&output.stderr).to_string();

            let result = CliCommandResult {
                success: output.status.success(),
                stdout,
                stderr,
                exit_code: output.status.code().unwrap_or(-1),
                duration_ms,
            };

            if result.success {
                log::info!("Async command completed in {}ms", result.duration_ms);
            } else {
                log::warn!(
                    "Async command failed with code {} in {}ms",
                    result.exit_code,
                    result.duration_ms
                );
            }

            Ok(result)
        }
        Ok(Err(e)) => Ok(CliCommandResult {
            success: false,
            stdout: String::new(),
            stderr: format!("Failed to execute: {}", e),
            exit_code: -1,
            duration_ms: start.elapsed().as_millis(),
        }),
        Err(_) => Ok(CliCommandResult {
            success: false,
            stdout: String::new(),
            stderr: format!("Command timed out after {} seconds", timeout_secs),
            exit_code: -1,
            duration_ms: start.elapsed().as_millis(),
        }),
    }
}
