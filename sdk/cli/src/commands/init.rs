// Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
//
// CLI command: agentrt init
//
// Scaffolds a new AgentRT project with the standard directory layout.

use anyhow::{Context, Result};
use colored::Colorize;
use std::fs;
use std::path::Path;

use crate::templates;

/// Initialize a new AgentRT project.
pub fn run(name: &str) -> Result<()> {
    let project_dir = Path::new(name);

    if project_dir.exists() {
        anyhow::bail!(
            "Directory '{}' already exists. Choose a different project name.",
            name
        );
    }

    println!("{} Initializing AgentRT project: {}", "▶".green().bold(), name.cyan());

    // Create directory structure
    let dirs = [
        "",
        "agents",
        "prompts",
        "tools",
        "hooks",
        "skills",
        "plugins",
        "tests",
    ];

    for dir in &dirs {
        let path = if dir.is_empty() {
            project_dir.to_path_buf()
        } else {
            project_dir.join(dir)
        };
        fs::create_dir_all(&path)
            .with_context(|| format!("Failed to create directory: {}", path.display()))?;
        println!("  {} Created {}", "✓".green(), path.display());
    }

    // Write agentrt.yaml
    let config_path = project_dir.join("agentrt.yaml");
    fs::write(&config_path, templates::DEFAULT_AGENTRT_YAML)
        .with_context(|| format!("Failed to write {}", config_path.display()))?;
    println!("  {} Created {}", "✓".green(), config_path.display());

    // Write main agent definition
    let agent_path = project_dir.join("agents").join("main.agent.yaml");
    fs::write(&agent_path, templates::DEFAULT_AGENT_YAML)
        .with_context(|| format!("Failed to write {}", agent_path.display()))?;
    println!("  {} Created {}", "✓".green(), agent_path.display());

    // Write system prompt
    let prompt_path = project_dir.join("prompts").join("system.yaml");
    fs::write(&prompt_path, templates::DEFAULT_SYSTEM_PROMPT)
        .with_context(|| format!("Failed to write {}", prompt_path.display()))?;
    println!("  {} Created {}", "✓".green(), prompt_path.display());

    // Write example custom tool
    let tool_path = project_dir.join("tools").join("custom.py");
    fs::write(&tool_path, templates::EXAMPLE_TOOL)
        .with_context(|| format!("Failed to write {}", tool_path.display()))?;
    println!("  {} Created {}", "✓".green(), tool_path.display());

    // Write example hook
    let hook_path = project_dir.join("hooks").join("audit.py");
    fs::write(&hook_path, templates::EXAMPLE_HOOK)
        .with_context(|| format!("Failed to write {}", hook_path.display()))?;
    println!("  {} Created {}", "✓".green(), hook_path.display());

    // Write .gitignore
    let gitignore_path = project_dir.join(".gitignore");
    fs::write(&gitignore_path, templates::GITIGNORE)
        .with_context(|| format!("Failed to write {}", gitignore_path.display()))?;
    println!("  {} Created {}", "✓".green(), gitignore_path.display());

    println!();
    println!("{} Project '{}' initialized successfully!", "✓".green().bold(), name.cyan());
    println!();
    println!("  Next steps:");
    println!("    cd {}", name);
    println!("    agentrt config validate");
    println!("    agentrt run \"Hello, world!\"");

    Ok(())
}