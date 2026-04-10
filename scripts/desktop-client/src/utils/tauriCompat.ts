type InvokeFn = {
  <T = unknown>(cmd: string, args?: Record<string, unknown>): Promise<T>;
};

const mockMemories = [
  { id: "m1", type: "conversation", content: "用户偏好使用中文进行交互，技术术语可保留英文", source: "chat-003", tokens: 18, createdAt: new Date(Date.now()-300000).toISOString() },
  { id: "m2", type: "fact", content: "AgentOS 使用 Docker Compose 管理服务，开发模式端口 18789", source: "system", tokens: 16, createdAt: new Date(Date.now()-600000).toISOString() },
  { id: "m3", type: "skill", content: "已学会：通过 invoke() 调用 Tauri 后端命令，支持异步返回", source: "learning", tokens: 14, createdAt: new Date(Date.now()-900000).toISOString() },
  { id: "m4", type: "preference", content: "用户喜欢简洁的回复格式，避免冗长解释", source: "chat-001", tokens: 12, createdAt: new Date(Date.now()-1200000).toISOString() },
  { id: "m5", type: "error", content: "上次启动 PostgreSQL 时因端口冲突失败，需先检查端口占用", source: "service-start", tokens: 20, createdAt: new Date(Date.now()-1800000).toISOString() },
  { id: "m6", type: "observation", content: "系统当前运行在 Windows 平台，x64 架构，16GB 内存", source: "system-scan", tokens: 15, createdAt: new Date(Date.now()-100000).toISOString() },
];

function generateSmartResponse(userInput: string): string {
  const input = userInput.toLowerCase();
  if (input.includes("启动") || input.includes("start") || input.includes("服务")) {
    return `已收到您的请求。让我先检查当前服务状态，然后执行启动操作。

**执行计划：**
1. 检查 Docker 环境是否就绪
2. 按 kernel → gateway → postgres → redis 的顺序依次启动
3. 验证每个服务的健康状态
4. 返回最终状态报告

正在执行... ✅ 所有服务已成功启动：
- **kernel**: running (port 18080)
- **gateway**: running (port 18789)
- **postgres**: running (port 5432)
- **redis**: running (port 6379)

服务集群已完全就绪。`;
  }
  if (input.includes("智能体") || input.includes("agent")) {
    return `关于智能体管理，AgentOS 支持以下操作：

**可用的智能体类型：**
1. **研究型** - 网络搜索、文档分析、信息汇总
2. **编码型** - 代码生成、调试、重构、审查
3. **助手型** - 通用对话、任务协调、问答
4. **系统型** - 服务管理、配置维护、监控

您可以点击「注册新智能体」来创建自定义智能体。`;
  }
  if (input.includes("你好") || input.includes("hello") || input.includes("hi")) {
    return `您好！👋 我是 AgentOS 智能助手。

我可以帮您完成以下操作：

🔧 **系统管理** — 启动/停止/重启服务集群
🤖 **智能体** — 注册和管理 AI 智能体
⚙️ **配置** — LLM 模型配置（OpenAI/Anthropic/本地模型）
💾 **文件与日志** — 编辑配置文件、查看实时日志

请告诉我您需要什么帮助！`;
  }
  if (input.includes("记忆") || input.includes("memory")) {
    return `**AgentOS 记忆系统状态：**

| 类型 | 条目数 | Token 占用 |
|------|--------|-----------|
| 对话记忆 | 3 | 54 |
| 事实记忆 | 2 | 31 |
| 技能记忆 | 1 | 14 |
| 偏好记忆 | 2 | 26 |
| 错误记忆 | 1 | 20 |
| 观察记忆 | 1 | 15 |

上下文窗口: 3,842 / 128,000 tokens (3.0%)`;
  }
  return `感谢您的提问：「${userInput}」

作为 AgentOS 智能助手，我已经理解了您的需求。基于当前的系统状态和可用工具，我建议我们可以按以下步骤进行：

1. **分析需求** — 理解具体目标和约束条件
2. **制定方案** — 选择最佳执行路径
3. **执行操作** — 调用相应工具完成任务
4. **验证结果** — 确认输出符合预期

如果您确认要继续，请回复「确认」，我将开始执行。`;
}

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
    test_backend_connection: (args: any) => ({ success: true, message: "Connection successful" }),
    select_directory: "C:\\Users\\Developer\\AgentOS",
    get_llm_usage: { total_tokens: 125000, input_tokens: 85000, output_tokens: 40000, estimated_cost: 2.15 },
    // LLM / AI Chat
    llm_chat: (args: any) => {
      const userMsg = args?.messages ? [...args.messages].filter((m: any) => m.role === "user").pop()?.content || "" : "";
      const content = generateSmartResponse(userMsg);
      return {
        id: `chatcmpl-${Date.now()}`,
        content,
        role: "assistant",
        model: args?.model || "gpt-4o",
        finish_reason: "stop",
        usage: { prompt_tokens: Math.floor(userMsg.length * 1.5), completion_tokens: Math.floor(content.length * 1.5), total_tokens: Math.floor((userMsg.length + content.length) * 1.5) },
        tool_calls: null,
      };
    },
    test_llm_connection: (args: any) => ({
      success: true, message: "API connection successful", latency_ms: 120 + Math.floor(Math.random() * 200),
      models: args?.provider_id === "openai" ? ["gpt-4o","gpt-4o-mini","gpt-4-turbo"]
        : args?.provider_id === "anthropic" ? ["claude-3.5-sonnet","claude-3-opus","claude-3-haiku"]
        : ["llama-3-70b","llama-3-8b","mistral-7b"],
    }),
    list_llm_providers: [
      { id: "openai", name: "OpenAI", type: "openai", base_url: "https://api.openai.com/v1", model: "gpt-4o", configured: false },
      { id: "anthropic", name: "Anthropic", type: "anthropic", base_url: "https://api.anthropic.com/v1", model: "claude-3.5-sonnet", configured: false },
      { id: "localai", name: "Local AI", type: "localai", base_url: "http://localhost:8080/v1", model: "llama-3-70b", configured: false },
    ],
    save_llm_provider: (args: any) => ({ ...args?.config, saved: true }),
    delete_llm_provider: () => ({ success: true }),
    // Memory System
    memory_store: (args: any) => {
      const entry = { id: `mem-${Date.now()}`, type: args?.type, content: args?.content, source: args?.source, tokens: Math.ceil((args?.content || "").length / 4), createdAt: new Date().toISOString() };
      return entry;
    },
    memory_search: (args: any) => mockMemories.filter((m: any) => (!args?.type || m.type === args.type) && (!args?.query || String(m.content).toLowerCase().includes(String(args.query).toLowerCase()))).slice(0, Number(args?.limit) || 10),
    memory_list: (args: any) => mockMemories.filter((m: any) => !args?.type || m.type === args.type).slice(0, Number(args?.limit) || 50),
    memory_delete: () => ({ success: true }),
    memory_clear: () => mockMemories.filter(() => false).length,
    context_window_stats: () => ({ totalTokens: 3842, maxTokens: 128000, usedPercent: 3.0, breakdown: { system: 256, history: 3200, tools: 128, output: 258 } }),
    // Cognitive Loop
    run_cognitive_loop: (args: any) => {
      const input = args?.input || "";
      const now = new Date().toISOString();
      return [
        { phase: "perception", thought: `收到用户输入：「${input}」`, detail: `长度 ${input.length} 字符`, timestamp: now },
        { phase: "reasoning", thought: "分析用户意图：识别为系统操作请求", detail: "置信度: 94%", timestamp: now },
        { phase: "reasoning", thought: "检索相关记忆：发现上次类似操作的配置模式", detail: "召回相关性: 0.88", timestamp: now },
        { phase: "action", thought: "调用工具执行操作", detail: "Tauri invoke → Rust backend", timestamp: now, tool_call: { id: "call_001", type: "function", function: { name: "execute_cli_command", arguments: "{}" } } },
        { phase: "reflection", thought: "评估结果：操作已完成", detail: "质量评分: A+", timestamp: now },
      ];
    },
    call_tool: (args: any) => ({ tool_call_id: `tc-${Date.now()}`, output: `Tool '${args?.name}' executed successfully` }),
    list_tools: () => [
      { name: "start_services", description: "启动服务集群", category: "system" },
      { name: "stop_services", description: "停止所有服务", category: "system" },
      { name: "get_service_status", description: "查询服务运行状态", category: "system" },
      { name: "list_agents", description: "列出已注册智能体", category: "agent" },
      { name: "register_agent", description: "注册新智能体", category: "agent" },
      { name: "submit_task", description: "提交异步任务", category: "task" },
      { name: "get_system_info", description: "获取系统硬件信息", category: "system" },
      { name: "memory_store", description: "存储记忆到向量库", category: "memory" },
      { name: "memory_search", description: "语义搜索长期记忆", category: "memory" },
      { name: "read_file", description: "读取本地文件", category: "io" },
    ],
    runtime_metrics: () => ({ cycle_count: 847, tool_call_count: 274, memory_entries_count: 10, avg_latency_ms: 1240, success_rate: 98.5, total_tokens_consumed: 42300 }),
    // Extended Tasks
    list_tasks: () => [
      { id: "task-001", agent_id: "agent-001", name: "代码审查 PR #142", type: "coding", status: "completed", progress: 100, created_at: new Date(Date.now()-86400000).toISOString(), result: { issues_found: 3 } },
      { id: "task-002", agent_id: "agent-002", name: "数据分析报告 Q1", type: "analysis", status: "running", progress: 67, created_at: new Date(Date.now()-3600000).toISOString() },
      { id: "task-003", agent_id: "agent-003", name: "竞品调研", type: "research", status: "failed", progress: 23, created_at: new Date(Date.now()-7200000).toISOString(), error: "API rate limit exceeded" },
    ],
    delete_task: () => ({ success: true }),
    restart_task: (args: any) => ({ id: args?.task_id, status: "pending", progress: 0, created_at: new Date().toISOString() }),
    // Extended Agents
    register_agent: (args: any) => ({ id: `agent-${Date.now()}`, name: args?.agent_name, type: args?.agent_type, status: "idle", task_count: 0, created_at: new Date().toISOString() }),
    // Settings
    save_settings: (_args: any) => ({ success: true }),
    load_settings: () => ({ language: "zh", theme: "light", serviceMode: "dev" }),
    // Agent Lifecycle
    start_agent: (args: any) => ({ id: args?.agent_id || "", name: "Agent-Running", type: "running", status: "running", task_count: 0 }),
    stop_agent: (args: any) => ({ id: args?.agent_id || "", name: "Agent-Stopped", type: "stopped", status: "stopped", task_count: 0 }),
    get_agent_config: (args: any) => ({
      id: args?.agent_id || "",
      name: "Default Agent",
      type: "assistant",
      model: "gpt-4o",
      system_prompt: "You are a helpful AI assistant powered by AgentOS.",
      tools: ["search", "code", "file"],
      auto_start: false,
      max_concurrent_tasks: 5,
      memory_config: { max_entries: 1000, retention_days: 30 },
    }),
    update_agent_config: (args: any) => ({ id: args?.agent_id || "", ...args?.config }),
    // File System Operations
    list_directory: (args: any) => {
      const mockFiles = [
        { name: ".gitignore", path: "/project/.gitignore", isDir: false, sizeBytes: 256, modifiedAt: Date.now(), permissions: "rw-r--r--" },
        { name: "package.json", path: "/project/package.json", isDir: false, sizeBytes: 2048, modifiedAt: Date.now(), permissions: "rw-r--r--" },
        { name: "src", path: "/project/src", isDir: true, sizeBytes: 4096, modifiedAt: Date.now(), permissions: "rwxr-xr-x" },
        { name: "node_modules", path: "/project/node_modules", isDir: true, sizeBytes: 8192, modifiedAt: Date.now(), permissions: "rwxr-xr-x" },
        { name: "README.md", path: "/project/README.md", isDir: false, sizeBytes: 1024, modifiedAt: Date.now(), permissions: "rw-r--r--" },
        { name: "tsconfig.json", path: "/project/tsconfig.json", isDir: false, sizeBytes: 512, modifiedAt: Date.now(), permissions: "rw-r--r--" },
      ];
      return { path: args?.path || "/", files: mockFiles, totalSize: 16000, fileCount: 4, dirCount: 2 };
    },
    read_file: (args: any) => ("Mock file content for: " + String(args?.path || "")),
    write_file: (_args: any) => undefined,
    delete_file: (_args: any) => undefined,
    copy_file: (_args: any) => undefined,
    move_file: (_args: any) => undefined,
    create_directory: (_args: any) => undefined,
    // Process Management
    list_processes: () => [
      { pid: 1, name: "System Idle Process", cpuPercent: 0, memoryMb: 0, status: "idle", command: "", startedAt: Date.now() },
      { pid: 1234, name: "node.exe", cpuPercent: 12.5, memoryMb: 180, status: "running", command: "node vite --port 1425", startedAt: Date.now() - 3600000 },
      { pid: 5678, name: "code.exe", cpuPercent: 8.3, memoryMb: 450, status: "running", command: "code --new-window", startedAt: Date.now() - 7200000 },
      { pid: 9012, name: "chrome.exe", cpuPercent: 25.7, memoryMb: 890, status: "running", command: "chrome --restore-last-session", startedAt: Date.now() - 10800000 },
      { pid: 3456, name: "explorer.exe", cpuPercent: 2.1, memoryMb: 120, status: "running", command: "explorer.exe", startedAt: Date.now() - 86400000 },
    ],
    kill_process: (_args: any) => undefined,
    get_process_info: (args: any) => ({ pid: args?.pid || 0, name: "test-process", cpuPercent: 10, memoryMb: 100, status: "running", command: "test", startedAt: Date.now() }),
    // Network Diagnostics
    get_network_interfaces: () => [
      { name: "Wi-Fi", ipv4: "192.168.1.100", ipv6: "::1", mac: "AA:BB:CC:DD:EE:FF", isUp: true, bytesSent: 500000000, bytesRecv: 1500000000 },
      { name: "Ethernet", ipv4: "10.0.0.50", mac: "11:22:33:44:55:66", isUp: false, bytesSent: 0, bytesRecv: 0 },
    ],
    check_port: (args: any) => ({ port: args?.port || 80, host: args?.host || "localhost", open: Math.random() > 0.3, latencyMs: Math.floor(Math.random() * 50) + 5 }),
    ping: (args: any) => ({ host: args?.host || "localhost", packetsSent: 4, packetsReceived: 4, packetLossPercent: 0, avgLatencyMs: 15 }),
    dns_lookup: (args: any) => ({ hostname: args?.hostname || "example.com", addresses: ["93.184.216.34"] }),
    // System Monitor
    system_monitor: () => ({
      cpu: { usagePercent: 23 + Math.random() * 15, cores: Array.from({ length: 8 }, (_, i) => ({ coreId: i, usage: 15 + Math.random() * 40 })) },
      memory: { totalGb: 16, usedGb: 8.5 + Math.random() * 3, freeGb: 4.5 - Math.random() * 3, percent: 53 + Math.random() * 18 },
      disk: { totalGb: 512, usedGb: 280 + Math.random() * 20, freeGb: 212 - Math.random() * 20, percent: 55 + Math.random() * 8 },
      network: [{ name: "Wi-Fi", ipv4: "192.168.1.100", mac: "AA:BB:CC:DD:EE:FF", isUp: true, bytesSent: 500000000, bytesRecv: 1500000000 }],
      uptimeSeconds: 172800 + Math.floor(Math.random() * 10000),
    }),
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



