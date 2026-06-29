// Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
//
// AgentRT TUI - Main entry point
//
// Terminal-based user interface for AgentRT.
// Communicates with the gateway via HTTP API for all runtime operations.
//
// Logging:
//   All diagnostic logs go to stderr (via env_logger) so they don't
//   interfere with the TUI rendering on stdout. Use:
//     RUST_LOG=debug agentrt-tui          # verbose
//     RUST_LOG=info agentrt-tui           # normal (default)
//     RUST_LOG=agentrt_tui=debug agentrt-tui  # only our logs

mod app;
mod client;
mod panels;
mod ui;

use anyhow::Result;
use clap::Parser;
use crossterm::{
    event::{self, DisableMouseCapture, EnableMouseCapture, Event, KeyCode, KeyEventKind},
    execute,
    terminal::{disable_raw_mode, enable_raw_mode, EnterAlternateScreen, LeaveAlternateScreen},
};
use log::{debug, error, info, warn};
use ratatui::prelude::*;
use std::io;
use std::time::Instant;

use crate::app::{ActivePanel, App};
use crate::client::GatewayClient;

/// AgentRT Terminal User Interface
#[derive(Parser)]
#[command(
    name = "agentrt-tui",
    version = env!("CARGO_PKG_VERSION"),
    about = "AgentRT Terminal User Interface",
)]
struct Cli {
    /// Gateway API base URL
    #[arg(long, env = "AGENTRT_GATEWAY_URL", default_value = "http://localhost:8080")]
    gateway_url: String,

    /// Agent definition file
    #[arg(short, long, default_value = "agents/main.agent.yaml")]
    agent_file: String,
}

#[tokio::main]
async fn main() {
    // ── Phase 0: Initialize logging immediately ──
    env_logger::Builder::from_env(
        env_logger::Env::default().default_filter_or("info")
    )
    .format_timestamp_millis()
    .target(env_logger::Target::Stderr) // don't trash the TUI
    .init();

    info!("══════════════════════════════════════════");
    info!("  AgentRT TUI v{} starting", env!("CARGO_PKG_VERSION"));
    info!("══════════════════════════════════════════");

    let cli = Cli::parse();
    info!("CLI args parsed:");
    info!("  gateway_url = {}", cli.gateway_url);
    info!("  agent_file  = {}", cli.agent_file);

    // ── Phase 1: Pre-flight checks ──
    let start_time = Instant::now();

    // Check if agent file exists
    if !std::path::Path::new(&cli.agent_file).exists() {
        warn!("Agent file '{}' not found on disk, will pass name to gateway",
              cli.agent_file);
    }

    // ── Phase 2: Gateway connection ──
    info!("Connecting to gateway at {}...", cli.gateway_url);
    let gateway = match GatewayClient::new(&cli.gateway_url) {
        Ok(gw) => {
            info!("HTTP client initialized (base={})", cli.gateway_url);
            gw
        }
        Err(e) => {
            error!("Failed to create HTTP client: {}", e);
            error!("  → Check that gateway_url is valid: '{}'", cli.gateway_url);
            error!("  → Hint: is the gateway running? Try: docker compose up -d");
            eprintln!("\n❌ Cannot create HTTP client: {}\n", e);
            std::process::exit(1);
        }
    };

    // Probe gateway health
    match gateway.health_check().await {
        Ok(h) => {
            info!("Gateway health check: status={}, version={:?}",
                  h.status, h.version);
            info!("Gateway connection established in {:?}", start_time.elapsed());
        }
        Err(e) => {
            warn!("Gateway health check failed: {}", e);
            warn!("TUI will start but commands won't work until gateway is up.");
            warn!("  → Start gateway:  docker compose up -d  or  agentos-gateway_d");
        }
    }

    // ── Phase 3: Start TUI ──
    info!("Setting up terminal (raw mode + alternate screen)...");

    let tui_result = run_tui(&cli, gateway).await;

    // ── Phase 4: Shutdown diagnostics ──
    let total_time = start_time.elapsed();
    match &tui_result {
        Ok(()) => info!("TUI exited normally after {:?}", total_time),
        Err(e) => {
            error!("TUI exited with error after {:?}: {}", total_time, e);
            error!("  → Full error chain: {:#}", e);
            eprintln!("\n❌ TUI error: {}\n", e);
        }
    }
    info!("AgentRT TUI shutdown complete");

    if let Err(e) = tui_result {
        std::process::exit(1);
    }
}

async fn run_tui(cli: &Cli, gateway: GatewayClient) -> Result<()> {
    // Setup terminal
    enable_raw_mode()
        .map_err(|e| {
            error!("Failed to enable raw mode: {}", e);
            error!("  → This usually means you're not in a real terminal.");
            error!("  → Try running in a terminal emulator, not an IDE panel.");
            e
        })?;

    let mut stdout = io::stdout();
    execute!(stdout, EnterAlternateScreen, EnableMouseCapture)
        .map_err(|e| {
            error!("Failed to enter alternate screen: {}", e);
            e
        })?;

    let backend = CrosstermBackend::new(stdout);
    let mut terminal = Terminal::new(backend)
        .map_err(|e| {
            error!("Failed to create terminal backend: {}", e);
            e
        })?;

    info!("Terminal initialized. Starting event loop.");

    // Create app state
    let mut app = App::new(&cli.agent_file, gateway);

    // ── Phase 3b: Deferred connection check in app ──
    if let Err(e) = app.check_connection().await {
        debug!("Initial connection check returned error (non-fatal): {}", e);
    }
    info!("App state initialized. connected={}, version={:?}",
          app.connected, app.gateway_version);

    // Main event loop
    let result = run_app(&mut terminal, &mut app).await;

    // Restore terminal
    info!("Restoring terminal...");
    disable_raw_mode()?;
    execute!(
        terminal.backend_mut(),
        LeaveAlternateScreen,
        DisableMouseCapture
    )?;
    terminal.show_cursor()?;
    info!("Terminal restored.");

    result?;
    Ok(())
}

async fn run_app<B: Backend>(
    terminal: &mut Terminal<B>,
    app: &mut App,
) -> Result<()> {
    loop {
        terminal.draw(|f| ui::render(f, app))?;

        if let Event::Key(key) = event::read()? {
            if key.kind != KeyEventKind::Press {
                continue;
            }

            match key.code {
                KeyCode::Char('c') | KeyCode::Char('C')
                    if key.modifiers.contains(event::KeyModifiers::CONTROL) => {
                        info!("User pressed Ctrl+C, shutting down...");
                        app.shutdown().await?;
                        return Ok(());
                    }
                KeyCode::Esc
                    if app.active_panel != ActivePanel::Chat => {
                        debug!("Panel: Esc → return to Chat");
                        app.active_panel = ActivePanel::Chat;
                    }
                KeyCode::F(1) => {
                    debug!("Panel: toggle Help");
                    app.toggle_panel(ActivePanel::Help);
                }
                KeyCode::F(2) => {
                    debug!("Panel: toggle Config");
                    app.toggle_panel(ActivePanel::Config);
                }
                KeyCode::F(3) => {
                    debug!("Panel: toggle Logs");
                    app.toggle_panel(ActivePanel::Logs);
                }
                KeyCode::F(4) => {
                    debug!("Panel: toggle Memory");
                    app.toggle_panel(ActivePanel::Memory);
                }
                KeyCode::F(5) => {
                    debug!("Panel: toggle Plugins");
                    app.toggle_panel(ActivePanel::Plugins);
                }
                KeyCode::Enter => {
                    debug!("User submitted input: '{}' ({} chars)",
                           truncate_str(&app.input, 80), app.input.len());
                    if let Err(e) = app.submit_input().await {
                        warn!("submit_input error: {}", e);
                        app.add_message(
                            app::MessageRole::System,
                            format!("Error: {}", e),
                        );
                    }
                }
                KeyCode::Backspace => {
                    app.input.pop();
                }
                KeyCode::Char(c) => {
                    app.input.push(c);
                }
                KeyCode::Up => {
                    app.scroll_up();
                }
                KeyCode::Down => {
                    app.scroll_down();
                }
                KeyCode::PageUp => {
                    app.scroll_page_up();
                }
                KeyCode::PageDown => {
                    app.scroll_page_down();
                }
                _ => {}
            }
        }
    }
}

fn truncate_str(s: &str, max: usize) -> &str {
    if s.len() <= max { s } else { &s[..max] }
}