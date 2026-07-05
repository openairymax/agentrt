// Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
//
// Config panel rendering.

use ratatui::{
    layout::Rect,
    style::{Color, Style},
    text::{Line, Span},
    widgets::{Block, Borders, Paragraph},
    Frame,
};

use crate::app::App;

/// Render the config panel.
pub fn render(f: &mut Frame, area: Rect, app: &App) {
    let block = Block::default()
        .borders(Borders::ALL)
        .border_style(Style::default().fg(Color::Cyan))
        .title(" Configuration (Esc to return) ");

    if app.config_content.is_empty() {
        let text = vec![
            Line::from(Span::styled("Config not loaded.", Style::default().fg(Color::DarkGray))),
            Line::from(Span::styled("Check your agentrt.yaml file.", Style::default().fg(Color::DarkGray))),
        ];
        f.render_widget(Paragraph::new(text).block(block), area);
    } else {
        f.render_widget(
            Paragraph::new(app.config_content.as_str()).block(block),
            area,
        );
    }
}