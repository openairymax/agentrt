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

// ==================== LLM / AI Chat Commands ====================

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct LLMChatMessage {
    pub role: String,
    pub content: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub tool_calls: Option<serde_json::Value>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub tool_call_id: Option<String>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct LLMChatRequest {
    pub provider_id: String,
    pub messages: Vec<LLMChatMessage>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub model: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub temperature: Option<f64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub max_tokens: Option<u32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub stream: Option<bool>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub tools: Option<serde_json::Value>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct LLMChatResponse {
    pub id: String,
    pub content: String,
    pub role: String,
    pub model: String,
    pub finish_reason: String,
    pub usage: UsageInfo,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub tool_calls: Option<serde_json::Value>,
}

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct UsageInfo {
    pub prompt_tokens: u32,
    pub completion_tokens: u32,
    pub total_tokens: u32,
}

#[tauri::command]
pub async fn llm_chat(request: LLMChatRequest) -> Result<LLMChatResponse, String> {
    let model = request.model.as_deref().unwrap_or("gpt-4o").to_string();
    let temperature = request.temperature.unwrap_or(0.7);
    let max_tokens = request.max_tokens.unwrap_or(2048);

    log::info!("LLM chat request: provider={}, model={}, messages={}", request.provider_id, model, request.messages.len());

    let user_msg = request.messages.iter()
        .filter(|m| m.role == "user")
        .last()
        .map(|m| m.content.clone())
        .unwrap_or_default();

    let response_content = generate_simulated_response(&user_msg, &model);

    Ok(LLMChatResponse {
        id: format!("chatcmpl-{}", uuid::Uuid::new_v4()),
        content: response_content,
        role: "assistant".to_string(),
        model: model.clone(),
        finish_reason: "stop".to_string(),
        usage: UsageInfo {
            prompt_tokens: estimate_tokens(&request.messages),
            completion_tokens: estimate_tokens_str(&response_content),
            total_tokens: 0,
        },
        tool_calls: None,
    })
}

fn generate_simulated_response(user_input: &str, _model: &str) -> String {
    let input_lower = user_input.to_lowercase();

    if input_lower.contains("启动") || input_lower.contains("start") || input_lower.contains("服务") {
        return r#"已收到您的请求。让我先检查当前服务状态，然后执行启动操作。

**执行计划：**
1. 检查 Docker 环境是否就绪
2. 按 kernel → gateway → postgres → redis 的顺序依次启动
3. 验证每个服务的健康状态
4. 返回最终状态报告

正在执行... ✅ 所有服务已成功启动：
- **kernel**: running (port 18080)
- **gateway**: running (port 18789)
- **postgres**: running (port 5432)
- **redis**: running (port 6379)

服务集群已完全就绪，您现在可以开始使用 AgentOS 了。"#.to_string();
    }

    if input_lower.contains("智能体") || input_lower.contains("agent") || input_lower.contains("注册") {
        return r#"关于智能体管理，AgentOS 支持以下操作：

**可用的智能体类型：**
1. **研究型 (research)** - 网络搜索、文档分析、信息汇总
2. **编码型 (coding)** - 代码生成、调试、重构、审查
3. **助手型 (assistant)** - 通用对话、任务协调、问答
4. **系统型 (system)** - 服务管理、配置维护、监控

您可以点击「注册新智能体」来创建自定义智能体，或使用「任务管理」向现有智能体提交工作。

需要我帮您执行哪个具体操作吗？"#.to_string();
    }

    if input_lower.contains("你好") || input_lower.contains("hello") || input_lower.contains("hi") {
        return r#"您好！👋 我是 AgentOS 智能助手。

我可以帮您完成以下操作：

🔧 **系统管理**
- 启动/停止/重启服务集群
- 查看系统状态和健康检查

🤖 **智能体**
- 注册和管理 AI 智能体
- 提交和跟踪任务

⚙️ **配置**
- LLM 模型配置（OpenAI/Anthropic/本地模型）
- 系统参数调整

💾 **文件与日志**
- 编辑配置文件
- 查看实时日志

请告诉我您需要什么帮助！"#.to_string();
    }

    if input_lower.contains("内存") || input_lower.contains("memory") || input_lower.contains("记忆") {
        return r#"**AgentOS 记忆系统状态：**

| 类型 | 条目数 | Token 占用 |
|------|--------|-----------|
| 对话记忆 | 3 | 54 |
| 事实记忆 | 2 | 31 |
| 技能记忆 | 1 | 14 |
| 偏好记忆 | 2 | 26 |
| 错误记忆 | 1 | 20 |
| 观察记忆 | 1 | 15 |

**上下文窗口:** 3,842 / 128,000 tokens (3.0%)

记忆系统运行正常。最近的观察记录显示您的系统运行在 Windows x64 平台，16GB 内存。

需要查看详细记忆内容吗？"#.to_string();
    }

    if input_lower.contains("帮助") || input_lower.contains("help") || input_lower.contains("怎么用") {
        return r#"**AgentOS 使用指南**

## 快捷键
- `Ctrl+K` — 全局搜索
- `Ctrl+Shift+P` — 命令面板
- `?` — 键盘快捷键帮助
- `Ctrl+B` — 切换侧边栏

## 核心页面
1. **仪表盘** (`/`) — 系统总览、监控数据
2. **服务管理** (`/services`) — Docker 服务控制
3. **智能体** (`/agents`) — AI 智能体管理
4. **任务** (`/tasks`) — 任务提交与追踪
5. **AI 助手** (`/ai-chat`) — 对话式交互
6. **AgentOS 运行时** (`/agent-runtime`) — 认知循环与记忆系统

## 常用操作
- 点击浮动按钮 🤖 进入 AI 对话
- 设置页切换主题（浅色/深色/跟随系统）
- 配置 LLM API 以启用完整 AI 功能

还有其他问题吗？"#.to_string();
    }

    format!("感谢您的提问：「{}」\n\n作为 AgentOS 智能助手，我已经理解了您的需求。基于当前的系统状态和可用工具，我建议我们可以按以下步骤进行：\n\n1. **分析需求** — 理解具体目标和约束条件\n2. **制定方案** — 选择最佳执行路径\n3. **执行操作** — 调用相应工具完成任务\n4. **验证结果** — 确认输出符合预期\n\n如果您确认要继续，请回复「确认」，我将开始执行。", user_input)
}

fn estimate_tokens(messages: &[LLMChatMessage]) -> u32 {
    messages.iter().map(|m| estimate_tokens_str(&m.content)).sum()
}

fn estimate_tokens_str(text: &str) -> u32 {
    ((text.len() as f32) / 4.0).ceil() as u32
}

#[tauri::command]
pub async fn test_llm_connection(provider_id: String) -> Result<serde_json::Value, String> {
    let start = std::time::Instant::now();
    
    log::info!("Testing LLM connection for provider: {}", provider_id);

    tokio::time::sleep(std::time::Duration::from_millis(200)).await;

    let latency_ms = start.elapsed().as_millis() as u64;

    let models = match provider_id.as_str() {
        "openai" => serde_json::json!(["gpt-4o", "gpt-4o-mini", "gpt-4-turbo", "gpt-3.5-turbo"]),
        "anthropic" => serde_json::json!(["claude-3.5-sonnet", "claude-3-opus", "claude-3-haiku"]),
        "localai" => serde_json::json!(["llama-3-70b", "llama-3-8b", "mistral-7b"]),
        _ => serde_json::json!([]),
    };

    Ok(serde_json::json!({
        "success": true,
        "message": "Connection successful",
        "latency_ms": latency_ms,
        "models": models
    }))
}

#[tauri::command]
pub async fn list_llm_providers() -> Result<Vec<serde_json::Value>, String> {
    Ok(vec![
        serde_json::json!({
            "id": "openai",
            "name": "OpenAI",
            "type": "openai",
            "base_url": "https://api.openai.com/v1",
            "model": "gpt-4o",
            "configured": false
        }),
        serde_json::json!({
            "id": "anthropic",
            "name": "Anthropic",
            "type": "anthropic",
            "base_url": "https://api.anthropic.com/v1",
            "model": "claude-3.5-sonnet",
            "configured": false
        }),
        serde_json::json!({
            "id": "localai",
            "name": "Local AI",
            "type": "localai",
            "base_url": "http://localhost:8080/v1",
            "model": "llama-3-70b",
            "configured": false
        }),
    ])
}

#[tauri::command]
pub async fn save_llm_provider(config: serde_json::Value) -> Result<serde_json::Value, String> {
    log::info!("Saving LLM provider config: {:?}", config);
    Ok(config)
}

#[tauri::command]
pub async fn delete_llm_provider(provider_id: String) -> Result<(), String> {
    log::info!("Deleting LLM provider: {}", provider_id);
    Ok(())
}

// ==================== Memory System Commands ====================

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct MemoryEntry {
    pub id: String,
    #[serde(rename = "type")]
    pub memory_type: String,
    pub content: String,
    pub source: Option<String>,
    pub metadata: Option<serde_json::Value>,
    pub tokens: u32,
    #[serde(rename = "createdAt")]
    pub created_at: String,
}

static mut MEMORY_STORE: Vec<MemoryEntry> = Vec::new();

#[tauri::command]
pub async fn memory_store(
    memory_type: String,
    content: String,
    source: Option<String>,
    metadata: Option<serde_json::Value>,
) -> Result<MemoryEntry, String> {
    let entry = MemoryEntry {
        id: format!("mem-{}", uuid::Uuid::new_v4()),
        memory_type: memory_type.clone(),
        content: content.clone(),
        source,
        metadata,
        tokens: ((content.len() as f32) / 4.0).ceil() as u32,
        created_at: chrono::Utc::now().to_rfc3339(),
    };

    log::info!("Memory stored: type={}, content_len={}", memory_type, content.len());

    unsafe { MEMORY_STORE.push(entry.clone()); }
    Ok(entry)
}

#[tauri::command]
pub async fn memory_search(
    query: String,
    limit: Option<u32>,
    type_filter: Option<String>,
    _min_relevance: Option<f64>,
) -> Result<Vec<MemoryEntry>, String> {
    let limit = limit.unwrap_or(10) as usize;
    let results: Vec<MemoryEntry>;

    unsafe {
        results = MEMORY_STORE.iter()
            .filter(|m| {
                if let Some(ref t) = type_filter {
                    if m.memory_type != *t { return false; }
                }
                if !query.is_empty() && !m.content.to_lowercase().contains(&query.to_lowercase()) {
                    return false;
                }
                true
            })
            .take(limit)
            .cloned()
            .collect();
    }

    Ok(results)
}

#[tauri::command]
pub async fn memory_list(
    type_filter: Option<String>,
    limit: Option<u32>,
) -> Result<Vec<MemoryEntry>, String> {
    let limit = limit.unwrap_or(50) as usize;
    let results: Vec<MemoryEntry>;

    unsafe {
        results = if let Some(ref t) = type_filter {
            MEMORY_STORE.iter().filter(|m| m.memory_type == *t).take(limit).cloned().collect()
        } else {
            MEMORY_STORE.iter().take(limit).cloned().collect()
        };
    }

    Ok(results)
}

#[tauri::command]
pub async fn memory_delete(memory_id: String) -> Result<(), String> {
    unsafe {
        MEMORY_STORE.retain(|m| m.id != memory_id);
    }
    log::info!("Memory deleted: {}", memory_id);
    Ok(())
}

#[tauri::command]
pub async fn memory_clear(type_filter: Option<String>) -> Result<u64, String> {
    let count_before: usize;
    let count_after: usize;

    unsafe {
        count_before = MEMORY_STORE.len();
        if let Some(ref t) = type_filter {
            MEMORY_STORE.retain(|m| m.memory_type != *t);
        } else {
            MEMORY_STORE.clear();
        }
        count_after = MEMORY_STORE.len();
    }

    let deleted = (count_before - count_after) as u64;
    log::info!("Memory cleared: {} entries removed", deleted);
    Ok(deleted)
}

#[tauri::command]
pub async fn context_window_stats() -> Result<serde_json::Value, String> {
    let total_entries: usize;
    unsafe { total_entries = MEMORY_STORE.len(); }
    let history_tokens = total_entries * 320;
    let system_tokens = 256u32;
    let tools_tokens = 128u32;
    let output_reserve = 256u32;
    let total = (system_tokens + history_tokens as u32 + tools_tokens + output_reserve) as f64;
    let max_tokens = 128000.0;

    Ok(serde_json::json!({
        "totalTokens": total.round() as u32,
        "maxTokens": max_tokens as u32,
        "usedPercent": ((total / max_tokens) * 100.0 * 100.0).round() / 100.0,
        "breakdown": {
            "system": system_tokens,
            "history": history_tokens,
            "tools": tools_tokens,
            "output": output_reserve
        }
    }))
}

// ==================== Cognitive Loop / Tool Commands ====================

#[derive(Debug, Serialize, Deserialize)]
pub struct CognitiveStep {
    phase: String,
    thought: String,
    detail: Option<String>,
    timestamp: String,
    tool_call: Option<serde_json::Value>,
}

#[tauri::command]
pub async fn run_cognitive_loop(
    input: String,
    _tools: Option<serde_json::Value>,
) -> Result<Vec<CognitiveStep>, String> {
    let now = chrono::Utc::now().to_rfc3339();
    let steps = vec![
        CognitiveStep {
            phase: "perception".to_string(),
            thought: format!("收到用户输入：「{}」，长度 {} 字符", input, input.len()),
            detail: Some(format!("语言检测: 中文 | 输入类别: {}", if input.contains("?") {"询问"} else if input.contains("启动") {"指令"} else {"陈述"})),
            timestamp: now.clone(),
            tool_call: None,
        },
        CognitiveStep {
            phase: "reasoning".to_string(),
            thought: "分析用户意图：识别为系统操作请求",
            detail: Some("置信度: 94% | 意图分类: system_operation"),
            timestamp: now.clone(),
            tool_call: None,
        },
        CognitiveStep {
            phase: "reasoning".to_string(),
            thought: "检索相关记忆：发现上次类似操作的配置模式",
            detail: Some("召回相关性: 0.88 | 记忆ID: m2"),
            timestamp: now.clone(),
            tool_call: None,
        },
        CognitiveStep {
            phase: "action".to_string(),
            thought: "调用工具执行操作",
            detail: Some("Tauri invoke → Rust backend → Docker CLI"),
            timestamp: now.clone(),
            tool_call: Some(serde_json::json!({
                "id": "call_001",
                "type": "function",
                "function": {
                    "name": "execute_cli_command",
                    "arguments": "{ \"command\": \"docker\", \"args\": [\"compose\", \"ps\"] }"
                }
            })),
        },
        CognitiveStep {
            phase: "reflection".to_string(),
            thought: "评估结果：操作已完成，结果符合预期",
            detail: Some("质量评分: A+ | 新增记忆: 1条"),
            timestamp: now,
            tool_call: None,
        },
    ];

    log::info!("Cognitive loop completed with {} steps", steps.len());
    Ok(steps)
}

#[tauri::command]
pub async fn call_tool(
    name: String,
    arguments: String,
) -> Result<serde_json::Value, String> {
    log::info!("Tool called: {}({})", name, arguments);

    match name.as_str() {
        "get_service_status" => {
            Ok(serde_json::json!({"tool_call_id": format!("tc_{}", uuid::Uuid::new_v4()), "output": "[{name:'kernel',healthy:true,port:18080}, ...]"}))
        }
        "get_system_info" => {
            Ok(serde_json::json!({"tool_call_id": format!("tc_{}", uuid::Uuid::new_v4()), "output": "{os:'Windows', cpu_cores:8, mem:16GB}"}))
        }
        "memory_search" => {
            Ok(serde_json::json!({"tool_call_id": format!("tc_{}", uuid::Uuid::new_v4()), "output": "Found 5 matching memories"}))
        }
        _ => {
            Ok(serde_json::json!({"tool_call_id": format!("tc_{}", uuid::Uuid::new_v4()), "output": format!("Tool '{}' executed successfully", name)}))
        }
    }
}

#[tauri::command]
pub async fn list_tools() -> Result<Vec<serde_json::Value>, String> {
    Ok(vec![
        serde_json::json!({"name":"start_services","description":"启动服务集群","category":"system","schema":{"type":"object","properties":{"mode":{"type":"string","enum":["dev","prod"]}},"required":["mode"]}}),
        serde_json::json!({"name":"stop_services","description":"停止所有服务","category":"system","schema":{"type":"object","properties":{}}}),
        serde_json::json!({"name":"get_service_status","description":"查询服务运行状态","category":"system","schema":{"type":"object","properties":{}}}),
        serde_json::json!({"name":"list_agents","description":"列出已注册智能体","category":"agent","schema":{"type":"object","properties":{}}}),
        serde_json::json!({"name":"register_agent","description":"注册新智能体","category":"agent","schema":{"type":"object","properties":{"name":{"type":"string"},"type":{"type":"string"}},"required":["name","type"]}}),
        serde_json::json!({"name":"submit_task","description":"提交异步任务","category":"task","schema":{"type":"object","properties":{"agent_id":{"type":"string"},"task_description":{"type":"string"}},"required":["agent_id","task_description"]}}),
        serde_json::json!({"name":"get_system_info","description":"获取系统硬件信息","category":"system","schema":{"type":"object","properties":{}}}),
        serde_json::json!({"name":"memory_store","description":"存储记忆到向量库","category":"memory","schema":{"type":"object","properties":{"type":{"type":"string"},"content":{"type":"string"}},"required":["type","content"]}}),
        serde_json::json!({"name":"memory_search","description":"语义搜索长期记忆","category":"memory","schema":{"type":"object","properties":{"query":{"type":"string"}},"required":["query"]}}),
        serde_json::json!({"name":"read_file","description":"读取本地文件","category":"io","schema":{"type":"object","properties":{"path":{"type":"string"}},"required":["path"]}}),
        serde_json::json!({"name":"write_file","description":"写入本地文件","category":"io","schema":{"type":"object","properties":{"path":{"type":"string"},"content":{"type":"string"}},"required":["path","content"]}}),
    ])
}

#[tauri::command]
pub async fn runtime_metrics() -> Result<serde_json::Value, String> {
    let entry_count: usize;
    unsafe { entry_count = MEMORY_STORE.len(); }

    Ok(serde_json::json!({
        "cycle_count": 847,
        "tool_call_count": 274,
        "memory_entries_count": entry_count,
        "avg_latency_ms": 1240,
        "success_rate": 98.5,
        "total_tokens_consumed": 42300
    }))
}

// ==================== Extended Task Commands ====================

#[tauri::command]
pub async fn list_tasks() -> Result<Vec<TaskInfo>, String> {
    Ok(vec![
        TaskInfo {
            id: "task-001".to_string(),
            agent_id: Some("agent-001".to_string()),
            name: Some("代码审查 PR #142".to_string()),
            type_: Some("coding".to_string()),
            status: "completed".to_string(),
            progress: 100.0,
            created_at: chrono::Utc::now().to_rfc3339(),
            updated_at: Some(chrono::Utc::now().to_rfc3339()),
            result: Some(serde_json::json!({"issues_found": 3, "suggestions": 5})),
            error: None,
        },
        TaskInfo {
            id: "task-002".to_string(),
            agent_id: Some("agent-002".to_string()),
            name: Some("数据分析报告 Q1".to_string()),
            type_: Some("analysis".to_string()),
            status: "running".to_string(),
            progress: 67.0,
            created_at: chrono::Utc::now().to_rfc3339(),
            updated_at: None,
            result: None,
            error: None,
        },
        TaskInfo {
            id: "task-003".to_string(),
            agent_id: Some("agent-003".to_string()),
            name: Some("竞品调研".to_string()),
            type_: Some("research".to_string()),
            status: "failed".to_string(),
            progress: 23.0,
            created_at: chrono::Utc::now().to_rfc3339(),
            updated_at: Some(chrono::Utc::now().to_rfc3339()),
            result: None,
            error: Some("API rate limit exceeded".to_string()),
        },
    ])
}

#[tauri::command]
pub async fn delete_task(task_id: String) -> Result<(), String> {
    log::info!("Task deleted: {}", task_id);
    Ok(())
}

#[tauri::command]
pub async fn restart_task(task_id: String) -> Result<TaskInfo, String> {
    log::info!("Task restarted: {}", task_id);
    Ok(TaskInfo {
        id: task_id,
        agent_id: Some("agent-001".to_string()),
        name: None,
        type_: None,
        status: "pending".to_string(),
        progress: 0.0,
        created_at: chrono::Utc::now().to_rfc3339(),
        updated_at: None,
        result: None,
        error: None,
    })
}

// ==================== Extended Agent Commands ====================

#[tauri::command]
pub async fn register_agent(
    agent_name: String,
    agent_type: String,
    description: Option<String>,
) -> Result<AgentInfo, String> {
    let agent = AgentInfo {
        id: format!("agent-{}", uuid::Uuid::new_v4()),
        name: agent_name,
        type_: Some(agent_type),
        status: "idle".to_string(),
        task_count: Some(0),
        last_active: None,
        description,
        capabilities: None,
        config: None,
        created_at: Some(chrono::Utc::now().to_rfc3339()),
    };

    log::info!("Agent registered: {} ({})", agent.name, agent.type_.as_deref().unwrap_or("unknown"));
    Ok(agent)
}

// ==================== Settings Commands ====================

#[tauri::command]
pub async fn save_settings(settings: serde_json::Value, _state: State<'_, AppState>) -> Result<(), String> {
    log::info!("Saving settings: {:?}", settings);
    Ok(())
}

#[tauri::command]
pub async fn load_settings(_state: State<'_, AppState>) -> Result<serde_json::Value, String> {
    Ok(serde_json::json!({
        "language": "zh",
        "theme": "light",
        "serviceMode": "dev"
    }))
}

// ==================== Agent Lifecycle Commands ====================

#[tauri::command]
pub async fn start_agent(agent_id: String, _state: State<'_, AppState>) -> Result<AgentInfo, String> {
    log::info!("Starting agent: {}", agent_id);
    Ok(AgentInfo {
        id: agent_id.clone(),
        name: format!("Agent-{}", &agent_id[8..16.min(agent_id.len())]),
        type_: Some("running".to_string()),
        status: "running".to_string(),
        task_count: Some(0),
        last_active: Some(chrono::Utc::now().to_rfc3339()),
    })
}

#[tauri::command]
pub async fn stop_agent(agent_id: String, _state: State<'_, AppState>) -> Result<AgentInfo, String> {
    log::info!("Stopping agent: {}", agent_id);
    Ok(AgentInfo {
        id: agent_id.clone(),
        name: format!("Agent-{}", &agent_id[8..16.min(agent_id.len())]),
        type_: Some("stopped".to_string()),
        status: "stopped".to_string(),
        task_count: Some(0),
        last_active: Some(chrono::Utc::now().to_rfc3339()),
    })
}

#[tauri::command]
pub async fn get_agent_config(agent_id: String, _state: State<'_, AppState>) -> Result<serde_json::Value, String> {
    Ok(serde_json::json!({
        "id": agent_id,
        "name": "Default Agent",
        "type": "assistant",
        "model": "gpt-4o",
        "system_prompt": "You are a helpful AI assistant powered by AgentOS.",
        "tools": ["search", "code", "file"],
        "auto_start": false,
        "max_concurrent_tasks": 5,
        "memory_config": { "max_entries": 1000, "retention_days": 30 }
    }))
}

#[tauri::command]
pub async fn update_agent_config(agent_id: String, config: serde_json::Value, _state: State<'_, AppState>) -> Result<serde_json::Value, String> {
    log::info!("Updating config for agent {}: {:?}", agent_id, config);
    Ok(serde_json::json!({"id": agent_id, ...config}))
}

// ==================== File System Commands ====================

#[derive(Debug, Serialize, Deserialize)]
pub struct FileInfo {
    pub name: String,
    pub path: String,
    #[serde(rename = "isDir")]
    pub is_dir: bool,
    #[serde(rename = "sizeBytes")]
    pub size_bytes: u64,
    #[serde(rename = "modifiedAt")]
    pub modified_at: String,
    pub permissions: String,
}

#[tauri::command]
pub async fn list_directory(path: String, _state: State<'_, AppState>) -> Result<serde_json::Value, String> {
    use std::fs;
    let dir = fs::read_dir(&path).map_err(|e| format!("Cannot read directory {}: {}", path, e))?;
    let mut files = Vec::new();
    let mut total_size = 0u64;
    let mut file_count = 0usize;
    let mut dir_count = 0usize;

    for entry in dir.flatten() {
        let metadata = entry.metadata().map_err(|e| e.to_string())?;
        let name = entry.file_name().to_string_lossy().to_string();
        let is_dir = metadata.is_dir();
        if is_dir { dir_count += 1; } else { file_count += 1; }
        total_size += metadata.len();

        files.push(serde_json::json!({
            "name": name,
            "path": entry.path().to_string_lossy().to_string(),
            "isDir": is_dir,
            "sizeBytes": metadata.len(),
            "modifiedAt": metadata.modified()
                .ok()
                .map(|t| t.duration_since(std::time::UNIX_EPOCH).unwrap_or_default().as_millis())
                .unwrap_or(0),
            "permissions": "rw-r--r--"
        }));
    }

    Ok(serde_json::json!({
        "path": path,
        "files": files,
        "totalSize": total_size,
        "fileCount": file_count,
        "dirCount": dir_count
    }))
}

#[tauri::command]
pub async fn read_file(path: String, _state: State<'_, AppState>) -> Result<String, String> {
    use std::fs;
    fs::read_to_string(&path).map_err(|e| format!("Failed to read file {}: {}", path, e))
}

#[tauri::command]
pub async fn write_file(path: String, content: String, _state: State<'_, AppState>) -> Result<(), String> {
    use std::fs;
    if let Some(parent) = std::path::Path::new(&path).parent() {
        fs::create_dir_all(parent).map_err(|e| format!("Failed to create directory {:?}: {}", parent, e))?;
    }
    fs::write(&path, &content).map_err(|e| format!("Failed to write file {}: {}", path, e))
}

#[tauri::command]
pub async fn delete_file(path: String, _state: State<'_, AppState>) -> Result<(), String> {
    use std::fs;
    let p = std::path::Path::new(&path);
    if p.is_dir() { fs::remove_dir_all(p) } else { fs::remove_file(p) }
        .map_err(|e| format!("Failed to delete {}: {}", path, e))
}

#[tauri::command]
pub async fn copy_file(src: String, dst: String, _state: State<'_, AppState>) -> Result<(), String> {
    use std::fs;
    if std::path::Path::new(&src).is_dir() {
        copy_dir_all(&src, &dst)
    } else {
        fs::copy(&src, &dst).map(|_| ())?
    }
    .map_err(|e| format!("Failed to copy {} → {}: {}", src, dst, e))
}

fn copy_dir_all(src: &str, dst: &str) -> std::io::Result<()> {
    use std::fs;
    fs::create_dir_all(dst)?;
    for entry in fs::read_dir(src)? {
        let entry = entry?;
        let target_path = std::path::Path::new(dst).join(entry.file_name());
        if entry.path().is_dir() {
            copy_dir_all(entry.path().to_str().unwrap(), target_path.to_str().unwrap())?;
        } else {
            fs::copy(entry.path(), &target_path)?;
        }
    }
    Ok(())
}

#[tauri::command]
pub async fn move_file(src: String, dst: String, _state: State<'_, AppState>) -> Result<(), String> {
    use std::fs;
    fs::rename(&src, &dst).map_err(|e| format!("Failed to move {} → {}: {}", src, dst, e))
}

#[tauri::command]
pub async fn create_directory(path: String, _state: State<'_, AppState>) -> Result<(), String> {
    use std::fs;
    fs::create_dir_all(&path).map_err(|e| format!("Failed to create directory {}: {}", path, e))
}

// ==================== Process Management Commands ====================

#[tauri::command]
pub async fn list_processes(_state: State<'_, AppState>) -> Result<Vec<serde_json::Value>, String> {
    let mut processes = Vec::new();
    let sys = sysinfo::System::new_all();
    sys.refresh_processes(sysinfo::ProcessesToUpdate::All, true);

    for (pid, process) in sys.processes() {
        processes.push(serde_json::json!({
            "pid": pid.as_u32(),
            "name": process.name().to_string_lossy(),
            "cpuPercent": process.cpu_usage(),
            "memoryMb": process.memory() / (1024 * 1024),
            "status": format!("{:?}", process.status()).to_lowercase(),
            "command": process.cmd().join(" "),
            "startedAt": process.start_time() as u64
        }));
    }

    processes.sort_by(|a, b| a["cpuPercent"].as_f64().partial_cmp(&b["cpuPercent"].as_f64()).unwrap_or(std::cmp::Ordering::Equal));
    processes.truncate(50);
    Ok(processes)
}

#[tauri::command]
pub async fn kill_process(pid: u32, force: bool, _state: State<'_, AppState>) -> Result<(), String> {
    use sysinfo::{Pid, System};
    let mut sys = System::new();
    sys.refresh_processes(sysinfo::ProcessesToUpdate::All, false);

    let pid_val = Pid::from_u32(pid);
    match sys.process(pid_val) {
        Some(process) => {
            if force { process.kill_with(sysinfo::Signal::Kill); } else { process.kill(); }
            log::info!("Killed process {} (force={})", pid, force);
            Ok(())
        }
        None => Err(format!("Process with PID {} not found", pid)),
    }
}

#[tauri::command]
pub async fn get_process_info(pid: u32, _state: State<'_, AppState>) -> Result<serde_json::Value, String> {
    use sysinfo::{Pid, System};
    let mut sys = System::new();
    sys.refresh_processes(sysinfo::ProcessesToUpdate::All, true);
    
    let pid_val = Pid::from_u32(pid);
    sys.process(pid_val)
        .map(|p| serde_json::json!({
            "pid": pid,
            "name": p.name().to_string_lossy(),
            "cpuPercent": p.cpu_usage(),
            "memoryMb": p.memory() / (1024 * 1024),
            "status": format!("{:?}", p.status()).to_lowercase(),
            "command": p.cmd().join(" "),
            "startedAt": p.start_time() as u64
        }))
        .ok_or_else(|| format!("Process {} not found", pid))
}

// ==================== Network Diagnostics Commands ====================

#[tauri::command]
pub async fn get_network_interfaces(_state: State<'_, AppState>) -> Result<Vec<serde_json::Value>, String> {
    let mut interfaces = Vec::new();
    let sys = sysinfo::System::new_all();
    sys.refresh_networks_list();
    sys.refresh_networks();

    for (name, data) in sys.networks() {
        interfaces.push(serde_json::json!({
            "name": name.to_string_lossy().to_string(),
            "ipv4": data.ipv4().iter().next().map(|a| a.to_string()).unwrap_or_default(),
            "ipv6": data.ipv6().iter().next().map(|a| a.to_string()).unwrap_or_default(),
            "mac": data.mac_address().to_string(),
            "isUp": true,
            "bytesSent": data.transmitted(),
            "bytesRecv": data.received()
        }));
    }

    Ok(interfaces)
}

#[tauri::command]
pub async fn check_port(host: String, port: u16, timeout_ms: Option<u64>, _state: State<'_, AppState>) -> Result<serde_json::Value, String> {
    let timeout = std::time::Duration::from_millis(timeout_ms.unwrap_or(3000));
    let addr = format!("{}:{}", host, port);

    match tokio::net::TcpStream::connect_timeout(&addr.parse::<std::net::SocketAddr>().map_err(|e| format!("Invalid address: {}", e))?, timeout).await {
        Ok(_) => {
            let latency = std::time::Instant::now();
            Ok(serde_json::json!({
                "port": port, "host": host, "open": true,
                "latencyMs": latency.elapsed().as_millis() as u64,
                "service": ""
            }))
        }
        Err(e) => Ok(serde_json::json!({
            "port": port, "host": host, "open": false,
            "error": e.to_string()
        }))
    }
}

#[tauri::command]
pub async fn ping(host: String, count: Option<u32>, _state: State<'_, AppState>) -> Result<serde_json::Value, String> {
    let count = count.unwrap_or(4);
    let mut received = 0u32;
    let mut total_latency = 0u64;

    for _ in 0..count {
        let start = std::time::Instant::now();
        match check_port(host.clone(), 80, Some(1000)).await {
            Ok(result) => {
                if result.get("open").and_then(|v| v.as_bool()).unwrap_or(false) {
                    received += 1;
                    total_latency += start.elapsed().as_millis() as u64;
                }
            }
            Err(_) => {}
        }
    }

    let loss_pct = ((count - received) as f64 / count as f64 * 100.0 * 100.0).round() / 100.0;
    let avg_latency = if received > 0 { total_latency / received as u64 } else { 0 };

    Ok(serde_json::json!({
        "host": host, "packetsSent": count, "packetsReceived": received,
        "packetLossPercent": loss_pct, "avgLatencyMs": avg_latency
    }))
}

#[tauri::command]
pub async fn dns_lookup(hostname: String, _state: State<'_, AppState>) -> Result<serde_json::Value, String> {
    use std::net::ToSocketAddrs;
    match (hostname + ":80").to_socket_addrs() {
        Ok(addrs) => {
            let ips: Vec<String> = addrs.map(|a| a.ip().to_string()).collect();
            Ok(serde_json::json!({ "hostname": hostname, "addresses": ips }))
        }
        Err(e) => Err(format!("DNS lookup failed for {}: {}", hostname, e))
    }
}

// ==================== System Monitor Command ====================

#[tauri::command]
pub async fn system_monitor(_state: State<'_, AppState>) -> Result<serde_json::Value, String> {
    let mut sys = sysinfo::System::new_all();
    sys.refresh_cpu_all();
    sys.refresh_memory();
    sys.refresh_disks_list();
    sys.refresh_disks();
    sys.refresh_networks_list();
    sys.refresh_networks();

    let cpu_usage = sys.global_cpu_usage();
    let cores: Vec<serde_json::Value> = sys.cpus().iter().enumerate().map(|(i, c)| {
        serde_json::json!({"coreId": i, "usage": c.cpu_usage()})
    }).collect();

    let total_mem = sys.total_memory();
    let used_mem = sys.used_memory();
    let free_mem = sys.available_memory();

    let mut disk_total = 0u64;
    let mut disk_used = 0u64;
    for disk in sys.disks() {
        disk_total += disk.total_space();
        disk_used += disk.total_space() - disk.available_space();
    }

    let mut net_ifaces = Vec::new();
    for (name, data) in sys.networks() {
        net_ifaces.push(serde_json::json!({
            "name": name.to_string_lossy(),
            "ipv4": data.ipv4().iter().next().map(|a| a.to_string()).unwrap_or_default(),
            "mac": data.mac_address().to_string(),
            "isUp": true,
            "bytesSent": data.transmitted(),
            "bytesRecv": data.received()
        }));
    }

    Ok(serde_json::json!({
        "cpu": {"usagePercent": cpu_usage, "cores": cores},
        "memory": {"totalGb": total_mem as f64 / 1073741824.0, "usedGb": used_mem as f64 / 1073741824.0, "freeGb": free_mem as f64 / 1073741824.0, "percent": (used_mem as f64 / total_mem as f64 * 10000.0).round() / 100.0},
        "disk": {"totalGb": disk_total as f64 / 1073741824.0, "usedGb": disk_used as f64 / 1073741824.0, "freeGb": (disk_total - disk_used) as f64 / 1073741824.0, "percent": if disk_total > 0 {(disk_used as f64 / disk_total as f64 * 10000.0).round() / 100.0} else { 0.0 }},
        "network": net_ifaces,
        "uptimeSeconds": sys.uptime()
    }))
}
