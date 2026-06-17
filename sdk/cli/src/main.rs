// Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
//
// AgentRT CLI - Main entry point
//
// All commands that interact with the AgentRT runtime communicate
// through the gateway HTTP API. Local-only commands (init, create)
// do not require a running gateway.

mod client;
mod commands;
mod templates;

use clap::{Parser, Subcommand};
use clap_complete::Shell;
use std::io;

/// AgentRT - Intelligent Agent Runtime CLI
///
/// CLI tool for managing AgentRT projects, agents, and runtime operations.
/// Commands prefixed with (*) require a running AgentRT gateway.
#[derive(Parser)]
#[command(
    name = "agentrt",
    version = env!("CARGO_PKG_VERSION"),
    about = "AgentRT Intelligent Agent Runtime CLI",
    long_about = None,
    after_help = "For detailed documentation, visit https://github.com/spharx/agentrt",
)]
struct Cli {
    #[command(subcommand)]
    command: Commands,

    /// Gateway API base URL (default: http://localhost:8080)
    #[arg(long, global = true, env = "AGENTRT_GATEWAY_URL", default_value = "http://localhost:8080")]
    gateway_url: String,
}

#[derive(Subcommand)]
enum Commands {
    /// Initialize a new AgentRT project
    Init {
        /// Project name
        #[arg(default_value = "my-agent-project")]
        name: String,
    },

    /// Create a new agent, tool, plugin, prompt, or skill
    Create {
        #[command(subcommand)]
        subcommand: CreateCommand,
    },

    /// (*) Run an agent (interactive or single-shot)
    Run {
        /// Prompt text for single-shot execution
        prompt: Option<String>,

        /// Agent definition file path
        #[arg(short, long, default_value = "agents/main.agent.yaml")]
        agent_file: String,

        /// Model to use for this run
        #[arg(short, long)]
        model: Option<String>,
    },

    /// Manage AgentRT configuration
    Config {
        #[command(subcommand)]
        subcommand: ConfigCommand,
    },

    /// (*) Manage LLM providers and costs
    Llm {
        #[command(subcommand)]
        subcommand: LlmCommand,
    },

    /// Prompt template management
    Prompt {
        #[command(subcommand)]
        subcommand: PromptCommand,
    },

    /// (*) Search and install from agent marketplace
    Market {
        #[command(subcommand)]
        subcommand: MarketCommand,
    },

    /// (*) Deployment and status operations
    Deploy {
        #[command(subcommand)]
        subcommand: DeployCommand,
    },

    /// Generate shell completions
    Completion {
        /// Shell type
        #[arg(value_enum)]
        shell: Shell,
    },
}

#[derive(Subcommand)]
enum CreateCommand {
    /// Create a new agent definition
    Agent {
        /// Agent name
        name: String,
    },
    /// Create a new tool
    Tool {
        /// Tool name
        name: String,
    },
    /// Create a new plugin
    Plugin {
        /// Plugin name
        name: String,
    },
    /// Create a new Prompt template
    Prompt {
        /// Prompt name
        name: String,
    },
    /// Create a new Skill
    Skill {
        /// Skill name
        name: String,
    },
}

#[derive(Subcommand)]
enum ConfigCommand {
    /// Show current configuration
    Show,
    /// Set a configuration value
    Set {
        /// Configuration key (e.g., llm.providers.openai.api_key)
        key: String,
        /// Configuration value
        value: String,
    },
    /// Validate agentos.yaml configuration
    Validate,
    /// (*) Reload configuration on running gateway
    Reload,
}

#[derive(Subcommand)]
enum LlmCommand {
    /// (*) List configured LLM providers
    List,
    /// (*) Test provider connectivity
    Test {
        /// Provider name
        provider: String,
    },
    /// (*) Show LLM usage costs
    Cost,
}

#[derive(Subcommand)]
enum PromptCommand {
    /// List available Prompt templates
    List,
    /// Show a Prompt template
    Show {
        /// Template name
        name: String,
    },
    /// Tune a Prompt template
    Tune {
        /// Template name
        name: String,
        /// Path to evaluation dataset
        #[arg(long)]
        dataset: Option<String>,
    },
    /// A/B test two Prompt versions
    AbTest {
        /// Template name
        name: String,
        /// Baseline version
        #[arg(long)]
        baseline: String,
        /// Candidate version
        #[arg(long)]
        candidate: String,
    },
}

#[derive(Subcommand)]
enum MarketCommand {
    /// (*) Search the agent marketplace
    Search {
        /// Search keyword
        keyword: String,
    },
    /// (*) Install from marketplace
    Install {
        /// Package identifier (e.g., community/code-review-agent)
        package: String,
    },
    /// (*) Publish to OpenLab Markets
    Publish,
}

#[derive(Subcommand)]
enum DeployCommand {
    /// (*) Deploy to production
    Deploy {
        /// Deployment target
        #[arg(short, long, default_value = "docker")]
        target: String,
    },
    /// (*) Show AgentRT runtime status
    Status,
    /// (*) Show AgentRT runtime logs
    Logs {
        /// Number of log lines to show
        #[arg(short, long, default_value = "50")]
        lines: u32,
    },
}

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    env_logger::Builder::from_env(env_logger::Env::default().default_filter_or("info"))
        .format_timestamp_secs()
        .init();

    let cli = Cli::parse();

    match cli.command {
        Commands::Init { name } => {
            commands::init::run(&name)?;
        }
        Commands::Create { subcommand } => match subcommand {
            CreateCommand::Agent { name } => commands::create::run_agent(&name)?,
            CreateCommand::Tool { name } => commands::create::run_tool(&name)?,
            CreateCommand::Plugin { name } => commands::create::run_plugin(&name)?,
            CreateCommand::Prompt { name } => commands::create::run_prompt(&name)?,
            CreateCommand::Skill { name } => commands::create::run_skill(&name)?,
        },
        Commands::Run {
            prompt,
            agent_file,
            model,
        } => {
            commands::run::execute(&cli.gateway_url, prompt, &agent_file, model).await?;
        }
        Commands::Config { subcommand } => match subcommand {
            ConfigCommand::Show => commands::config_cmd::show()?,
            ConfigCommand::Set { key, value } => commands::config_cmd::set(&key, &value)?,
            ConfigCommand::Validate => commands::config_cmd::validate()?,
            ConfigCommand::Reload => {
                commands::config_cmd::reload(&cli.gateway_url).await?;
            }
        },
        Commands::Llm { subcommand } => match subcommand {
            LlmCommand::List => commands::llm::list(&cli.gateway_url).await?,
            LlmCommand::Test { provider } => {
                commands::llm::test(&cli.gateway_url, &provider).await?;
            }
            LlmCommand::Cost => commands::llm::cost(&cli.gateway_url).await?,
        },
        Commands::Prompt { subcommand } => match subcommand {
            PromptCommand::List => commands::prompt::list()?,
            PromptCommand::Show { name } => commands::prompt::show(&name)?,
            PromptCommand::Tune { name, dataset } => {
                commands::prompt::tune(&name, dataset)?;
            }
            PromptCommand::AbTest {
                name,
                baseline,
                candidate,
            } => {
                commands::prompt::ab_test(&name, &baseline, &candidate)?;
            }
        },
        Commands::Market { subcommand } => match subcommand {
            MarketCommand::Search { keyword } => {
                commands::market::search(&cli.gateway_url, &keyword).await?;
            }
            MarketCommand::Install { package } => {
                commands::market::install(&cli.gateway_url, &package).await?;
            }
            MarketCommand::Publish => {
                commands::market::publish(&cli.gateway_url).await?;
            }
        },
        Commands::Deploy { subcommand } => match subcommand {
            DeployCommand::Deploy { target } => {
                commands::deploy::deploy(&cli.gateway_url, &target).await?;
            }
            DeployCommand::Status => {
                commands::deploy::status(&cli.gateway_url).await?;
            }
            DeployCommand::Logs { lines } => {
                commands::deploy::logs(&cli.gateway_url, lines).await?;
            }
        },
        Commands::Completion { shell } => {
            let mut cmd = <Cli as clap::CommandFactory>::command();
            let name = cmd.get_name().to_string();
            clap_complete::generate(shell, &mut cmd, name, &mut io::stdout());
        }
    }

    Ok(())
}