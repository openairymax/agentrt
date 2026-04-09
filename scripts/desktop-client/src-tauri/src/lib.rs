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
