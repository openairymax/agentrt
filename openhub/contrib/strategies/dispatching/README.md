# Dispatching Strategy

Task dispatching and routing strategy for OpenHub AgentOS.

## Overview

The dispatching strategy module provides intelligent task routing and agent selection mechanisms for the OpenHub platform. It analyzes task requirements and matches them with suitable agents based on capabilities, availability, and performance metrics.

## Features

- Capability-based agent matching
- Load balancing across agents
- Priority-based task routing
- Performance-based selection
- Affinity-based routing
- Resource-aware scheduling

## Architecture

```
                    ┌─────────────────┐
                    │   Task Input    │
                    └────────┬────────┘
                             │
                    ┌────────▼────────┐
                    │  Task Analyzer  │
                    └────────┬────────┘
                             │
        ┌────────────────────┼────────────────────┐
        │                    │                    │
┌───────▼───────┐   ┌───────▼───────┐   ┌───────▼───────┐
│  Capability   │   │   Priority    │   │    Load      │
│   Matcher     │   │   Router      │   │   Balancer   │
└───────┬───────┘   └───────┬───────┘   └───────┬───────┘
        │                   │                    │
        └───────────────────┼────────────────────┘
                            │
                   ┌────────▼────────┐
                   │  Agent         │
                   │  Selector      │
                   └────────┬────────┘
                            │
                   ┌────────▼────────┐
                   │  Dispatcher     │
                   └─────────────────┘
```

## Usage

```python
from dispatching import DispatchingStrategy, TaskRequirements, AgentCapability

# Define task requirements
requirements = TaskRequirements(
    required_capabilities=["code_generation", "testing"],
    preferred_agent=None,
    priority="high",
    estimated_complexity=7
)

# Create strategy
strategy = DispatchingStrategy()

# Select best agent for task
selected = strategy.select_agent(requirements, available_agents)
print(f"Selected agent: {selected.name}")

# Dispatch task
result = strategy.dispatch(task, selected)
```

## Configuration

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| load_balance_mode | string | "round_robin" | Load balancing mode |
| capability_weight | float | 0.4 | Weight for capability matching |
| priority_weight | float | 0.3 | Weight for priority routing |
| load_weight | float | 0.3 | Weight for load balancing |
| max_retry_attempts | integer | 3 | Max dispatch retry attempts |
| timeout_seconds | integer | 300 | Dispatch timeout |

## License

MIT
