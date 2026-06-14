// Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
//
// CLI command: agentrt config
//
// Local config management plus gateway reload.

use anyhow::{Context, Result};
use colored::Colorize;
use std::fs;
use std::path::Path;

use crate::client::GatewayClient;

const CONFIG_FILE: &str = "agentos.yaml";

/// Show the current configuration.
pub fn show() -> Result<()> {
    let path = find_config()?;
    let content = fs::read_to_string(&path)
        .with_context(|| format!("Failed to read {}", path.display()))?;

    println!("{} Configuration: {}", "📋".blue(), path.display().to_string().cyan());
    println!();
    println!("{}", content);
    Ok(())
}

/// Set a configuration value.
pub fn set(key: &str, value: &str) -> Result<()> {
    let path = find_config()?;
    println!(
        "{} Setting {} = {} in {}",
        "⚙".yellow(),
        key.cyan(),
        value.green(),
        path.display()
    );

    // Read existing config
    let content = fs::read_to_string(&path)
        .with_context(|| format!("Failed to read {}", path.display()))?;

    let mut config: serde_yaml::Value = serde_yaml::from_str(&content)
        .with_context(|| format!("Failed to parse {} as YAML", path.display()))?;

    // Parse the value as YAML to support complex types
    let parsed_value: serde_yaml::Value = serde_yaml::from_str(value)
        .unwrap_or(serde_yaml::Value::String(value.to_string()));

    // Navigate and set nested key
    set_nested(&mut config, key, parsed_value)?;

    // Write back
    let updated = serde_yaml::to_string(&config)
        .context("Failed to serialize config to YAML")?;

    fs::write(&path, &updated)
        .with_context(|| format!("Failed to write {}", path.display()))?;

    println!("{} Configuration updated.", "✓".green());
    println!("  Run 'agentrt config reload' to apply changes to running gateway.");
    Ok(())
}

/// Validate the agentos.yaml configuration.
pub fn validate() -> Result<()> {
    let path = find_config()?;
    println!("{} Validating: {}", "🔍".yellow(), path.display().to_string().cyan());

    let content = fs::read_to_string(&path)
        .with_context(|| format!("Failed to read {}", path.display()))?;

    // Parse YAML syntax
    let config: serde_yaml::Value = serde_yaml::from_str(&content)
        .with_context(|| format!("Invalid YAML in {}: syntax error", path.display()))?;

    // Validate required top-level keys
    let required_keys = ["kernel", "llm", "security"];
    let mut errors = Vec::new();

    if let Some(mapping) = config.as_mapping() {
        for key in &required_keys {
            if !mapping.contains_key(&serde_yaml::Value::String(key.to_string())) {
                errors.push(format!("Missing required section: '{}'", key));
            }
        }
    } else {
        errors.push("Config root must be a YAML mapping".to_string());
    }

    if errors.is_empty() {
        println!("{} Configuration is valid.", "✓".green());
    } else {
        println!("{} Configuration has {} issue(s):", "✗".red(), errors.len());
        for error in &errors {
            println!("  - {}", error.red());
        }
        anyhow::bail!("Configuration validation failed with {} error(s)", errors.len());
    }

    Ok(())
}

/// Reload configuration on a running gateway.
pub async fn reload(gateway_url: &str) -> Result<()> {
    let client = GatewayClient::new(gateway_url)?;
    println!("{} Reloading configuration...", "⟳".blue());

    match client.post::<serde_json::Value>("/api/v1/config/reload", &serde_json::json!({})).await {
        Ok(resp) => {
            println!(
                "{} Configuration reloaded: {}",
                "✓".green(),
                serde_json::to_string_pretty(&resp).unwrap_or_default()
            );
        }
        Err(e) => {
            anyhow::bail!("Failed to reload configuration: {}", e);
        }
    }

    Ok(())
}

fn find_config() -> Result<std::path::PathBuf> {
    // Check current directory first, then parent directories
    let mut current = std::env::current_dir().context("Failed to get current directory")?;
    loop {
        let candidate = current.join(CONFIG_FILE);
        if candidate.exists() {
            return Ok(candidate);
        }
        if !current.pop() {
            break;
        }
    }
    anyhow::bail!(
        "No {} found in current directory or any parent directory.",
        CONFIG_FILE
    )
}

fn set_nested(
    root: &mut serde_yaml::Value,
    key: &str,
    value: serde_yaml::Value,
) -> Result<()> {
    let parts: Vec<&str> = key.split('.').collect();
    let mut current = root;

    for (i, part) in parts.iter().enumerate() {
        let is_last = i == parts.len() - 1;

        if is_last {
            // Set the value
            if let Some(mapping) = current.as_mapping_mut() {
                mapping.insert(
                    serde_yaml::Value::String(part.to_string()),
                    value.clone(),
                );
            } else {
                anyhow::bail!("Cannot set '{}': parent is not a mapping", key);
            }
        } else {
            // Navigate into nested mapping
            let key_val = serde_yaml::Value::String(part.to_string());
            if let Some(mapping) = current.as_mapping_mut() {
                let entry = mapping
                    .entry(key_val.clone())
                    .or_insert_with(|| serde_yaml::Value::Mapping(serde_yaml::Mapping::new()));
                current = entry;
            } else {
                anyhow::bail!("Cannot navigate to '{}': parent is not a mapping", part);
            }
        }
    }

    Ok(())
}