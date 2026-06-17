// Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
//
// UI rendering for the AgentRT TUI.
//
// Layout structure:
// ┌─ Title Bar ────────────────────────────────────┐
// │  AgentRT v0.1.1 | [agent] | [status] | stats  │
// ├─────────────────────────────────────────────────┤
// │  ┌─ Main Content (varies by panel) ──────────┐ │
// │  │  Chat / Help / Config / Logs / etc.       │ │
// │  └───────────────────────────────────────────┘ │
// ├─ Status Bar ───────────────────────────────────┤
// │  Turn: N | Tokens: N | Cost: $N | Time: N     │
// ├─ Input Bar ────────────────────────────────────┤
// │  > _                                           │
// └─ Shortcuts ────────────────────────────────────┘
//  [F1:Help] [F2:Config] [F3:Logs] [F4:Memory] [F5:Plugins]

use ratatui::{
    layout::{Alignment, Constraint, Direction, Layout, Rect},
    style::{Color, Modifier, Style},
    text::{Line, Span},
    widgets::{Block, Borders, Paragraph},
    Frame,
};

use crate::app::{ActivePanel, App};
use crate::panels;

/// Main render function. Called on each frame.
pub fn render(f: &mut Frame, app: &mut App) {
    let area = f.area();

    // ─── Layout ──────────────────────────────────────
    let main_layout = Layout::default()
        .direction(Direction::Vertical)
        .constraints([
            Constraint::Length(1),  // Title bar
            Constraint::Min(3),     // Main content
            Constraint::Length(1),  // Status bar
            Constraint::Length(3),  // Input area
            Constraint::Length(1),  // Shortcuts
        ])
        .split(area);

    // ─── Title Bar ───────────────────────────────────
    render_title_bar(f, main_layout[0], app);

    // ─── Main Content ────────────────────────────────
    match app.active_panel {
        ActivePanel::Chat => panels::chat::render(f, main_layout[1], app),
        ActivePanel::Help => panels::help::render(f, main_layout[1], app),
        ActivePanel::Config => panels::config::render(f, main_layout[1], app),
        ActivePanel::Logs => panels::logs::render(f, main_layout[1], app),
        ActivePanel::Memory => panels::memory::render(f, main_layout[1], app),
        ActivePanel::Plugins => panels::plugins::render(f, main_layout[1], app),
    }

    // ─── Status Bar ──────────────────────────────────
    render_status_bar(f, main_layout[2], app);

    // ─── Input Area ──────────────────────────────────
    render_input_area(f, main_layout[3], app);

    // ─── Shortcuts ───────────────────────────────────
    render_shortcuts(f, main_layout[4], app);
}

fn render_title_bar(f: &mut Frame, area: Rect, app: &App) {
    let version = app.gateway_version.as_deref().unwrap_or("unknown");
    let status = if app.connected {
        Span::styled("ONLINE", Style::default().fg(Color::Green).add_modifier(Modifier::BOLD))
    } else if app.loading {
        Span::styled("WAITING", Style::default().fg(Color::Yellow).add_modifier(Modifier::BOLD))
    } else {
        Span::styled("OFFLINE", Style::default().fg(Color::Red).add_modifier(Modifier::BOLD))
    };

    let title = Line::from(vec![
        Span::styled(" AgentRT TUI ", Style::default().fg(Color::Cyan).add_modifier(Modifier::BOLD)),
        Span::styled(format!(" v{} ", version), Style::default().fg(Color::DarkGray)),
        Span::raw("| "),
        Span::styled(&app.agent_file, Style::default().fg(Color::White)),
        Span::raw(" | "),
        status,
    ]);

    f.render_widget(Paragraph::new(title), area);
}

fn render_status_bar(f: &mut Frame, area: Rect, app: &App) {
    let mcp_status = if app.mcp_enabled {
        Span::styled("MCP: ✓", Style::default().fg(Color::Green))
    } else {
        Span::styled("MCP: ✗", Style::default().fg(Color::DarkGray))
    };

    let a2a_status = if app.a2a_enabled {
        Span::styled("A2A: ✓", Style::default().fg(Color::Green))
    } else {
        Span::styled("A2A: ✗", Style::default().fg(Color::DarkGray))
    };

    let status_line = Line::from(vec![
        Span::raw(" "),
        Span::styled(
            format!("Turn: {} ", app.turn),
            Style::default().fg(Color::Cyan),
        ),
        Span::raw("| "),
        Span::styled(
            format!("Tokens: {} ", app.tokens),
            Style::default().fg(Color::Yellow),
        ),
        Span::raw("| "),
        Span::styled(
            format!("Cost: ${:.4} ", app.cost),
            Style::default().fg(Color::Magenta),
        ),
        Span::raw("| "),
        Span::styled(
            format!("Time: {} ", app.elapsed_time()),
            Style::default().fg(Color::White),
        ),
        Span::raw("| "),
        mcp_status,
        Span::raw(" "),
        a2a_status,
        Span::raw(" | "),
        Span::styled(
            &app.status_message,
            Style::default().fg(Color::DarkGray),
        ),
    ]);

    let block = Block::default().style(Style::default().bg(Color::DarkGray));
    f.render_widget(Paragraph::new(status_line).block(block), area);
}

fn render_input_area(f: &mut Frame, area: Rect, app: &App) {
    let prompt = if app.loading {
        "⏳ Waiting for response..."
    } else {
        "> "
    };

    let input_text = format!("{}{}", prompt, app.input);

    let cursor_pos = if app.loading {
        None
    } else {
        Some(prompt.len() + app.input.len())
    };

    // Show cursor only in chat mode and not loading
    let show_cursor = app.active_panel == ActivePanel::Chat && !app.loading;
    if show_cursor {
        let cursor_x = (area.x as usize + cursor_pos.unwrap_or(0))
            .min(area.width.saturating_sub(1) as usize) as u16;
        f.set_cursor_position((cursor_x, area.y));
    }

    let block = Block::default()
        .borders(Borders::TOP)
        .border_style(Style::default().fg(Color::Cyan))
        .title(" Input ");

    f.render_widget(
        Paragraph::new(input_text)
            .block(block)
            .style(Style::default().fg(Color::White)),
        area,
    );
}

fn render_shortcuts(f: &mut Frame, area: Rect, app: &App) {
    let shortcuts = vec![
        span_for_shortcut("F1", "Help", app.active_panel == ActivePanel::Help),
        Span::raw(" "),
        span_for_shortcut("F2", "Config", app.active_panel == ActivePanel::Config),
        Span::raw(" "),
        span_for_shortcut("F3", "Logs", app.active_panel == ActivePanel::Logs),
        Span::raw(" "),
        span_for_shortcut("F4", "Memory", app.active_panel == ActivePanel::Memory),
        Span::raw(" "),
        span_for_shortcut("F5", "Plugins", app.active_panel == ActivePanel::Plugins),
        Span::raw(" "),
        Span::styled("[Ctrl+C:Exit]", Style::default().fg(Color::Red)),
    ];

    f.render_widget(
        Paragraph::new(Line::from(shortcuts))
            .alignment(Alignment::Center)
            .style(Style::default().bg(Color::DarkGray)),
        area,
    );
}

fn span_for_shortcut<'a>(key: &'a str, label: &'a str, active: bool) -> Span<'a> {
    let style = if active {
        Style::default().fg(Color::Yellow).add_modifier(Modifier::BOLD)
    } else {
        Style::default().fg(Color::Gray)
    };

    Span::styled(
        format!("[{key}:{label}]"),
        style,
    )
}