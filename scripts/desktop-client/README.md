# AgentOS Desktop Client

Official cross-platform desktop application for AgentOS - built with [Tauri](https://tauri.app) v2.

## Features

- 🖥️ **Dashboard** - Real-time system monitoring with resource usage, service status, and health metrics
- 🐳 **Services Management** - Start/stop/restart Docker services (dev/prod modes)
- 🤖 **Agent Management** - View registered AI agents, submit tasks, monitor execution
- 📋 **Task Management** - Submit tasks to agents, track progress, view history
- ⚙️ **Configuration Editor** - Edit config files with syntax highlighting and validation
- 📄 **Log Viewer** - Real-time log streaming with filtering and search
- 💻 **Integrated Terminal** - Execute CLI commands directly from the UI
- 🔄 **Auto-refresh** - Automatic status updates for real-time monitoring

## Tech Stack

- **Frontend**: React 18 + TypeScript + Vite
- **Backend**: Rust (Tauri v2)
- **Styling**: CSS Variables (Dark Theme)
- **Icons**: Lucide React
- **State**: React Hooks + Tauri Commands

## Prerequisites

- Node.js >= 18.x
- Rust >= 1.70 (stable)
- Tauri CLI v2

## Quick Start

### Installation

```bash
# Clone the repository (if not already done)
cd desktop-client

# Install frontend dependencies
npm install

# Install Tauri CLI
npm install -g @tauri-apps/cli@latest

# Run in development mode
npm run tauri dev
```

### Build for Production

```bash
# Create production build
npm run tauri build

# Output location:
#   Windows: src-tauri/target/release/bundle/
#   macOS:   src-tauri/target/release/bundle/macos/
#   Linux:   src-tauri/target/release/release/bundle/linux/
```

## Project Structure

```
desktop-client/
├── src-tauri/              # Rust backend (Tauri)
│   ├── src/
│   │   ├── main.rs         # Entry point
│   │   ├── lib.rs          # Tauri setup & commands registration
│   │   ├── commands.rs     # All Tauri commands (API layer)
│   │   └── cli.rs          # CLI wrapper utilities
│   ├── Cargo.toml          # Rust dependencies
│   └── tauri.conf.json     # Tauri configuration
├── src/                    # Frontend (React)
│   ├── main.tsx            # React entry point
│   ├── App.tsx             # Main app with routing
│   ├── styles/
│   │   └── globals.css     # Global styles & theme
│   └── pages/
│       ├── Dashboard.tsx   # System overview
│       ├── Services.tsx    # Docker service management
│       ├── Agents.tsx      # AI agent management
│       ├── Tasks.tsx       # Task submission & tracking
│       ├── Config.tsx      # Configuration editor
│       ├── Logs.tsx        # Log viewer
│       └── Terminal.tsx    # Integrated terminal
├── index.html              # HTML template
├── package.json            # Frontend dependencies
├── tsconfig.json           # TypeScript config
├── vite.config.ts          # Vite build config
└── README.md               # This file
```

## Available Pages/Routes

| Route | Page | Description |
|-------|------|-------------|
| `/` | Dashboard | System overview with stats cards |
| `/services` | Services | Docker container management |
| `/agents` | Agents | AI agent registry & details |
| `/tasks` | Tasks | Task submission & history |
| `/config` | Config | Configuration file editor |
| `/logs` | Logs | System log viewer |
| `/terminal` | Terminal | Command-line interface |

## Key Features Explained

### Dashboard
- Real-time CPU, memory, service health metrics
- Auto-refresh every 30 seconds
- Quick action buttons for common operations

### Services
- Start/stop/restart all Docker services
- Switch between dev and prod modes
- View individual service status and ports
- Open services directly in browser

### Agents
- List all registered AI agents
- View agent details and task history
- Submit new tasks to agents
- Monitor agent status (running/idle/error)

### Tasks
- Submit tasks with priority levels
- Track task progress with visual indicators
- View task history with filtering
- Cancel running tasks

### Configuration
- Edit multiple config files safely
- Syntax highlighting for YAML/ENV/JSON
- Unsaved changes detection
- Security warnings for sensitive files

### Log Viewer
- Real-time log streaming (auto-refresh)
- Filter by service or search terms
- Color-coded log levels (error/warn/info)
- Export logs to file

### Integrated Terminal
- Execute any shell command directly
- Command history with arrow key navigation
- Preset quick commands for common operations
- Copy/clear output functionality

## Tauri Commands (Rust Backend)

The following commands are exposed to the frontend:

```rust
// System Information
get_system_info() -> SystemInfo
get_version_info() -> VersionInfo

// Service Management
get_service_status() -> Vec<ServiceStatus>
start_services(mode: String) -> CliCommandResult
stop_services() -> CliCommandResult
restart_services(mode: String) -> CliCommandResult
get_health_status() -> Vec<ServiceStatus>

// Agent Operations
list_agents() -> Vec<AgentInfo>
get_agent_details(agent_id: String) -> AgentInfo
submit_task(agent_id: String, description: String, priority: String) -> TaskInfo
get_task_status(task_id: String) -> TaskInfo
cancel_task(task_id: String)

// Configuration
read_config_file(path: String) -> String
write_config_file(path: String, content: String)

// Logging
get_logs(service: Option<String>, tail: Option<u32>) -> String

// CLI Execution
execute_cli_command(command: String, args: Vec<String>) -> CliCommandResult

// Utilities
open_terminal(working_dir: Option<String>)
open_browser(url: String)
check_for_updates() -> UpdateInfo
```

## Development Tips

### Hot Reload
- Frontend changes auto-reload via Vite HMR
- Rust backend requires restart (`Ctrl+C` then `npm run tauri dev`)

### Debugging
- DevTools automatically opens in development mode
- Check console for Tauri command errors
- Use `invoke()` return values for debugging

### Adding New Pages
1. Create component in `src/pages/`
2. Add route in `src/App.tsx`
3. Add nav item to `navItems` array
4. Register Tauri command if needed

## Platform Support

| Platform | Status | Notes |
|----------|--------|-------|
| Windows 10+ | ✅ Supported | Requires Windows 10 1903+ |
| macOS 12+ | ✅ Supported | Intel & Apple Silicon |
| Linux (Ubuntu 22.04+) | ✅ Tested | Debian-based distros |

## Performance

- Bundle size: < 20MB (vs Electron ~150MB)
- Memory usage: ~50-100MB idle
- Startup time: < 2 seconds

## Security

- No remote code execution
- File access limited to project directory
- Sensitive config files flagged with warnings
- CSP headers configured

## Future Enhancements (Roadmap)

- [ ] Multi-language support (i18n)
- [ ] Dark/Light theme toggle
- [ ] Plugin system for custom agents
- [ ] Real-time notifications
- [ ] Keyboard shortcuts
- [ ] Settings persistence
- [ ] Export/import configurations
- [ ] Team collaboration features
- [ ] Advanced metrics dashboard (charts/graphs)

## Contributing

1. Fork the repository
2. Create feature branch (`git checkout -b feature/amazing-feature`)
3. Commit changes (`git commit -m 'Add amazing feature'`)
4. Push to branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

MIT License - see [LICENSE](../LICENSE) for details.

## Support

- Documentation: https://docs.agentos.io
- Issues: https://github.com/SpharxTeam/AgentOS/issues
- Discord: [Join our community](https://discord.gg/agentos)

---

Built with ❤️ using [Tauri](https://tauri.app) and [React](https://react.dev)
