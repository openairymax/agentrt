// Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
//
// CLI command: agentrt run
//
// Sends agent execution requests to the gateway.

use anyhow::Result;
use colored::Colorize;

use crate::client::{GatewayClient, RunRequest, RunResponse};

/// Execute an agent run through the gateway.
pub async fn execute(
    gateway_url: &str,
    prompt: Option<String>,
    agent_file: &str,
    model: Option<String>,
) -> Result<()> {
    let client = GatewayClient::new(gateway_url)?;

    // Check gateway health
    match client.health_check().await {
        Ok(health) => {
            println!(
                "{} Gateway: {} (v{})",
                "✓".green(),
                health.status,
                health.version.as_deref().unwrap_or("unknown")
            );
        }
        Err(_) => {
            anyhow::bail!(
                "Cannot connect to AgentRT gateway at {}. Is it running?",
                gateway_url
            );
        }
    }

    let interactive = prompt.is_none();
    let request = RunRequest {
        prompt: prompt.clone(),
        agent_file: agent_file.to_string(),
        model,
        interactive,
    };

    if interactive {
        println!("{} Starting interactive session...", "▶".blue().bold());
        println!("  Agent: {}", agent_file);
        println!("  Type your prompts or Ctrl+C to exit.");
        println!();
        // Interactive mode: read from stdin in a loop
        run_interactive(&client, agent_file).await?;
    } else {
        let prompt_text = prompt.as_deref().unwrap_or("");
        println!("{} Sending prompt: {}", "▶".blue().bold(), prompt_text);
        println!();

        let response: RunResponse = client
            .post("/api/v1/agent/run", &request)
            .await?;

        println!("{}", response.response);
        println!();
        if let (Some(tokens), Some(cost)) = (response.tokens_used, response.cost_usd) {
            println!(
                "{} Tokens: {} | Cost: ${:.6} | Session: {}",
                "ℹ".dimmed(),
                tokens,
                cost,
                response.session_id
            );
        }
    }

    Ok(())
}

/// Run in interactive mode, reading user input in a loop.
async fn run_interactive(client: &GatewayClient, agent_file: &str) -> Result<()> {
    let mut session_id: Option<String> = None;

    loop {
        // Print prompt
        print!("{} ", ">".cyan().bold());
        let _ = std::io::Write::flush(&mut std::io::stdout());

        let mut input = String::new();
        match std::io::stdin().read_line(&mut input) {
            Ok(0) => break, // EOF
            Ok(_) => {}
            Err(e) => {
                eprintln!("{} Error reading input: {}", "✗".red(), e);
                break;
            }
        }

        let input = input.trim().to_string();
        if input.is_empty() {
            continue;
        }
        if input.eq_ignore_ascii_case("exit") || input.eq_ignore_ascii_case("quit") {
            println!("{} Goodbye.", "✋".yellow());
            break;
        }

        let request = RunRequest {
            prompt: Some(input),
            agent_file: agent_file.to_string(),
            model: None,
            interactive: true,
        };

        match client.post::<RunResponse>("/api/v1/agent/run", &request).await {
            Ok(response) => {
                println!();
                println!("{}", response.response);
                println!();
                session_id = Some(response.session_id.clone());
            }
            Err(e) => {
                eprintln!("{} Gateway error: {}", "✗".red(), e);
            }
        }
    }

    Ok(())
}