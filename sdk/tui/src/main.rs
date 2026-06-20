// Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
//
// AgentRT TUI - Main entry point
//
// Terminal-based user interface for AgentRT.
// Communicates with the gateway via HTTP API for all runtime operations.

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
use ratatui::prelude::*;
use std::io;

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
async fn main() -> Result<()> {
    env_logger::Builder::from_env(env_logger::Env::default().default_filter_or("info"))
        .format_timestamp_secs()
        .init();

    let cli = Cli::parse();

    // Setup terminal
    enable_raw_mode()?;
    let mut stdout = io::stdout();
    execute!(stdout, EnterAlternateScreen, EnableMouseCapture)?;
    let backend = CrosstermBackend::new(stdout);
    let mut terminal = Terminal::new(backend)?;

    // Connect to gateway
    let gateway = GatewayClient::new(&cli.gateway_url)?;
    let mut app = App::new(&cli.agent_file, gateway);

    // Main event loop
    let result = run_app(&mut terminal, &mut app).await;

    // Restore terminal
    disable_raw_mode()?;
    execute!(
        terminal.backend_mut(),
        LeaveAlternateScreen,
        DisableMouseCapture
    )?;
    terminal.show_cursor()?;

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
                        app.shutdown().await?;
                        return Ok(());
                    }
                KeyCode::Esc
                    if app.active_panel != ActivePanel::Chat => {
                        app.active_panel = ActivePanel::Chat;
                    }
                KeyCode::F(1) => app.toggle_panel(ActivePanel::Help),
                KeyCode::F(2) => app.toggle_panel(ActivePanel::Config),
                KeyCode::F(3) => app.toggle_panel(ActivePanel::Logs),
                KeyCode::F(4) => app.toggle_panel(ActivePanel::Memory),
                KeyCode::F(5) => app.toggle_panel(ActivePanel::Plugins),
                KeyCode::Enter => {
                    app.submit_input().await?;
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