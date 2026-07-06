// Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
//
// CLI command: agentrt llm
//
// LLM provider management and cost tracking through the gateway.

use anyhow::Result;
use colored::Colorize;

use crate::client::{CostResponse, GatewayClient, LlmProvider, LlmTestRequest};

/// List configured LLM providers.
pub async fn list(gateway_url: &str) -> Result<()> {
    let client = GatewayClient::new(gateway_url)?;

    let providers: Vec<LlmProvider> = client.get("/api/v1/llm/providers").await?;

    if providers.is_empty() {
        println!("{} No LLM providers configured.", "ℹ".yellow());
        println!("  Add providers to your agentrt.yaml under 'llm.providers'.");
        return Ok(());
    }

    println!("{} LLM Providers:", "🤖".blue().bold());
    println!();
    println!("  {:<20} {:<15} Models", "Provider", "Status");
    println!("  {:-<20} {:-<15} {:-<30}", "", "", "");

    for p in &providers {
        let status_icon = if p.status == "connected" { "✓".green() } else { "✗".red() };
        println!(
            "  {:<20} {:<15} {}",
            p.name,
            format!("{} {}", status_icon, p.status),
            p.models.join(", ")
        );
    }

    Ok(())
}

/// Test provider connectivity.
pub async fn test(gateway_url: &str, provider: &str) -> Result<()> {
    let client = GatewayClient::new(gateway_url)?;

    println!("{} Testing provider: {}...", "🔍".yellow(), provider.cyan());

    let request = LlmTestRequest {
        provider: provider.to_string(),
    };

    match client
        .post::<serde_json::Value>("/api/v1/llm/test", &request)
        .await
    {
        Ok(resp) => {
            println!(
                "{} Provider '{}' is working: {}",
                "✓".green(),
                provider,
                serde_json::to_string_pretty(&resp).unwrap_or_default()
            );
        }
        Err(e) => {
            eprintln!("{} Provider '{}' test failed: {}", "✗".red(), provider, e);
        }
    }

    Ok(())
}

/// Show LLM usage costs.
pub async fn cost(gateway_url: &str) -> Result<()> {
    let client = GatewayClient::new(gateway_url)?;

    let costs: CostResponse = client.get("/api/v1/llm/cost").await?;

    println!("{} LLM Usage Costs:", "💰".blue().bold());
    println!();
    println!("  Total tokens:   {}", costs.total_tokens);
    println!("  Total cost:      ${:.6}", costs.total_cost_usd);
    println!();

    if let Some(by_provider) = &costs.by_provider {
        if !by_provider.is_empty() {
            println!("  {:<20} {:<15} Cost (USD)", "Provider", "Tokens");
            println!("  {:-<20} {:-<15} {:-<15}", "", "", "");
            for pc in by_provider {
                println!(
                    "  {:<20} {:<15} ${:.6}",
                    pc.provider, pc.tokens, pc.cost_usd
                );
            }
        }
    }

    Ok(())
}