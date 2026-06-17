// Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
//
// CLI command: agentrt create
//
// Creates new agent, tool, plugin, prompt, or skill definitions.

use anyhow::{Context, Result};
use colored::Colorize;
use std::fs;
use std::path::Path;

use crate::templates;

fn ensure_dir(dir: &str) -> Result<()> {
    if !Path::new(dir).exists() {
        fs::create_dir_all(dir).with_context(|| format!("Failed to create directory: {}", dir))?;
    }
    Ok(())
}

/// Create a new agent definition.
pub fn run_agent(name: &str) -> Result<()> {
    ensure_dir("agents")?;
    let filename = format!("agents/{}.agent.yaml", name);
    let content = templates::DEFAULT_AGENT_YAML.replace("main-agent", name);
    write_file(&filename, &content)?;
    println!("{} Created agent: {}", "✓".green(), filename.cyan());
    println!("  Edit {} to configure your agent.", filename);
    Ok(())
}

/// Create a new tool.
pub fn run_tool(name: &str) -> Result<()> {
    ensure_dir("tools")?;
    let snake_name = name.replace('-', "_").to_lowercase();
    let filename = format!("tools/{}.py", snake_name);
    let content = templates::EXAMPLE_TOOL.replace("custom_tool", &snake_name);
    write_file(&filename, &content)?;
    println!("{} Created tool: {}", "✓".green(), filename.cyan());
    println!("  Implement the 'execute' function in {}.", filename);
    Ok(())
}

/// Create a new plugin.
pub fn run_plugin(name: &str) -> Result<()> {
    ensure_dir("plugins")?;
    let snake_name = name.replace('-', "_").to_lowercase();
    let filename = format!("plugins/{}.py", snake_name);
    let content = templates::EXAMPLE_PLUGIN.replace("my_plugin", &snake_name);
    write_file(&filename, &content)?;
    println!("{} Created plugin: {}", "✓".green(), filename.cyan());
    Ok(())
}

/// Create a new Prompt template.
pub fn run_prompt(name: &str) -> Result<()> {
    ensure_dir("prompts")?;
    let filename = format!("prompts/{}.yaml", name);
    let content = templates::EXAMPLE_PROMPT.replace("example-prompt", name);
    write_file(&filename, &content)?;
    println!("{} Created prompt template: {}", "✓".green(), filename.cyan());
    println!("  Edit the template in {}.", filename);
    Ok(())
}

/// Create a new Skill.
pub fn run_skill(name: &str) -> Result<()> {
    ensure_dir("skills")?;
    let filename = format!("skills/{}.md", name);
    write_file(&filename, templates::EXAMPLE_SKILL)?;
    println!("{} Created skill: {}", "✓".green(), filename.cyan());
    println!("  Edit the skill definition in {}.", filename);
    Ok(())
}

fn write_file(path: &str, content: &str) -> Result<()> {
    fs::write(path, content).with_context(|| format!("Failed to write file: {}", path))
}