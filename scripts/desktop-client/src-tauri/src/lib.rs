mod cli;
mod commands;

use tauri::Manager;

pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_shell::init())
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
        ])
        .setup(|app| {
            log::info!("AgentOS Desktop Client starting...");

            #[cfg(debug_assertions)]
            {
                let window = app.get_webview_window("main").unwrap();
                window.open_devtools();
            }

            Ok(())
        })
        .run(tauri::generate_context!())
        .expect("error while running AgentOS Desktop");
}
