use serde::{Deserialize, Serialize};
use tauri::State;
use std::sync::Mutex;
use crate::cli::{self, CliConfig, CliCommandResult};

pub struct AppState {
    pub config: Mutex<CliConfig>,
}

impl Default for AppState {
    fn default() -> Self {
        Self {
            config: Mutex::new(CliConfig::default()),
        }
    }
}

#[derive(Debug, Serialize, Deserialize)]
pub struct SystemInfo {
    pub os: String,
    pub os_version: String,
    pub architecture: String,
    pub cpu_cores: usize,
    pub total_memory_gb: f64,
    pub free_memory_gb: f64,
    pub hostname: String,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct ServiceStatus {
    pub name: String,
    pub status: String,
    pub healthy: bool,
    pub uptime_seconds: Option<u64>,
    pub port: Option<u16>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct AgentInfo {
    pub id: String,
    pub name: String,
    pub status: String,
    pub task_count: u32,
    pub last_active: Option<String>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct TaskInfo {
    pub id: String,
    pub agent_id: String,
    pub status: String,
    pub progress: f32,
    pub created_at: String,
    pub updated_at: Option<String>,
}

#[tauri::command]
pub async fn get_system_info(state: State<'_, AppState>) -> Result<SystemInfo, String> {
    let sys = sysinfo::System::new_all();
    
    let os = sysinfo::System::operating_system().to_string();
    let os_version = sysinfo::System::os_version()
        .unwrap_or_else(|| "Unknown".to_string());
    let architecture = std::env::consts::ARCH.to_string();
    let cpu_cores = sysinfo::System::cpu_num() as usize;
    let total_memory_gb = sys.total_memory() as f64 / (1024.0 * 1024.0 * 1024.0);
    let free_memory_gb = sys.free_memory() as f64 / (1024.0 * 1024.0 * 1024.0);
    let hostname = sysinfo::System::host_name()
        .unwrap_or_else(|| "Unknown".to_string());

    Ok(SystemInfo {
        os,
        os_version,
        architecture,
        cpu_cores,
        total_memory_gb,
        free_memory_gb,
        hostname,
    })
}

#[tauri::command]
pub async fn execute_cli_command(
    command: String,
    args: Vec<String>,
    state: State<'_, AppState>,
) -> Result<CliCommandResult, String> {
    let config = state.config.lock().map_err(|e| e.to_string())?;
    
    let working_dir = config
        .detect_project_root()
        .ok();

    let args_refs: Vec<&str> = args.iter().map(|s| s.as_str()).collect();
    
    cli::execute_command(
        &command,
        &args_refs,
        working_dir.as_deref(),
        config.timeout_seconds,
    )
    .map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn get_service_status(state: State<'_, AppState>) -> Result<Vec<ServiceStatus>, String> {
    let config = state.config.lock().map_err(|e| e.to_string())?;
    let docker_dir = config.get_docker_dir().map_err(|e| e.to_string())?;

    let result = cli::execute_command(
        "docker",
        &["compose", "ps", "--format", "json"],
        Some(&docker_dir),
        30,
    )
    .map_err(|e| e.to_string())?;

    if !result.success {
        return Err(format!("Failed to get service status: {}", result.stderr));
    }

    let mut services = Vec::new();

    for line in result.stdout.lines() {
        if line.trim().is_empty() {
            continue;
        }

        let parsed: serde_json::Value = match serde_json::from_str(line) {
            Ok(v) => v,
            Err(_) => continue,
        };

        let name = parsed["Name"].as_str().unwrap_or("unknown").to_string();
        let status = parsed["Status"].as_str().unwrap_or("unknown").to_string();
        
        let healthy = status.contains("running") || status.contains("healthy");
        let port = parsed["Publishers"]
            .as_array()
            .and_then(|arr| arr.first())
            .and_then(|p| p["PublishedPort"].as_u64())
            .map(|p| p as u16);

        services.push(ServiceStatus {
            name,
            status,
            healthy,
            uptime_seconds: None,
            port,
        });
    }

    Ok(services)
}

#[tauri::command]
pub async fn start_services(
    mode: String,
    state: State<'_, AppState>,
) -> Result<CliCommandResult, String> {
    let config = state.config.lock().map_err(|e| e.to_string())?;
    let docker_dir = config.get_docker_dir().map_err(|e| e.to_string())?;

    let args = match mode.as_str() {
        "prod" => vec![
            "--env-file".to_string(),
            "../.env.production".to_string(),
            "-f".to_string(),
            "docker-compose.prod.yml".to_string(),
            "up".to_string(),
            "-d".to_string(),
        ],
        _ => vec!["up".to_string(), "-d".to_string()],
    };

    let args_refs: Vec<&str> = args.iter().map(|s| s.as_str()).collect();

    cli::execute_command("docker", &["compose"], Some(&docker_dir), 120)
        .and_then(|_| {
            cli::execute_command(
                "docker",
                &args_refs,
                Some(&docker_dir),
                300,
            )
        })
        .map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn stop_services(state: State<'_, AppState>) -> Result<CliCommandResult, String> {
    let config = state.config.lock().map_err(|e| e.to_string())?;
    let docker_dir = config.get_docker_dir().map_err(|e| e.to_string())?;

    cli::execute_command(
        "docker",
        &["compose", "down"],
        Some(&docker_dir),
        120,
    )
    .map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn restart_services(
    mode: String,
    state: State<'_, AppState>,
) -> Result<CliCommandResult, String> {
    stop_services(state.clone()).await?;
    tokio::time::sleep(std::time::Duration::from_secs(3)).await;
    start_services(mode, state).await
}

#[tauri::command]
pub async fn get_logs(
    service: Option<String>,
    tail: Option<u32>,
    state: State<'_, AppState>,
) -> Result<String, String> {
    let config = state.config.lock().map_err(|e| e.to_string())?;
    let docker_dir = config.get_docker_dir().map_err(|e| e.to_string())?;

    let tail_count = tail.unwrap_or(100).to_string();
    
    let mut args = vec!["logs", "--tail", &tail_count];
    
    if let Some(svc) = service {
        args.push(&svc);
    }

    let args_refs: Vec<&str> = args.into_iter().collect();

    let result = cli::execute_command(
        "docker",
        &args_refs,
        Some(&docker_dir),
        30,
    )
    .map_err(|e| e.to_string())?;

    if result.success {
        Ok(result.stdout)
    } else {
        Err(format!("Failed to get logs: {}", result.stderr))
    }
}

#[tauri::command]
pub async fn get_health_status(state: State<'_, AppState>) -> Result<Vec<ServiceStatus>, String> {
    let services = get_service_status(state).await?;
    
    let health_results: Vec<ServiceStatus> = services
        .into_iter()
        .map(|mut svc| {
            svc.healthy = svc.status.contains("running") || svc.status.contains("healthy");
            svc
        })
        .collect();

    Ok(health_results)
}

#[tauri::command]
pub async fn read_config_file(
    path: String,
    _state: State<'_, AppState>,
) -> Result<String, String> {
    use std::fs;

    fs::read_to_string(&path).map_err(|e| format!("Failed to read {}: {}", path, e))
}

#[tauri::command]
pub async fn write_config_file(
    path: String,
    content: String,
    _state: State<'_, AppState>,
) -> Result<(), String> {
    use std::fs;

    fs::write(&path, &content).map_err(|e| format!("Failed to write {}: {}", path, e))
}

#[tauri::command]
pub async fn list_agents(_state: State<'_, AppState>) -> Result<Vec<AgentInfo>, String> {
    Ok(vec![
        AgentInfo {
            id: "agent-001".to_string(),
            name: "Research Assistant".to_string(),
            status: "idle".to_string(),
            task_count: 0,
            last_active: Some(chrono::Utc::now().to_rfc3339()),
        },
        AgentInfo {
            id: "agent-002".to_string(),
            name: "Code Reviewer".to_string(),
            status: "running".to_string(),
            task_count: 3,
            last_active: Some(chrono::Utc::now().to_rfc3339()),
        },
        AgentInfo {
            id: "agent-003".to_string(),
            name: "Data Analyst".to_string(),
            status: "idle".to_string(),
            task_count: 1,
            last_active: None,
        },
    ])
}

#[tauri::command]
pub async fn get_agent_details(
    agent_id: String,
    _state: State<'_, AppState>,
) -> Result<AgentInfo, String> {
    list_agents(_state).await?
        .into_iter()
        .find(|a| a.id == agent_id)
        .ok_or_else(|| format!("Agent not found: {}", agent_id))
}

#[tauri::command]
pub async fn submit_task(
    agent_id: String,
    task_description: String,
    priority: Option<String>,
    _state: State<'_, AppState>,
) -> Result<TaskInfo, String> {
    let task_id = uuid::Uuid::new_v4().to_string();
    
    log::info!(
        "Submitting task '{}' to agent '{}'",
        task_description,
        agent_id
    );

    Ok(TaskInfo {
        id: task_id,
        agent_id,
        status: "pending".to_string(),
        progress: 0.0,
        created_at: chrono::Utc::now().to_rfc3339(),
        updated_at: None,
    })
}

#[tauri::command]
pub async fn get_task_status(
    task_id: String,
    _state: State<'_, AppState>,
) -> Result<TaskInfo, String> {
    Ok(TaskInfo {
        id: task_id,
        agent_id: "agent-001".to_string(),
        status: "completed".to_string(),
        progress: 100.0,
        created_at: chrono::Utc::now().to_rfc3339(),
        updated_at: Some(chrono::Utc::now().to_rfc3339()),
    })
}

#[tauri::command]
pub async fn cancel_task(
    task_id: String,
    _state: State<'_, AppState>,
) -> Result<(), String> {
    log::info!("Cancelling task: {}", task_id);
    Ok(())
}

#[tauri::command]
pub async fn open_terminal(
    working_dir: Option<String>,
    _state: State<'_, AppState>,
) -> Result<(), String> {
    #[cfg(target_os = "macos")]
    {
        let dir = working_dir.unwrap_or_else(|| dirs::home_dir()
            .unwrap_or_default()
            .to_string_lossy()
            .to_string());
        
        std::process::Command::new("open")
            .args(["-a", "Terminal", &dir])
            .spawn()
            .map_err(|e| format!("Failed to open terminal: {}", e))?;
    }

    #[cfg(target_os = "windows")]
    {
        let dir = working_dir.unwrap_or_else(|| {
            std::env::var("USERPROFILE").unwrap_or_else(|_| "C:\\".to_string())
        });
        
        std::process::Command::new("cmd")
            .args(["/k", "cd", "/d", &dir])
            .spawn()
            .map_err(|e| format!("Failed to open terminal: {}", e))?;
    }

    #[cfg(target_os = "linux")]
    {
        let dir = working_dir.unwrap_or_else(|| dirs::home_dir()
            .unwrap_or_default()
            .to_string_lossy()
            .to_string());
        
        let terminals = ["gnome-terminal", "konsole", "xfce4-terminal", "xterm"];
        
        for terminal in &terminals {
            if which::which(terminal).is_ok() {
                std::process::Command::new(terminal)
                    .args(["--working-directory=", &dir])
                    .spawn()
                    .map_err(|e| format!("Failed to open terminal: {}", e))?;
                return Ok(());
            }
        }

        return Err("No compatible terminal found".to_string());
    }

    #[cfg(not(any(target_os = "macos", target_os = "windows", target_os = "linux")))]
    {
        return Err("Unsupported operating system".to_string());
    }

    Ok(())
}

#[tauri::command]
pub async fn open_browser(url: String, _state: State<'_, AppState>) -> Result<(), String> {
    webbrowser::open(&url).map_err(|e| format!("Failed to open browser: {}", e))
}

#[tauri::command]
pub async fn check_for_updates(_state: State<'_, AppState>) -> Result<UpdateInfo, String> {
    Ok(UpdateInfo {
        current_version: env!("CARGO_PKG_VERSION").to_string(),
        latest_version: "0.2.0".to_string(),
        update_available: true,
        release_url: "https://github.com/SpharxTeam/AgentOS/releases".to_string(),
        release_notes: "New features and bug fixes".to_string(),
    })
}

#[derive(Debug, Serialize, Deserialize)]
pub struct UpdateInfo {
    pub current_version: String,
    pub latest_version: String,
    pub update_available: bool,
    pub release_url: String,
    pub release_notes: String,
}

#[tauri::command]
pub async fn get_version_info(_state: State<'_, AppState>) -> Result<VersionInfo, String> {
    Ok(VersionInfo {
        app_version: env!("CARGO_PKG_VERSION").to_string(),
        build_time: option_env!("VERGEN_BUILD_TIMESTAMP").unwrap_or("unknown").to_string(),
        git_commit: option_env!("VERGEN_GIT_SHA").unwrap_or("unknown").to_string(),
        rust_version: option_env!("RUSTC_VERSION").unwrap_or("unknown").to_string(),
        tauri_version: option_env!("TAURI_VERSION").unwrap_or("unknown").to_string(),
    })
}

#[derive(Debug, Serialize, Deserialize)]
pub struct VersionInfo {
    pub app_version: String,
    pub build_time: String,
    pub git_commit: String,
    pub rust_version: String,
    pub tauri_version: String,
}

#[tauri::command]
pub async fn download_and_install_update(
    app: tauri::AppHandle,
    _state: State<'_, AppState>,
) -> Result<(), String> {
    use tauri_plugin_updater::UpdaterExt;
    
    let updater = app.updater().map_err(|e| e.to_string())?;
    
    let update = updater.check().await.map_err(|e| e.to_string())?
        .ok_or("No update available")?;
    
    log::info!("Downloading update {}...", update.version);
    
    let mut downloaded = 0;
    let total = update.content_length.unwrap_or(0);
    
    update.download_and_install(
        |chunk_length, content_length| {
            downloaded += chunk_length;
            log::info!("Downloaded {} of {} bytes", downloaded, content_length.unwrap_or(0));
        },
        || {
            log::info!("Download complete, installing...");
        },
    ).await.map_err(|e| e.to_string())?;
    
    log::info!("Update installed successfully");
    
    Ok(())
}
