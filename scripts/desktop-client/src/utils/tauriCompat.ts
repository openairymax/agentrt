type InvokeFn = {
  <T = unknown>(cmd: string, args?: Record<string, unknown>): Promise<T>;
};

const mockInvoke = async <T = unknown>(cmd: string, args?: Record<string, unknown>): Promise<T> => {
  console.log(`[Mock Tauri] invoke: ${cmd}`, args);

  const mockData: Record<string, unknown> = {
    get_system_info: {
      os: "Windows",
      os_version: "11",
      architecture: "x64",
      cpu_cores: 8,
      total_memory_gb: 16.0,
      free_memory_gb: 8.5,
      hostname: "agentos-dev"
    },
    get_service_status: [
      { name: "gateway", status: "running", healthy: true, port: 18789 },
      { name: "kernel", status: "running", healthy: true, port: 18080 },
      { name: "postgres", status: "running", healthy: true, port: 5432 },
      { name: "redis", status: "running", healthy: true, port: 6379 },
      { name: "llm_service", status: "stopped", healthy: false, port: 8080 },
      { name: "tool_service", status: "running", healthy: true, port: 8081 }
    ],
    start_services: { success: true, stdout: "Services started successfully", stderr: "", exit_code: 0 },
    stop_services: { success: true, stdout: "Services stopped successfully", stderr: "", exit_code: 0 },
    restart_services: { success: true, stdout: "Services restarted successfully", stderr: "", exit_code: 0 },
    list_agents: [
      { id: "agent-001", name: "Code Assistant", type: "coding", status: "idle", capabilities: ["code_generation", "debugging", "refactoring"], created_at: "2026-04-08T10:00:00Z" },
      { id: "agent-002", name: "Data Analyst", type: "analysis", status: "running", capabilities: ["data_analysis", "visualization", "reporting"], created_at: "2026-04-08T11:30:00Z" },
      { id: "agent-003", name: "Research Agent", type: "research", status: "error", capabilities: ["web_search", "document_analysis", "summarization"], created_at: "2026-04-08T12:00:00Z" }
    ],
    get_agent_details: {
      id: args?.agent_id || "agent-001",
      name: "Code Assistant",
      type: "coding",
      status: "idle",
      capabilities: ["code_generation", "debugging", "refactoring"],
      config: { model: "gpt-4", temperature: 0.7, max_tokens: 2000 },
      created_at: "2026-04-08T10:00:00Z",
      last_active: "2026-04-09T15:30:00Z"
    },
    submit_task: { task_id: "task-" + Date.now(), status: "queued", message: "Task submitted successfully" },
    write_config_file: { success: true, message: "Configuration saved" },
    get_health_status: {
      overall: "healthy",
      services: { gateway: "healthy", kernel: "healthy", postgres: "healthy", redis: "healthy", llm_service: "unhealthy", tool_service: "healthy" },
      uptime: "2h 15m 30s",
      version: "0.1.0"
    },
    open_browser: (() => { console.log(`[Mock] Opening browser: ${args?.url}`); window.open(args?.url as string, "_blank"); return { success: true }; })(),
    open_terminal: { success: true },
    execute_cli_command: {
      success: true,
      stdout: `Command executed: ${args?.command}\nOutput: Mock command output`,
      stderr: "",
      exit_code: 0,
      duration_ms: 150
    },
    test_backend_connection: { success: true, message: "Connection successful" }
  };

  if (cmd === "read_config_file") {
    const configFiles: Record<string, string> = {
      "agentos.yaml": `# AgentOS Configuration\nversion: "1.0"\nenvironment: development\n\nserver:\n  host: 0.0.0.0\n  port: 18789\n\ndatabase:\n  type: postgresql\n  host: localhost\n  port: 5432\n  name: agentos\n\nredis:\n  host: localhost\n  port: 6379\n\nlogging:\n  level: info\n  format: json\n`,
      ".env.production": `# Environment Variables\nNODE_ENV=development\nAPI_PORT=18789\nDATABASE_URL=postgresql://localhost:5432/agentos\nREDIS_URL=redis://localhost:6379\nSECRET_KEY=dev-secret-key-change-in-production\n`,
      "docker-compose.yml": `version: '3.8'\nservices:\n  gateway:\n    image: agentos/gateway:latest\n    ports:\n      - "18789:18789"\n  kernel:\n    image: agentos/kernel:latest\n    ports:\n      - "18080:18080"\n  postgres:\n    image: postgres:15\n    environment:\n      POSTGRES_DB: agentos\n`,
      "docker/docker-compose.yml": `version: '3.8'\nservices:\n  gateway:\n    image: agentos/gateway:latest\n    ports:\n      - "18789:18789"\n  kernel:\n    image: agentos/kernel:latest\n    ports:\n      - "18080:18080"\n`,
      "docker/docker-compose.prod.yml": `version: '3.8'\nservices:\n  gateway:\n    image: agentos/gateway:latest\n    ports:\n      - "18789:18789"\n    environment:\n      - NODE_ENV=production\n`
    };
    return (configFiles[(args?.filename || args?.path || "agentos.yaml") as string] || "# File not found") as T;
  }

  if (cmd === "get_logs") {
    const logs = [
      "2026-04-09T15:58:00Z [INFO]  [gateway] Gateway service started on port 18789",
      "2026-04-09T15:58:01Z [INFO]  [kernel] Kernel initialized successfully",
      "2026-04-09T15:58:02Z [DEBUG] [gateway] Health check endpoint registered",
      "2026-04-09T15:58:05Z [INFO]  [postgres] Database connection pool created",
      "2026-04-09T15:58:10Z [WARN]  [redis] High memory usage detected: 85%",
      "2026-04-09T15:58:15Z [ERROR] [llm_service] Failed to connect to LLM provider: timeout",
      "2026-04-09T15:58:20Z [INFO]  [tool_service] Tool service ready with 15 tools",
      "2026-04-09T15:58:25Z [DEBUG] [kernel] IPC channel established",
      "2026-04-09T15:58:30Z [INFO]  [gateway] Request received: GET /api/health",
      "2026-04-09T15:58:35Z [INFO]  [gateway] Response sent: 200 OK (2ms)"
    ];
    const filtered = args?.service
      ? logs.filter((line: string) => line.includes(args.service as string))
      : logs;
    return filtered.join("\n") as T;
  }

  if (cmd in mockData) {
    return mockData[cmd] as T;
  }

  return null as T;
};

let tauriInvoke: InvokeFn | null = null;

import("@tauri-apps/api/core")
  .then((mod) => {
    tauriInvoke = mod.invoke as InvokeFn;
  })
  .catch(() => {
    tauriInvoke = null;
  });

const invoke: InvokeFn = <T = unknown>(cmd: string, args?: Record<string, unknown>): Promise<T> => {
  if (tauriInvoke) {
    return tauriInvoke<T>(cmd, args);
  }
  return mockInvoke<T>(cmd, args);
};

export { invoke };
