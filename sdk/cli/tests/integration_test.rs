// Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
//
// P2.9: CLI Integration Tests — init/run/create/config/llm commands
//
// Tests cover:
//   1. init command — project scaffolding
//   2. run command — argument parsing and gateway interaction
//   3. create command — component creation
//   4. config command — configuration management
//   5. llm command — provider listing and testing
//   6. Error handling — missing args, invalid inputs
//   7. Gateway URL — default and custom values

use std::fs;
use std::path::Path;
use std::process::Command;

// ============================================================================
// Test Helpers
// ============================================================================

/// Get the path to the built CLI binary.
/// Uses CARGO_BIN_EXE_agentrt env var set by Cargo for integration tests.
fn cli_binary() -> String {
    std::env::var("CARGO_BIN_EXE_agentrt")
        .unwrap_or_else(|_| {
            if cfg!(debug_assertions) {
                "target/debug/agentrt".to_string()
            } else {
                "target/release/agentrt".to_string()
            }
        })
}

/// Run CLI with given args and return output.
fn run_cli(args: &[&str]) -> std::process::Output {
    let binary = cli_binary();
    Command::new(&binary)
        .args(args)
        .output()
        .expect("Failed to execute CLI binary")
}

/// Run CLI with given args in a specific working directory.
fn run_cli_in(args: &[&str], current_dir: &Path) -> std::process::Output {
    let binary = cli_binary();
    Command::new(&binary)
        .args(args)
        .current_dir(current_dir)
        .output()
        .expect("Failed to execute CLI binary")
}

/// Run CLI and assert success.
fn run_cli_ok(args: &[&str]) -> std::process::Output {
    let output = run_cli(args);
    if !output.status.success() {
        eprintln!("CLI failed: {}", String::from_utf8_lossy(&output.stderr));
    }
    assert!(output.status.success(), "CLI command {:?} should succeed", args);
    output
}

/// Run CLI in a specific working directory and assert success.
fn run_cli_ok_in(args: &[&str], current_dir: &Path) -> std::process::Output {
    let output = run_cli_in(args, current_dir);
    if !output.status.success() {
        eprintln!("CLI failed: {}", String::from_utf8_lossy(&output.stderr));
    }
    assert!(output.status.success(), "CLI command {:?} should succeed", args);
    output
}

/// Create a temporary directory and clean it up after test.
/// Does NOT change the process-wide CWD to avoid race conditions
/// when tests run in parallel.
fn with_temp_dir<F>(test_fn: F)
where
    F: FnOnce(&Path),
{
    let tmp = tempfile::tempdir().expect("Failed to create temp dir");
    test_fn(tmp.path());
}

// ============================================================================
// Test 1: init command — project scaffolding
// ============================================================================

#[test]
fn test_init_creates_directory_structure() {
    with_temp_dir(|tmp| {
        let output = run_cli_ok_in(&["init", "test-project"], tmp);
        let stdout = String::from_utf8_lossy(&output.stdout);

        // Verify output
        assert!(stdout.contains("Initializing"), "Should show init message");
        assert!(stdout.contains("test-project"), "Should show project name");

        // Verify directory structure
        let project_dir = tmp.join("test-project");
        assert!(project_dir.exists(), "Project directory should exist");
        assert!(project_dir.join("agents").exists(), "agents/ should exist");
        assert!(project_dir.join("agentos.yaml").exists(), "agentos.yaml should exist");
        assert!(project_dir.join("prompts").exists(), "prompts/ should exist");
        assert!(project_dir.join("tools").exists(), "tools/ should exist");
        assert!(project_dir.join("hooks").exists(), "hooks/ should exist");
        assert!(project_dir.join("skills").exists(), "skills/ should exist");
        assert!(project_dir.join("plugins").exists(), "plugins/ should exist");
        assert!(project_dir.join("tests").exists(), "tests/ should exist");
    });
}

#[test]
fn test_init_default_name() {
    with_temp_dir(|tmp| {
        let output = run_cli_ok_in(&["init"], tmp);
        let stdout = String::from_utf8_lossy(&output.stdout);

        assert!(stdout.contains("my-agent-project"), "Should use default name");
        assert!(tmp.join("my-agent-project").exists(), "Default project dir should exist");
    });
}

#[test]
fn test_init_rejects_existing_directory() {
    with_temp_dir(|tmp| {
        // Create dir first
        fs::create_dir_all(tmp.join("existing-project")).unwrap();

        let output = run_cli_in(&["init", "existing-project"], tmp);
        assert!(!output.status.success(), "Should fail for existing directory");

        let stderr = String::from_utf8_lossy(&output.stderr);
        assert!(stderr.contains("already exists"), "Should mention existing dir");
    });
}

#[test]
fn test_init_creates_config_file() {
    with_temp_dir(|tmp| {
        run_cli_ok_in(&["init", "cfg-test"], tmp);

        let config_path = tmp.join("cfg-test/agentos.yaml");
        assert!(config_path.exists(), "Config file should exist");

        let content = fs::read_to_string(config_path).unwrap();
        assert!(content.contains("version:"), "Config should have version");
        assert!(content.contains("kernel:"), "Config should have kernel section");
    });
}

// ============================================================================
// Test 2: create command — component creation
// ============================================================================

#[test]
fn test_create_agent() {
    with_temp_dir(|tmp| {
        run_cli_ok_in(&["init", "create-test"], tmp);
        let project_dir = tmp.join("create-test");

        let output = run_cli_ok_in(&["create", "agent", "my-agent"], &project_dir);
        let stdout = String::from_utf8_lossy(&output.stdout);

        assert!(stdout.contains("my-agent"), "Should show agent name");
        assert!(project_dir.join("agents/my-agent.agent.yaml").exists(), "Agent file should exist");

        let agent_content = fs::read_to_string(project_dir.join("agents/my-agent.agent.yaml")).unwrap();
        assert!(agent_content.contains("name:"), "Agent file should be valid YAML");
        assert!(agent_content.contains("my-agent"), "Agent file should contain name");
    });
}

#[test]
fn test_create_tool() {
    with_temp_dir(|tmp| {
        run_cli_ok_in(&["init", "create-test"], tmp);
        let project_dir = tmp.join("create-test");

        let output = run_cli_ok_in(&["create", "tool", "my-tool"], &project_dir);
        let stdout = String::from_utf8_lossy(&output.stdout);

        assert!(stdout.contains("my_tool"), "Should show tool name");
        assert!(project_dir.join("tools/my_tool.py").exists(), "Tool file should exist");
    });
}

#[test]
fn test_create_plugin() {
    with_temp_dir(|tmp| {
        run_cli_ok_in(&["init", "create-test"], tmp);
        let project_dir = tmp.join("create-test");

        let output = run_cli_ok_in(&["create", "plugin", "my-plugin"], &project_dir);
        let stdout = String::from_utf8_lossy(&output.stdout);

        assert!(stdout.contains("my_plugin"), "Should show plugin name");
        assert!(project_dir.join("plugins/my_plugin.py").exists(), "Plugin file should exist");
    });
}

#[test]
fn test_create_prompt() {
    with_temp_dir(|tmp| {
        run_cli_ok_in(&["init", "create-test"], tmp);
        let project_dir = tmp.join("create-test");

        let output = run_cli_ok_in(&["create", "prompt", "my-prompt"], &project_dir);
        let stdout = String::from_utf8_lossy(&output.stdout);

        assert!(stdout.contains("my-prompt"), "Should show prompt name");
        assert!(project_dir.join("prompts/my-prompt.yaml").exists(), "Prompt file should exist");
    });
}

#[test]
fn test_create_skill() {
    with_temp_dir(|tmp| {
        run_cli_ok_in(&["init", "create-test"], tmp);
        let project_dir = tmp.join("create-test");

        let output = run_cli_ok_in(&["create", "skill", "my-skill"], &project_dir);
        let stdout = String::from_utf8_lossy(&output.stdout);

        assert!(stdout.contains("my-skill"), "Should show skill name");
        assert!(project_dir.join("skills/my-skill.md").exists(), "Skill file should exist");
    });
}

// ============================================================================
// Test 3: config command — configuration management
// ============================================================================

#[test]
fn test_config_show() {
    with_temp_dir(|tmp| {
        run_cli_ok_in(&["init", "config-test"], tmp);
        let project_dir = tmp.join("config-test");

        let output = run_cli_ok_in(&["config", "show"], &project_dir);
        let stdout = String::from_utf8_lossy(&output.stdout);

        assert!(stdout.contains("version"), "Config should show version");
        assert!(stdout.contains("kernel"), "Config should show kernel section");
    });
}

#[test]
fn test_config_set_and_validate() {
    with_temp_dir(|tmp| {
        run_cli_ok_in(&["init", "config-test"], tmp);
        let project_dir = tmp.join("config-test");

        let output = run_cli_ok_in(&["config", "set", "kernel.name", "test-agent"], &project_dir);
        let stdout = String::from_utf8_lossy(&output.stdout);

        assert!(stdout.contains("test-agent"), "Should show set value");

        // Verify the change persisted
        let show_output = run_cli_ok_in(&["config", "show"], &project_dir);
        let show_stdout = String::from_utf8_lossy(&show_output.stdout);
        assert!(show_stdout.contains("test-agent"), "Config should persist change");
    });
}

#[test]
fn test_config_validate() {
    with_temp_dir(|tmp| {
        run_cli_ok_in(&["init", "config-test"], tmp);
        let project_dir = tmp.join("config-test");

        let output = run_cli_ok_in(&["config", "validate"], &project_dir);
        let stdout = String::from_utf8_lossy(&output.stdout);

        assert!(
            stdout.contains("valid") || stdout.contains("✓"),
            "Config should validate successfully"
        );
    });
}

#[test]
fn test_config_set_invalid_key() {
    with_temp_dir(|tmp| {
        run_cli_ok_in(&["init", "config-test"], tmp);
        let project_dir = tmp.join("config-test");

        // CLI accepts any key-value pair (no strict key validation)
        let output = run_cli_in(&["config", "set", "nonexistent.key.deep", "value"], &project_dir);
        // Should not crash, may succeed or fail
        let stderr = String::from_utf8_lossy(&output.stderr);
        let stdout = String::from_utf8_lossy(&output.stdout);
        assert!(
            output.status.success() || stderr.contains("Error") || stdout.contains("✓"),
            "Should handle invalid key gracefully"
        );
    });
}

// ============================================================================
// Test 4: CLI argument parsing and defaults
// ============================================================================

#[test]
fn test_help_output() {
    let output = run_cli_ok(&["--help"]);
    let stdout = String::from_utf8_lossy(&output.stdout);

    assert!(stdout.contains("init"), "Help should show init command");
    assert!(stdout.contains("run"), "Help should show run command");
    assert!(stdout.contains("create"), "Help should show create command");
    assert!(stdout.contains("config"), "Help should show config command");
    assert!(stdout.contains("llm"), "Help should show llm command");
    assert!(stdout.contains("deploy"), "Help should show deploy command");
    assert!(stdout.contains("completion"), "Help should show completion command");
}

#[test]
fn test_version_output() {
    let output = run_cli_ok(&["--version"]);
    let stdout = String::from_utf8_lossy(&output.stdout);

    assert!(stdout.contains("agentrt"), "Version should show name");
}

#[test]
fn test_subcommand_help() {
    for cmd in &["init", "create", "run", "config", "llm", "deploy", "db"] {
        let output = run_cli_ok(&[cmd, "--help"]);
        let stdout = String::from_utf8_lossy(&output.stdout);

        assert!(
            stdout.contains(cmd),
            "{} help should mention command name", cmd
        );
    }
}

#[test]
fn test_gateway_url_flag() {
    let output = run_cli(&["--gateway-url", "http://custom:9090", "llm", "list"]);
    // Should fail because no gateway is running, but URL should be parsed
    let stderr = String::from_utf8_lossy(&output.stderr);
    assert!(!output.status.success(), "Should fail without gateway");
    // The custom URL should be attempted
    assert!(
        stderr.contains("custom:9090") || stderr.contains("connection"),
        "Should attempt custom gateway URL"
    );
}

#[test]
fn test_missing_required_args() {
    let output = run_cli(&["create"]);
    assert!(!output.status.success(), "create without subcommand should fail");

    let output = run_cli(&["config", "set"]);
    assert!(!output.status.success(), "config set without key/value should fail");
}

// ============================================================================
// Test 5: llm command — provider management
// ============================================================================

#[test]
fn test_llm_help() {
    let output = run_cli_ok(&["llm", "--help"]);
    let stdout = String::from_utf8_lossy(&output.stdout);

    assert!(stdout.contains("list"), "LLM help should show list");
    assert!(stdout.contains("test"), "LLM help should show test");
    assert!(stdout.contains("cost"), "LLM help should show cost");
}

#[test]
fn test_llm_list_no_gateway() {
    // Should fail gracefully when no gateway is available
    let output = run_cli(&["llm", "list"]);
    // Expect failure (no gateway), but not a crash
    assert!(!output.status.success(), "Should fail when gateway unavailable");
}

// ============================================================================
// Test 6: run command — argument parsing
// ============================================================================

#[test]
fn test_run_with_prompt() {
    // Test argument parsing without actually connecting to gateway
    let output = run_cli(&["run", "Hello, world!"]);
    // Should fail because no gateway, but argument parsing should work
    let stdout = String::from_utf8_lossy(&output.stdout);
    let stderr = String::from_utf8_lossy(&output.stderr);
    assert!(
        stdout.contains("Hello") || stderr.contains("connection") || stderr.contains("gateway"),
        "Should attempt to run with prompt"
    );
}

#[test]
fn test_run_with_model() {
    let output = run_cli(&["run", "--model", "gpt-4", "Test"]);
    let stderr = String::from_utf8_lossy(&output.stderr);
    assert!(
        stderr.contains("gateway") || stderr.contains("connect"),
        "Should attempt gateway connection with model flag"
    );
}

#[test]
fn test_run_with_agent_file() {
    let output = run_cli(&["run", "--agent-file", "custom.yaml", "Test"]);
    let stderr = String::from_utf8_lossy(&output.stderr);
    assert!(
        stderr.contains("gateway") || stderr.contains("connect"),
        "Should attempt gateway connection with agent-file flag"
    );
}

// ============================================================================
// Test 7: completion command
// ============================================================================

#[test]
fn test_completion_bash() {
    let output = run_cli_ok(&["completion", "bash"]);
    let stdout = String::from_utf8_lossy(&output.stdout);

    assert!(!stdout.is_empty(), "Bash completion should produce output");
    assert!(stdout.contains("complete"), "Bash completion should contain complete command");
}

#[test]
fn test_completion_zsh() {
    let output = run_cli_ok(&["completion", "zsh"]);
    let stdout = String::from_utf8_lossy(&output.stdout);

    assert!(!stdout.is_empty(), "Zsh completion should produce output");
}

#[test]
fn test_completion_fish() {
    let output = run_cli_ok(&["completion", "fish"]);
    let stdout = String::from_utf8_lossy(&output.stdout);

    assert!(!stdout.is_empty(), "Fish completion should produce output");
}

// ============================================================================
// Test 8: db command — argument parsing
// ============================================================================

#[test]
fn test_db_help() {
    let output = run_cli_ok(&["db", "--help"]);
    let stdout = String::from_utf8_lossy(&output.stdout);

    assert!(stdout.contains("status"), "DB help should show status");
    assert!(stdout.contains("migrate"), "DB help should show migrate");
}

// ============================================================================
// Test 9: Error handling — edge cases
// ============================================================================

#[test]
fn test_unknown_subcommand() {
    let output = run_cli(&["nonexistent-command"]);
    assert!(!output.status.success(), "Unknown command should fail");
}

#[test]
fn test_empty_args() {
    let output = run_cli(&[]);
    // When no args provided, clap prints help to stderr
    let stderr = String::from_utf8_lossy(&output.stderr);
    assert!(stderr.contains("Usage") || stderr.contains("init"), "Empty args should show usage on stderr");
}