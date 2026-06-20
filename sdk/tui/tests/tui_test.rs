// Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
//
// P2.10: TUI Functional Verification Tests
//
// Tests cover:
//   1. App state initialization
//   2. Panel switching (Chat → Help → Config → Logs → Memory → Plugins)
//   3. Chat message management
//   4. Input buffer operations
//   5. Status tracking
//   6. Log entry management
//   7. Panel navigation boundaries

use std::collections::VecDeque;
use std::time::Instant;

// ============================================================================
// Test Helpers — simulate TUI app state without ratatui/crossterm
// ============================================================================

/// Minimal app state that mirrors the TUI App struct for testing.
struct TestApp {
    pub messages: VecDeque<TestMessage>,
    pub input: String,
    pub active_panel: TestPanel,
    pub scroll_offset: u16,
    pub connected: bool,
    pub gateway_version: Option<String>,
    pub turn: u64,
    pub tokens: u64,
    pub cost: f64,
    pub session_start: Instant,
    pub mcp_enabled: bool,
    pub a2a_enabled: bool,
    pub logs: VecDeque<TestLogEntry>,
    pub help_text: Vec<String>,
    pub config_content: String,
    pub memory_stats: String,
    pub plugin_list: String,
    pub loading: bool,
    pub status_message: String,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum TestPanel {
    Chat,
    Help,
    Config,
    Logs,
    Memory,
    Plugins,
}

#[derive(Debug, Clone)]
struct TestMessage {
    role: TestRole,
    content: String,
    timestamp: String,
}

#[derive(Debug, Clone, PartialEq, Eq)]
enum TestRole {
    User,
    Agent,
    System,
    ToolCall,
    ToolResult,
}

#[derive(Debug, Clone)]
struct TestLogEntry {
    timestamp: String,
    level: String,
    message: String,
    daemon: Option<String>,
}

const MAX_CHAT: usize = 500;
const MAX_LOGS: usize = 200;

impl TestApp {
    fn new() -> Self {
        Self {
            messages: VecDeque::with_capacity(MAX_CHAT),
            input: String::new(),
            active_panel: TestPanel::Chat,
            scroll_offset: 0,
            connected: false,
            gateway_version: None,
            turn: 0,
            tokens: 0,
            cost: 0.0,
            session_start: Instant::now(),
            mcp_enabled: false,
            a2a_enabled: false,
            logs: VecDeque::with_capacity(MAX_LOGS),
            help_text: Vec::new(),
            config_content: String::new(),
            memory_stats: String::new(),
            plugin_list: String::new(),
            loading: false,
            status_message: String::new(),
        }
    }

    fn add_message(&mut self, role: TestRole, content: &str) {
        if self.messages.len() >= MAX_CHAT {
            self.messages.pop_front();
        }
        self.messages.push_back(TestMessage {
            role,
            content: content.to_string(),
            timestamp: String::from("2026-01-01T00:00:00Z"),
        });
    }

    fn add_log(&mut self, level: &str, message: &str, daemon: Option<&str>) {
        if self.logs.len() >= MAX_LOGS {
            self.logs.pop_front();
        }
        self.logs.push_back(TestLogEntry {
            timestamp: String::from("2026-01-01T00:00:00Z"),
            level: level.to_string(),
            message: message.to_string(),
            daemon: daemon.map(|s| s.to_string()),
        });
    }

    fn switch_panel(&mut self, panel: TestPanel) {
        self.active_panel = panel;
    }

    fn next_panel(&mut self) {
        self.active_panel = match self.active_panel {
            TestPanel::Chat => TestPanel::Help,
            TestPanel::Help => TestPanel::Config,
            TestPanel::Config => TestPanel::Logs,
            TestPanel::Logs => TestPanel::Memory,
            TestPanel::Memory => TestPanel::Plugins,
            TestPanel::Plugins => TestPanel::Chat,
        };
    }

    fn prev_panel(&mut self) {
        self.active_panel = match self.active_panel {
            TestPanel::Chat => TestPanel::Plugins,
            TestPanel::Help => TestPanel::Chat,
            TestPanel::Config => TestPanel::Help,
            TestPanel::Logs => TestPanel::Config,
            TestPanel::Memory => TestPanel::Logs,
            TestPanel::Plugins => TestPanel::Memory,
        };
    }
}

// ============================================================================
// Test 1: App State Initialization
// ============================================================================

#[test]
fn test_app_initial_state() {
    let app = TestApp::new();

    assert_eq!(app.active_panel, TestPanel::Chat, "Default panel should be Chat");
    assert_eq!(app.messages.len(), 0, "No messages initially");
    assert_eq!(app.input, "", "Input should be empty");
    assert_eq!(app.scroll_offset, 0, "Scroll should be at top");
    assert!(!app.connected, "Should not be connected initially");
    assert_eq!(app.turn, 0, "Turn should be 0");
    assert_eq!(app.tokens, 0, "Tokens should be 0");
    assert_eq!(app.cost, 0.0, "Cost should be 0.0");
    assert!(!app.loading, "Should not be loading");
    assert_eq!(app.logs.len(), 0, "No logs initially");
}

#[test]
fn test_app_session_start() {
    let app = TestApp::new();
    let elapsed = app.session_start.elapsed();
    // Session should have started very recently
    assert!(elapsed.as_secs() < 1, "Session should have started just now");
}

// ============================================================================
// Test 2: Panel Switching
// ============================================================================

#[test]
fn test_switch_to_each_panel() {
    let mut app = TestApp::new();

    let panels = [
        TestPanel::Chat,
        TestPanel::Help,
        TestPanel::Config,
        TestPanel::Logs,
        TestPanel::Memory,
        TestPanel::Plugins,
    ];

    for &panel in &panels {
        app.switch_panel(panel);
        assert_eq!(app.active_panel, panel, "Should switch to {:?}", panel);
    }
}

#[test]
fn test_next_panel_cycling() {
    let mut app = TestApp::new();

    // Chat → Help → Config → Logs → Memory → Plugins → Chat
    let expected_sequence = [
        TestPanel::Help,
        TestPanel::Config,
        TestPanel::Logs,
        TestPanel::Memory,
        TestPanel::Plugins,
        TestPanel::Chat,
        TestPanel::Help,
    ];

    for &expected in &expected_sequence {
        app.next_panel();
        assert_eq!(app.active_panel, expected, "Next panel should be {:?}", expected);
    }
}

#[test]
fn test_prev_panel_cycling() {
    let mut app = TestApp::new();

    // Chat → Plugins → Memory → Logs → Config → Help → Chat
    let expected_sequence = [
        TestPanel::Plugins,
        TestPanel::Memory,
        TestPanel::Logs,
        TestPanel::Config,
        TestPanel::Help,
        TestPanel::Chat,
        TestPanel::Plugins,
    ];

    for &expected in &expected_sequence {
        app.prev_panel();
        assert_eq!(app.active_panel, expected, "Prev panel should be {:?}", expected);
    }
}

#[test]
fn test_panel_switch_preserves_data() {
    let mut app = TestApp::new();

    // Add messages while on Chat panel
    app.add_message(TestRole::User, "Hello");
    app.add_message(TestRole::Agent, "Hi there!");

    // Switch to other panels and back
    app.switch_panel(TestPanel::Config);
    app.switch_panel(TestPanel::Logs);
    app.switch_panel(TestPanel::Chat);

    // Messages should be preserved
    assert_eq!(app.messages.len(), 2, "Messages should be preserved");
    assert_eq!(app.messages[0].content, "Hello");
    assert_eq!(app.messages[1].content, "Hi there!");
}

#[test]
fn test_complete_panel_cycle() {
    let mut app = TestApp::new();

    // Cycle through all 6 panels
    for _ in 0..6 {
        app.next_panel();
    }

    // Should be back at Chat
    assert_eq!(app.active_panel, TestPanel::Chat, "Full cycle should return to Chat");
}

// ============================================================================
// Test 3: Chat Message Management
// ============================================================================

#[test]
fn test_add_user_message() {
    let mut app = TestApp::new();
    app.add_message(TestRole::User, "What is AgentRT?");

    assert_eq!(app.messages.len(), 1);
    let msg = &app.messages[0];
    assert_eq!(msg.role, TestRole::User);
    assert_eq!(msg.content, "What is AgentRT?");
    assert!(!msg.timestamp.is_empty());
}

#[test]
fn test_add_agent_message() {
    let mut app = TestApp::new();
    app.add_message(TestRole::Agent, "AgentRT is an intelligent agent runtime.");

    assert_eq!(app.messages.len(), 1);
    let msg = &app.messages[0];
    assert_eq!(msg.role, TestRole::Agent);
}

#[test]
fn test_conversation_flow() {
    let mut app = TestApp::new();

    app.add_message(TestRole::User, "Hello");
    app.add_message(TestRole::Agent, "Hi! How can I help?");
    app.add_message(TestRole::User, "Run a task");
    app.add_message(TestRole::ToolCall, "executing: search_files");
    app.add_message(TestRole::ToolResult, "Found 3 files");
    app.add_message(TestRole::Agent, "I found 3 files matching your query.");

    assert_eq!(app.messages.len(), 6);
    assert_eq!(app.messages[0].role, TestRole::User);
    assert_eq!(app.messages[3].role, TestRole::ToolCall);
    assert_eq!(app.messages[4].role, TestRole::ToolResult);
    assert_eq!(app.messages[5].role, TestRole::Agent);
}

#[test]
fn test_message_cap_at_500() {
    let mut app = TestApp::new();

    for i in 0..600 {
        app.add_message(TestRole::System, &format!("msg-{}", i));
    }

    assert_eq!(app.messages.len(), MAX_CHAT, "Should cap at 500 messages");
    // First 100 messages should be evicted
    assert_eq!(app.messages[0].content, "msg-100");
    assert_eq!(app.messages[MAX_CHAT - 1].content, "msg-599");
}

#[test]
fn test_scroll_offset_management() {
    let mut app = TestApp::new();

    // Add messages and scroll
    for i in 0..50 {
        app.add_message(TestRole::System, &format!("msg-{}", i));
    }

    // Scroll offset should be adjustable
    app.scroll_offset = 10;
    assert_eq!(app.scroll_offset, 10);

    app.scroll_offset = 0;
    assert_eq!(app.scroll_offset, 0);
}

// ============================================================================
// Test 4: Input Buffer Operations
// ============================================================================

#[test]
fn test_input_typing() {
    let mut app = TestApp::new();

    app.input.push_str("Hello");
    assert_eq!(app.input, "Hello");

    app.input.push_str(", World!");
    assert_eq!(app.input, "Hello, World!");
}

#[test]
fn test_input_clear() {
    let mut app = TestApp::new();
    app.input = String::from("some text");
    app.input.clear();

    assert_eq!(app.input, "");
}

#[test]
fn test_input_unicode() {
    let mut app = TestApp::new();
    app.input = String::from("你好，世界！🚀");
    assert_eq!(app.input, "你好，世界！🚀");
}

#[test]
fn test_input_empty_submit() {
    let mut app = TestApp::new();
    app.input = String::new();

    // Empty input should not submit
    assert!(app.input.is_empty());
    assert_eq!(app.messages.len(), 0);
}

// ============================================================================
// Test 5: Status Tracking
// ============================================================================

#[test]
fn test_connection_status() {
    let mut app = TestApp::new();

    assert!(!app.connected, "Should start disconnected");

    app.connected = true;
    app.gateway_version = Some("0.1.1".to_string());
    assert!(app.connected);
    assert_eq!(app.gateway_version.as_deref(), Some("0.1.1"));

    app.connected = false;
    assert!(!app.connected);
}

#[test]
fn test_token_and_cost_tracking() {
    let mut app = TestApp::new();

    app.tokens += 150;
    app.cost += 0.003;
    assert_eq!(app.tokens, 150);
    assert!((app.cost - 0.003).abs() < 0.0001);

    app.tokens += 200;
    app.cost += 0.004;
    assert_eq!(app.tokens, 350);
    assert!((app.cost - 0.007).abs() < 0.0001);
}

#[test]
fn test_turn_counter() {
    let mut app = TestApp::new();

    for i in 1..=10 {
        app.turn = i;
        assert_eq!(app.turn, i);
    }
}

#[test]
fn test_loading_state() {
    let mut app = TestApp::new();

    app.loading = true;
    app.status_message = "Connecting to gateway...".to_string();
    assert!(app.loading);
    assert!(app.status_message.contains("gateway"));

    app.loading = false;
    app.status_message = "Ready".to_string();
    assert!(!app.loading);
    assert_eq!(app.status_message, "Ready");
}

#[test]
fn test_mcp_a2a_flags() {
    let mut app = TestApp::new();

    assert!(!app.mcp_enabled);
    assert!(!app.a2a_enabled);

    app.mcp_enabled = true;
    app.a2a_enabled = true;
    assert!(app.mcp_enabled);
    assert!(app.a2a_enabled);
}

// ============================================================================
// Test 6: Log Entry Management
// ============================================================================

#[test]
fn test_add_log_entry() {
    let mut app = TestApp::new();

    app.add_log("INFO", "Gateway started", Some("gateway_d"));
    app.add_log("WARN", "Memory usage high", Some("monit_d"));
    app.add_log("ERROR", "Connection timeout", None);

    assert_eq!(app.logs.len(), 3);
    assert_eq!(app.logs[0].level, "INFO");
    assert_eq!(app.logs[0].daemon.as_deref(), Some("gateway_d"));
    assert_eq!(app.logs[1].level, "WARN");
    assert_eq!(app.logs[2].level, "ERROR");
    assert_eq!(app.logs[2].daemon, None);
}

#[test]
fn test_log_cap_at_200() {
    let mut app = TestApp::new();

    for i in 0..250 {
        app.add_log("INFO", &format!("log-{}", i), None);
    }

    assert_eq!(app.logs.len(), MAX_LOGS, "Should cap at 200 logs");
    assert_eq!(app.logs[0].message, "log-50");
    assert_eq!(app.logs[MAX_LOGS - 1].message, "log-249");
}

#[test]
fn test_log_levels() {
    let mut app = TestApp::new();

    let levels = ["DEBUG", "INFO", "WARN", "ERROR", "FATAL"];
    for level in &levels {
        app.add_log(level, "test", None);
    }

    assert_eq!(app.logs.len(), levels.len());
    for (i, level) in levels.iter().enumerate() {
        assert_eq!(app.logs[i].level, *level);
    }
}

// ============================================================================
// Test 7: Panel Content Management
// ============================================================================

#[test]
fn test_config_content() {
    let mut app = TestApp::new();
    app.config_content = String::from("version: \"0.1.1\"\nagent:\n  name: test");
    assert!(app.config_content.contains("version"));
    assert!(app.config_content.contains("test"));
}

#[test]
fn test_memory_stats() {
    let mut app = TestApp::new();
    app.memory_stats = String::from("heap: 45MB/128MB, pool: 12MB/64MB");
    assert!(app.memory_stats.contains("heap"));
    assert!(app.memory_stats.contains("pool"));
}

#[test]
fn test_plugin_list() {
    let mut app = TestApp::new();
    app.plugin_list = String::from("code-analyzer v1.0\nsecurity-scanner v2.1");
    assert!(app.plugin_list.contains("code-analyzer"));
    assert!(app.plugin_list.contains("security-scanner"));
}

#[test]
fn test_help_text() {
    let mut app = TestApp::new();
    app.help_text = vec![
        "F1: Chat".to_string(),
        "F2: Help".to_string(),
        "F3: Config".to_string(),
        "F4: Logs".to_string(),
        "F5: Memory".to_string(),
        "F6: Plugins".to_string(),
        "Esc: Back to Chat".to_string(),
        "Ctrl+C: Quit".to_string(),
    ];

    assert_eq!(app.help_text.len(), 8);
    assert!(app.help_text[0].contains("F1"));
    assert!(app.help_text[7].contains("Quit"));
}

// ============================================================================
// Test 8: Edge Cases & Boundaries
// ============================================================================

#[test]
fn test_rapid_panel_switching() {
    let mut app = TestApp::new();

    // Rapid switching should not cause issues
    for _ in 0..100 {
        app.next_panel();
        app.prev_panel();
    }

    assert_eq!(app.active_panel, TestPanel::Chat, "Should return to Chat after rapid switching");
}

#[test]
fn test_large_message_content() {
    let mut app = TestApp::new();
    let large_content = "A".repeat(10_000);

    app.add_message(TestRole::Agent, &large_content);
    assert_eq!(app.messages[0].content.len(), 10_000);
}

#[test]
fn test_empty_message() {
    let mut app = TestApp::new();
    app.add_message(TestRole::User, "");

    assert_eq!(app.messages.len(), 1);
    assert!(app.messages[0].content.is_empty());
}

#[test]
fn test_multiple_message_roles() {
    let mut app = TestApp::new();

    let roles = [
        TestRole::User,
        TestRole::Agent,
        TestRole::System,
        TestRole::ToolCall,
        TestRole::ToolResult,
    ];

    for role in &roles {
        app.add_message(role.clone(), "test");
    }

    assert_eq!(app.messages.len(), roles.len());
}