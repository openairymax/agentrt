// Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
//
// Plugins panel rendering.

use ratatui::{
    layout::Rect,
    style::{Color, Style},
    text::{Line, Span},
    widgets::{Block, Borders, Paragraph},
    Frame,
};

use crate::app::App;

/// Render the plugins panel.
pub fn render(f: &mut Frame, area: Rect, app: &App) {
    let block = Block::default()
        .borders(Borders::ALL)
        .border_style(Style::default().fg(Color::Blue))
        .title(" Plugins (Esc to return) ");

    if app.plugin_list.is_empty() {
        let text = vec![
            Line::from(Span::styled(
                "No plugins loaded.",
                Style::default().fg(Color::DarkGray),
            )),
            Line::from(Span::styled(
                "Connect to a running gateway to view installed plugins.",
                Style::default().fg(Color::DarkGray),
            )),
        ];
        f.render_widget(Paragraph::new(text).block(block), area);
    } else {
        f.render_widget(
            Paragraph::new(app.plugin_list.as_str()).block(block),
            area,
        );
    }
}