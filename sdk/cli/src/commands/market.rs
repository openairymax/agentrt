// Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
//
// CLI command: agentrt market
//
// Search, install, and publish through the OpenLab Markets gateway.

use anyhow::Result;
use colored::Colorize;

use crate::client::{
    GatewayClient, MarketInstallRequest, MarketInstallResult, MarketSearchResult,
};

/// Search the agent marketplace.
pub async fn search(gateway_url: &str, keyword: &str) -> Result<()> {
    let client = GatewayClient::new(gateway_url)?;

    println!("{} Searching marketplace for: {}", "🔍".blue(), keyword.cyan());
    println!();

    let results: Vec<MarketSearchResult> = client
        .get(&format!("/api/v1/market/search?q={}", urlencoding::encode(keyword)))
        .await?;

    if results.is_empty() {
        println!("  No results found for '{}'.", keyword);
        println!("  Try a different keyword or browse the marketplace at https://openlab.spharx.com");
        return Ok(());
    }

    println!("  {:<25} {:<10} {:<10} {}", "Name", "Version", "Downloads", "Author");
    println!("  {:-<25} {:-<10} {:-<10} {:-<20}", "", "", "", "");

    for r in &results {
        println!(
            "  {:<25} {:<10} {:<10} {}",
            r.name.cyan(),
            r.version,
            r.downloads,
            r.author
        );
        println!("    {}", r.description);
    }

    println!();
    println!("  To install: agentrt install <package>");

    Ok(())
}

/// Install from the marketplace.
pub async fn install(gateway_url: &str, package: &str) -> Result<()> {
    let client = GatewayClient::new(gateway_url)?;

    println!("{} Installing: {}", "📦".blue(), package.cyan());

    let request = MarketInstallRequest {
        package: package.to_string(),
    };

    match client
        .post::<MarketInstallResult>("/api/v1/market/install", &request)
        .await
    {
        Ok(result) => {
            println!("{} {}", "✓".green(), result.message);
            if let Some(path) = &result.installed_path {
                println!("  Installed to: {}", path.cyan());
            }
        }
        Err(e) => {
            anyhow::bail!("Failed to install '{}': {}", package, e);
        }
    }

    Ok(())
}

/// Publish to OpenLab Markets.
pub async fn publish(gateway_url: &str) -> Result<()> {
    let client = GatewayClient::new(gateway_url)?;

    println!("{} Publishing to OpenLab Markets...", "📤".blue());

    // Check for agentos.yaml
    if !std::path::Path::new("agentos.yaml").exists() {
        anyhow::bail!("No agentos.yaml found. Run 'agentrt init' first.");
    }

    match client
        .post::<serde_json::Value>("/api/v1/market/publish", &serde_json::json!({}))
        .await
    {
        Ok(resp) => {
            println!(
                "{} Published successfully: {}",
                "✓".green(),
                serde_json::to_string_pretty(&resp).unwrap_or_default()
            );
        }
        Err(e) => {
            anyhow::bail!("Failed to publish: {}", e);
        }
    }

    Ok(())
}