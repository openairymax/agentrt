// Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
//
// Chat panel rendering.

use ratatui::{
    layout::Rect,
    style::{Color, Modifier, Style},
    text::{Line, Span, Text},
    widgets::{Block, Borders, Paragraph, Wrap},
    Frame,
};

use crate::app::{App, MessageRole};

/// Render the chat conversation panel.
pub fn render(f: &mut Frame, area: Rect, app: &App) {
    let block = Block::default()
        .borders(Borders::ALL)
        .border_style(Style::default().fg(Color::Cyan))
        .title(" Chat ");

    let inner_area = block.inner(area);

    let mut lines: Vec<Line> = Vec::new();

    // Calculate visible range based on scroll
    let total_msgs = app.messages.len();
    let visible_count = (inner_area.height as usize).saturating_sub(2);
    let start_idx = if total_msgs > visible_count {
        (total_msgs - visible_count).saturating_sub(app.scroll_offset as usize)
    } else {
        0
    };

    let visible_msgs: Vec<_> = app
        .messages
        .iter()
        .skip(start_idx)
        .take(visible_count)
        .collect();

    for msg in visible_msgs {
        let (role_style, prefix) = match msg.role {
            MessageRole::User => (
                Style::default().fg(Color::Green).add_modifier(Modifier::BOLD),
                "You",
            ),
            MessageRole::Agent => (
                Style::default().fg(Color::Cyan).add_modifier(Modifier::BOLD),
                "Agent",
            ),
            MessageRole::System => (
                Style::default().fg(Color::Yellow).add_modifier(Modifier::BOLD),
                "System",
            ),
            MessageRole::ToolCall => (
                Style::default().fg(Color::Magenta).add_modifier(Modifier::BOLD),
                "Tool",
            ),
            MessageRole::ToolResult => (
                Style::default().fg(Color::Blue).add_modifier(Modifier::BOLD),
                "Result",
            ),
        };

        // Role and timestamp header
        lines.push(Line::from(vec![
            Span::styled(
                format!("[{}] {}  ", prefix, msg.timestamp),
                Style::default().fg(Color::DarkGray),
            ),
        ]));

        // Message content (word-wrapped)
        for content_line in msg.content.lines() {
            let trimmed = content_line.trim();
            if trimmed.is_empty() {
                lines.push(Line::raw(""));
            } else {
                lines.push(Line::from(vec![
                    Span::styled("  ", Style::default()),
                    Span::styled(trimmed, role_style),
                ]));
            }
        }

        // Separator
        lines.push(Line::raw(""));
    }

    if app.messages.is_empty() {
        lines.push(Line::from(vec![
            Span::styled(
                "  Welcome to AgentRT TUI!",
                Style::default().fg(Color::DarkGray).add_modifier(Modifier::ITALIC),
            ),
        ]));
        lines.push(Line::from(vec![
            Span::styled(
                "  Type your message and press Enter to begin.",
                Style::default().fg(Color::DarkGray).add_modifier(Modifier::ITALIC),
            ),
        ]));
        lines.push(Line::from(vec![
            Span::styled(
                "  Press F1 for help.",
                Style::default().fg(Color::DarkGray).add_modifier(Modifier::ITALIC),
            ),
        ]));
    }

    if app.loading {
        lines.push(Line::from(vec![
            Span::styled(
                "  ⏳ Waiting for response...",
                Style::default().fg(Color::Yellow).add_modifier(Modifier::BOLD),
            ),
        ]));
    }

    f.render_widget(
        Paragraph::new(Text::from(lines))
            .block(block)
            .wrap(Wrap { trim: true })
            .scroll((0, 0)),
        area,
    );
}