// Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
//
// Memory panel rendering.

use ratatui::{
    layout::Rect,
    style::{Color, Style},
    text::{Line, Span},
    widgets::{Block, Borders, Paragraph},
    Frame,
};

use crate::app::App;

/// Render the memory statistics panel.
pub fn render(f: &mut Frame, area: Rect, app: &App) {
    let block = Block::default()
        .borders(Borders::ALL)
        .border_style(Style::default().fg(Color::Green))
        .title(" Memory Stats (Esc to return) ");

    if app.memory_stats.is_empty() {
        let text = vec![
            Line::from(Span::styled(
                "Memory statistics not available.",
                Style::default().fg(Color::DarkGray),
            )),
            Line::from(Span::styled(
                "Connect to a running gateway to view memory stats.",
                Style::default().fg(Color::DarkGray),
            )),
        ];
        f.render_widget(Paragraph::new(text).block(block), area);
    } else {
        f.render_widget(
            Paragraph::new(app.memory_stats.as_str()).block(block),
            area,
        );
    }
}