// Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
//
// CLI command: agentrt db
//
// Database migration management for heapstore.
// Delegates to scripts/ci/pipeline/deploy/db-migrate.sh for actual migration logic.

use anyhow::{Context, Result};
use colored::Colorize;
use std::path::PathBuf;
use std::process::Command;

/// Find the project root directory by locating CMakeLists.txt.
fn find_project_root() -> Result<PathBuf> {
    let mut dir = std::env::current_dir()?;
    loop {
        if dir.join("CMakeLists.txt").exists() {
            return Ok(dir);
        }
        if !dir.pop() {
            anyhow::bail!("Cannot find AgentRT project root (no CMakeLists.txt found)");
        }
    }
}

/// Run the db-migrate.sh script with the given arguments.
fn run_migrate_script(args: &[&str]) -> Result<()> {
    let project_root = find_project_root()?;
    let script = project_root.join("scripts/ci/pipeline/deploy/db-migrate.sh");

    if !script.exists() {
        anyhow::bail!(
            "Migration script not found: {}\nExpected at: {}",
            script.file_name().unwrap_or_default().to_string_lossy(),
            script.display()
        );
    }

    let status = Command::new("bash")
        .arg(script.to_string_lossy().as_ref())
        .args(args)
        .status()
        .with_context(|| "Failed to execute db-migrate.sh".to_string())?;

    if !status.success() {
        anyhow::bail!("Migration command failed with exit code: {:?}", status.code());
    }
    Ok(())
}

/// Show current migration status.
pub fn status() -> Result<()> {
    println!("{} Checking database migration status...", "📊".blue());
    run_migrate_script(&["status"])
}

/// Apply pending migrations.
pub fn migrate(dry_run: bool, force: bool) -> Result<()> {
    if dry_run {
        println!("{} DRY-RUN: Previewing pending migrations...", "🔍".yellow());
        run_migrate_script(&["migrate", "--dry-run"])
    } else {
        println!("{} Applying pending migrations...", "🚀".blue());
        let mut args = vec!["migrate"];
        if force {
            args.push("--force");
        }
        run_migrate_script(&args)
    }
}

/// Rollback the most recent migration.
pub fn rollback(force: bool) -> Result<()> {
    if !force {
        println!(
            "{} WARNING: Rollback is a destructive operation.",
            "⚠️".yellow()
        );
        println!("   Use --force to confirm.");
        println!("   Example: agentrt db rollback --force");
        return Ok(());
    }

    println!("{} Rolling back most recent migration...", "⏪".yellow());
    run_migrate_script(&["rollback", "--force"])
}

/// Create a new migration file template.
pub fn new_migration(name: &str) -> Result<()> {
    println!("{} Creating new migration: {}", "📝".blue(), name.cyan());
    run_migrate_script(&["new"])
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_find_project_root() {
        let root = find_project_root();
        assert!(root.is_ok(), "Should find project root");
        let root = root.unwrap();
        assert!(root.join("CMakeLists.txt").exists(), "Should contain CMakeLists.txt");
    }
}