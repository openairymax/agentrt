// Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
//
// Logs panel rendering.

use ratatui::{
    layout::Rect,
    style::{Color, Style, Stylize},
    text::{Line, Span, Text},
    widgets::{Block, Borders, Paragraph},
    Frame,
};

use crate::app::App;

/// Render the logs panel.
pub fn render(f: &mut Frame, area: Rect, app: &App) {
    let block = Block::default()
        .borders(Borders::ALL)
        .border_style(Style::default().fg(Color::Magenta))
        .title(" Logs (Esc to return) ");

    if app.logs.is_empty() {
        let text = vec![
            Line::from(Span::styled(
                "No log entries available.",
                Style::default().fg(Color::DarkGray),
            )),
            Line::from(Span::styled(
                "Logs appear when the gateway is connected.",
                Style::default().fg(Color::DarkGray),
            )),
        ];
        f.render_widget(Paragraph::new(text).block(block), area);
        return;
    }

    let mut lines: Vec<Line> = Vec::new();
    for entry in app.logs.iter().rev().take(area.height as usize) {
        let level_style = match entry.level.as_str() {
            "ERROR" => Style::default().fg(Color::Red).bold(),
            "WARN" => Style::default().fg(Color::Yellow).bold(),
            _ => Style::default().fg(Color::White),
        };

        lines.push(Line::from(vec![
            Span::styled(&entry.timestamp, Style::default().fg(Color::DarkGray)),
            Span::raw(" "),
            Span::styled(&entry.level, level_style),
            Span::raw(" "),
            Span::styled(&entry.message, Style::default().fg(Color::White)),
        ]));
    }

    f.render_widget(
        Paragraph::new(Text::from(lines)).block(block),
        area,
    );
}