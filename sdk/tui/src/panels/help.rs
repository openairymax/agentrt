// Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
//
// Help panel rendering.

use ratatui::{
    layout::Rect,
    style::{Color, Style},
    text::{Line, Span, Text},
    widgets::{Block, Borders, Paragraph, Wrap},
    Frame,
};

use crate::app::App;

/// Render the help panel.
pub fn render(f: &mut Frame, area: Rect, app: &App) {
    let block = Block::default()
        .borders(Borders::ALL)
        .border_style(Style::default().fg(Color::Yellow))
        .title(" Help (Esc to return) ");

    let lines: Vec<Line> = app
        .help_text
        .iter()
        .map(|s| Line::from(Span::raw(s.as_str())))
        .collect();

    f.render_widget(
        Paragraph::new(Text::from(lines))
            .block(block)
            .wrap(Wrap { trim: true }),
        area,
    );
}