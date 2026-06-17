// Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
//
// CLI command: agentrt deploy
//
// Deployment, status, and log viewing through the gateway.

use anyhow::Result;
use colored::Colorize;

use crate::client::{DeployStatusResponse, GatewayClient, LogEntry};

/// Deploy to production.
pub async fn deploy(gateway_url: &str, target: &str) -> Result<()> {
    let client = GatewayClient::new(gateway_url)?;

    println!("{} Deploying to target: {}", "🚀".blue(), target.cyan());

    let request = serde_json::json!({
        "target": target,
    });

    match client
        .post::<serde_json::Value>("/api/v1/deploy", &request)
        .await
    {
        Ok(resp) => {
            println!(
                "{} Deployment initiated: {}",
                "✓".green(),
                serde_json::to_string_pretty(&resp).unwrap_or_default()
            );
            println!("  Run 'agentrt deploy status' to check deployment progress.");
        }
        Err(e) => {
            anyhow::bail!("Deployment failed: {}", e);
        }
    }

    Ok(())
}

/// Show AgentRT runtime status.
pub async fn status(gateway_url: &str) -> Result<()> {
    let client = GatewayClient::new(gateway_url)?;

    println!("{} AgentRT Status:", "📊".blue().bold());
    println!();

    let status: DeployStatusResponse = client.get("/api/v1/health").await?;

    println!("  Runtime:     {} {}", status.status, status.version);
    if let (Some(mem), Some(cpu)) = (status.memory_usage_mb, status.cpu_percent) {
        println!("  Memory:      {:.1} MB", mem);
        println!("  CPU:         {:.1}%", cpu);
    }
    println!();

    if !status.daemons.is_empty() {
        println!("  Daemons:");
        println!("  {:<20} {:<10} {:<8} {:<10} {}", "Name", "Status", "PID", "Port", "Uptime");
        println!("  {:-<20} {:-<10} {:-<8} {:-<10} {:-<10}", "", "", "", "", "");

        for d in &status.daemons {
            let status_icon = if d.status == "running" { "✓".green() } else { "✗".red() };
            let uptime = d.uptime_seconds.map(format_uptime).unwrap_or_default();
            let port = d.port.map(|p| p.to_string()).unwrap_or_else(|| "-".to_string());

            println!(
                "  {:<20} {:<10} {:<8} {:<10} {}",
                d.name,
                format!("{} {}", status_icon, d.status),
                d.pid,
                port,
                uptime
            );
        }
    }

    Ok(())
}

/// Show runtime logs.
pub async fn logs(gateway_url: &str, lines: u32) -> Result<()> {
    let client = GatewayClient::new(gateway_url)?;

    let logs: Vec<LogEntry> = client
        .get(&format!("/api/v1/logs?lines={}", lines))
        .await?;

    println!("{} Recent Logs ({} lines):", "📜".blue().bold(), lines);
    println!();

    for entry in &logs {
        let level_color = match entry.level.as_str() {
            "ERROR" => "ERROR".red().bold(),
            "WARN" => "WARN".yellow().bold(),
            "INFO" => "INFO".normal(),
            "DEBUG" => "DEBUG".dimmed(),
            _ => entry.level.as_str().normal(),
        };

        let daemon_tag = if let Some(d) = &entry.daemon {
            format!("[{}]", d.cyan())
        } else {
            String::new()
        };

        println!(
            "{} {} {} {}",
            entry.timestamp.dimmed(),
            level_color,
            daemon_tag,
            entry.message
        );
    }

    Ok(())
}

fn format_uptime(seconds: u64) -> String {
    let hours = seconds / 3600;
    let minutes = (seconds % 3600) / 60;
    let secs = seconds % 60;

    if hours > 0 {
        format!("{}h {}m", hours, minutes)
    } else if minutes > 0 {
        format!("{}m {}s", minutes, secs)
    } else {
        format!("{}s", secs)
    }
}