// Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
//
// Application state for the AgentRT TUI.

use anyhow::Result;
use chrono::Local;
use std::collections::VecDeque;
use std::time::Instant;

use crate::client::GatewayClient;

/// Maximum number of chat messages to keep in memory.
const MAX_CHAT_MESSAGES: usize = 500;

/// Maximum number of log entries to keep.
const MAX_LOG_ENTRIES: usize = 200;

/// Active panel for the TUI.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ActivePanel {
    Chat,
    Help,
    Config,
    Logs,
    Memory,
    Plugins,
}

/// Represents a chat message in the conversation.
#[derive(Debug, Clone)]
pub struct ChatMessage {
    pub role: MessageRole,
    pub content: String,
    pub timestamp: String,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum MessageRole {
    User,
    Agent,
    System,
    ToolCall,
    ToolResult,
}

/// Represents a log entry.
#[derive(Debug, Clone)]
pub struct LogEntry {
    pub timestamp: String,
    pub level: String,
    pub message: String,
    pub daemon: Option<String>,
}

/// Application state.
pub struct App {
    /// Agent file being used
    pub agent_file: String,
    /// Chat messages
    pub messages: VecDeque<ChatMessage>,
    /// User input buffer
    pub input: String,
    /// Currently active panel
    pub active_panel: ActivePanel,
    /// Scroll position in chat
    pub scroll_offset: u16,
    /// Gateway client
    pub gateway: GatewayClient,
    /// Connected status
    pub connected: bool,
    /// Gateway version
    pub gateway_version: Option<String>,
    /// Current turn number
    pub turn: u64,
    /// Total tokens used
    pub tokens: u64,
    /// Total cost in USD
    pub cost: f64,
    /// Elapsed time since session start
    pub session_start: Instant,
    /// Whether MCP is enabled
    pub mcp_enabled: bool,
    /// Whether A2A is enabled
    pub a2a_enabled: bool,
    /// Log entries
    pub logs: VecDeque<LogEntry>,
    /// Help text cached
    pub help_text: Vec<String>,
    /// Config content
    pub config_content: String,
    /// Memory stats
    pub memory_stats: String,
    /// Plugin list
    pub plugin_list: String,
    /// Whether we are currently loading (waiting for response)
    pub loading: bool,
    /// Status message
    pub status_message: String,
}

impl App {
    pub fn new(agent_file: &str, gateway: GatewayClient) -> Self {
        Self {
            agent_file: agent_file.to_string(),
            messages: VecDeque::with_capacity(MAX_CHAT_MESSAGES),
            input: String::new(),
            active_panel: ActivePanel::Chat,
            scroll_offset: 0,
            gateway,
            connected: false,
            gateway_version: None,
            turn: 0,
            tokens: 0,
            cost: 0.0,
            session_start: Instant::now(),
            mcp_enabled: false,
            a2a_enabled: false,
            logs: VecDeque::with_capacity(MAX_LOG_ENTRIES),
            help_text: build_help_text(),
            config_content: String::new(),
            memory_stats: String::new(),
            plugin_list: String::new(),
            loading: false,
            status_message: "Press Enter to start".to_string(),
        }
    }

    /// Toggle a panel. If already active, go back to Chat.
    pub fn toggle_panel(&mut self, panel: ActivePanel) {
        if self.active_panel == panel {
            self.active_panel = ActivePanel::Chat;
        } else {
            self.active_panel = panel;
        }
    }

    /// Submit the current input as a message.
    pub async fn submit_input(&mut self) -> Result<()> {
        let input = self.input.trim().to_string();
        self.input.clear();

        if input.is_empty() {
            return Ok(());
        }

        // Add user message
        self.add_message(MessageRole::User, input.clone());

        // Check connection
        if !self.connected {
            self.check_connection().await?;
        }

        if self.connected && !input.eq_ignore_ascii_case("exit") {
            self.loading = true;
            self.turn += 1;

            // Send to gateway
            match self.gateway.send_message(&input, &self.agent_file).await {
                Ok(response) => {
                    self.add_message(MessageRole::Agent, response.response);
                    if let Some(t) = response.tokens_used {
                        self.tokens += t;
                    }
                    if let Some(c) = response.cost_usd {
                        self.cost += c;
                    }
                }
                Err(e) => {
                    self.add_message(MessageRole::System, format!("Error: {}", e));
                }
            }

            self.loading = false;
        } else if input.eq_ignore_ascii_case("exit") {
            self.add_message(MessageRole::System, "Type Ctrl+C to quit.".to_string());
        } else {
            self.add_message(
                MessageRole::System,
                "Not connected to gateway. Run 'agentrt' to start the server.".to_string(),
            );
        }

        Ok(())
    }

    /// Check gateway connection.
    pub async fn check_connection(&mut self) -> Result<()> {
        match self.gateway.health_check().await {
            Ok(health) => {
                self.connected = true;
                self.gateway_version = health.version.clone();
                self.status_message = format!("Connected to AgentRT v{}", health.version.as_deref().unwrap_or("unknown"));
            }
            Err(e) => {
                self.connected = false;
                self.status_message = format!("Gateway unreachable: {}", e);
            }
        }
        Ok(())
    }

    /// Add a chat message.
    pub fn add_message(&mut self, role: MessageRole, content: String) {
        let msg = ChatMessage {
            role,
            content,
            timestamp: Local::now().format("%H:%M:%S").to_string(),
        };

        if self.messages.len() >= MAX_CHAT_MESSAGES {
            self.messages.pop_front();
        }
        self.messages.push_back(msg);
    }

    /// Scroll up in chat.
    pub fn scroll_up(&mut self) {
        self.scroll_offset = self.scroll_offset.saturating_add(1);
    }

    /// Scroll down in chat.
    pub fn scroll_down(&mut self) {
        self.scroll_offset = self.scroll_offset.saturating_sub(1);
    }

    /// Scroll up one page.
    pub fn scroll_page_up(&mut self) {
        self.scroll_offset = self.scroll_offset.saturating_add(10);
    }

    /// Scroll down one page.
    pub fn scroll_page_down(&mut self) {
        self.scroll_offset = self.scroll_offset.saturating_sub(10);
    }

    /// Shutdown gracefully.
    pub async fn shutdown(&mut self) -> Result<()> {
        if self.connected {
            self.add_message(MessageRole::System, "Shutting down...".to_string());
        }
        Ok(())
    }

    /// Get elapsed time as formatted string.
    pub fn elapsed_time(&self) -> String {
        let elapsed = self.session_start.elapsed();
        let seconds = elapsed.as_secs();
        let hours = seconds / 3600;
        let minutes = (seconds % 3600) / 60;
        let secs = seconds % 60;

        if hours > 0 {
            format!("{}h{:02}m{:02}s", hours, minutes, secs)
        } else {
            format!("{:02}m{:02}s", minutes, secs)
        }
    }
}

fn build_help_text() -> Vec<String> {
    vec![
        "AgentRT TUI - Help".to_string(),
        String::new(),
        "Keyboard Shortcuts:".to_string(),
        "  F1          - Show this help panel".to_string(),
        "  F2          - Show configuration".to_string(),
        "  F3          - Show runtime logs".to_string(),
        "  F4          - Show memory statistics".to_string(),
        "  F5          - Show plugin list".to_string(),
        "  Enter       - Send message".to_string(),
        "  Ctrl+C      - Exit TUI".to_string(),
        "  Esc         - Return to chat".to_string(),
        "  Up/Down     - Scroll chat".to_string(),
        "  PgUp/PgDn   - Scroll chat (page)".to_string(),
        String::new(),
        "Commands:".to_string(),
        "  Type your message and press Enter to send.".to_string(),
        "  Type 'exit' to end the conversation.".to_string(),
        String::new(),
        "Status Bar:".to_string(),
        "  Shows turn count, token usage, cost, and elapsed time.".to_string(),
        "  MCP/A2A indicators show protocol availability.".to_string(),
    ]
}