mod cli;
mod commands;

use tauri::Manager;

pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_shell::init())
        .plugin(tauri_plugin_updater::Builder::new().build())
        .plugin(tauri_plugin_dialog::init())
        .invoke_handler(tauri::generate_handler![
            commands::get_system_info,
            commands::execute_cli_command,
            commands::get_service_status,
            commands::start_services,
            commands::stop_services,
            commands::restart_services,
            commands::get_logs,
            commands::get_health_status,
            commands::read_config_file,
            commands::write_config_file,
            commands::list_agents,
            commands::get_agent_details,
            commands::submit_task,
            commands::get_task_status,
            commands::cancel_task,
            commands::open_terminal,
            commands::open_browser,
            commands::check_for_updates,
            commands::get_version_info,
            commands::download_and_install_update,
            // LLM / AI Chat
            commands::llm_chat,
            commands::test_llm_connection,
            commands::list_llm_providers,
            commands::save_llm_provider,
            commands::delete_llm_provider,
            // Memory System
            commands::memory_store,
            commands::memory_search,
            commands::memory_list,
            commands::memory_delete,
            commands::memory_clear,
            commands::context_window_stats,
            // Cognitive Loop / Tools
            commands::run_cognitive_loop,
            commands::call_tool,
            commands::list_tools,
            commands::runtime_metrics,
            // Extended Tasks
            commands::list_tasks,
            commands::delete_task,
            commands::restart_task,
            // Extended Agents
            commands::register_agent,
            // Settings
            commands::save_settings,
            commands::load_settings,
            // Agent Lifecycle
            commands::start_agent,
            commands::stop_agent,
            commands::get_agent_config,
            commands::update_agent_config,
            // File System Operations
            commands::list_directory,
            commands::read_file,
            commands::write_file,
            commands::delete_file,
            commands::copy_file,
            commands::move_file,
            commands::create_directory,
            // Process Management
            commands::list_processes,
            commands::kill_process,
            commands::get_process_info,
            // Network Diagnostics
            commands::get_network_interfaces,
            commands::check_port,
            commands::ping,
            commands::dns_lookup,
            // System Monitor
            commands::system_monitor,
        ])
        .setup(|app| {
            log::info!("AgentOS Desktop Client starting...");

            #[cfg(debug_assertions)]
            {
                let window = app.get_webview_window("main").unwrap();
                window.open_devtools();
            }

            let handle = app.handle().clone();
            tauri::async_runtime::spawn(async move {
                if let Err(e) = check_update_on_startup(handle).await {
                    log::warn!("Failed to check for updates on startup: {}", e);
                }
            });

            Ok(())
        })
        .run(tauri::generate_context!())
        .expect("error while running AgentOS Desktop");
}

async fn check_update_on_startup(app: tauri::AppHandle) -> Result<(), Box<dyn std::error::Error>> {
    use tauri_plugin_updater::UpdaterExt;
    
    let updater = app.updater()?;
    
    match updater.check().await? {
        Some(update) => {
            log::info!(
                "Update available: {} -> {}",
                update.current_version,
                update.version
            );
        }
        None => {
            log::info!("No updates available");
        }
    }
    
    Ok(())
}
