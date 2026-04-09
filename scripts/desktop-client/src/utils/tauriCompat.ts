// Tauri API 兼容层 - 支持浏览器开发环境
let invoke: any;

try {
  invoke = require("@tauri-apps/api/core").invoke;
} catch (error) {
  // 在浏览器开发模式下提供模拟实现
  invoke = async (cmd: string, args?: any): Promise<any> => {
    console.log(`[Mock Tauri] invoke: ${cmd}`, args);
    
    // 模拟系统信息
    if (cmd === "get_system_info") {
      return {
        os: "Windows",
        os_version: "11",
        architecture: "x64",
        cpu_cores: 8,
        total_memory_gb: 16.0,
        free_memory_gb: 8.5,
        hostname: "agentos-dev"
      };
    }
    
    // 模拟服务状态
    if (cmd === "get_service_status") {
      return [
        { name: "gateway", status: "running", healthy: true, port: 18789 },
        { name: "kernel", status: "running", healthy: true, port: 18080 },
        { name: "postgres", status: "running", healthy: true, port: 5432 },
        { name: "redis", status: "running", healthy: true, port: 6379 },
        { name: "llm_service", status: "stopped", healthy: false, port: 8080 },
        { name: "tool_service", status: "running", healthy: true, port: 8081 }
      ];
    }
    
    // 模拟启动服务
    if (cmd === "start_services") {
      return {
        success: true,
        stdout: "Services started successfully",
        stderr: "",
        exit_code: 0
      };
    }
    
    // 模拟停止服务
    if (cmd === "stop_services") {
      return {
        success: true,
        stdout: "Services stopped successfully",
        stderr: "",
        exit_code: 0
      };
    }
    
    // 模拟重启服务
    if (cmd === "restart_services") {
      return {
        success: true,
        stdout: "Services restarted successfully",
        stderr: "",
        exit_code: 0
      };
    }
    
    // 模拟代理列表
    if (cmd === "list_agents") {
      return [
        {
          id: "agent-001",
          name: "Code Assistant",
          type: "coding",
          status: "idle",
          capabilities: ["code_generation", "debugging", "refactoring"],
          created_at: "2026-04-08T10:00:00Z"
        },
        {
          id: "agent-002",
          name: "Data Analyst",
          type: "analysis",
          status: "running",
          capabilities: ["data_analysis", "visualization", "reporting"],
          created_at: "2026-04-08T11:30:00Z"
        },
        {
          id: "agent-003",
          name: "Research Agent",
          type: "research",
          status: "error",
          capabilities: ["web_search", "document_analysis", "summarization"],
          created_at: "2026-04-08T12:00:00Z"
        }
      ];
    }
    
    // 模拟代理详情
    if (cmd === "get_agent_details") {
      return {
        id: args?.agent_id || "agent-001",
        name: "Code Assistant",
        type: "coding",
        status: "idle",
        capabilities: ["code_generation", "debugging", "refactoring"],
        config: {
          model: "gpt-4",
          temperature: 0.7,
          max_tokens: 2000
        },
        created_at: "2026-04-08T10:00:00Z",
        last_active: "2026-04-09T15:30:00Z"
      };
    }
    
    // 模拟提交任务
    if (cmd === "submit_task") {
      return {
        task_id: "task-" + Date.now(),
        status: "queued",
        message: "Task submitted successfully"
      };
    }
    
    // 模拟读取配置文件
    if (cmd === "read_config_file") {
      const configFiles: Record<string, string> = {
        "agentos.yaml": `# AgentOS Configuration
version: "1.0"
environment: development

server:
  host: 0.0.0.0
  port: 18789
  
database:
  type: postgresql
  host: localhost
  port: 5432
  name: agentos
  
redis:
  host: localhost
  port: 6379
  
logging:
  level: info
  format: json
`,
        ".env": `# Environment Variables
NODE_ENV=development
API_PORT=18789
DATABASE_URL=postgresql://localhost:5432/agentos
REDIS_URL=redis://localhost:6379
SECRET_KEY=dev-secret-key-change-in-production
`,
        "docker-compose.yml": `version: '3.8'
services:
  gateway:
    image: agentos/gateway:latest
    ports:
      - "18789:18789"
    environment:
      - NODE_ENV=production
      
  kernel:
    image: agentos/kernel:latest
    ports:
      - "18080:18080"
      
  postgres:
    image: postgres:15
    environment:
      POSTGRES_DB: agentos
      POSTGRES_USER: agentos
      POSTGRES_PASSWORD: \${POSTGRES_PASSWORD}
`
      };
      return configFiles[args?.filename || "agentos.yaml"] || "# File not found";
    }
    
    // 模拟写入配置文件
    if (cmd === "write_config_file") {
      return { success: true, message: "Configuration saved" };
    }
    
    // 模拟获取日志
    if (cmd === "get_logs") {
      const logs = [
        { timestamp: "2026-04-09T15:58:00Z", level: "INFO", service: "gateway", message: "Gateway service started on port 18789" },
        { timestamp: "2026-04-09T15:58:01Z", level: "INFO", service: "kernel", message: "Kernel initialized successfully" },
        { timestamp: "2026-04-09T15:58:02Z", level: "DEBUG", service: "gateway", message: "Health check endpoint registered" },
        { timestamp: "2026-04-09T15:58:05Z", level: "INFO", service: "postgres", message: "Database connection pool created" },
        { timestamp: "2026-04-09T15:58:10Z", level: "WARNING", service: "redis", message: "High memory usage detected: 85%" },
        { timestamp: "2026-04-09T15:58:15Z", level: "ERROR", service: "llm_service", message: "Failed to connect to LLM provider: timeout" },
        { timestamp: "2026-04-09T15:58:20Z", level: "INFO", service: "tool_service", message: "Tool service ready with 15 tools" },
        { timestamp: "2026-04-09T15:58:25Z", level: "DEBUG", service: "kernel", message: "IPC channel established" },
        { timestamp: "2026-04-09T15:58:30Z", level: "INFO", service: "gateway", message: "Request received: GET /api/health" },
        { timestamp: "2026-04-09T15:58:35Z", level: "INFO", service: "gateway", message: "Response sent: 200 OK (2ms)" }
      ];
      return logs.filter((log: any) => !args?.service || log.service === args.service);
    }
    
    // 模拟健康检查
    if (cmd === "get_health_status") {
      return {
        overall: "healthy",
        services: {
          gateway: "healthy",
          kernel: "healthy",
          postgres: "healthy",
          redis: "healthy",
          llm_service: "unhealthy",
          tool_service: "healthy"
        },
        uptime: "2h 15m 30s",
        version: "0.1.0"
      };
    }
    
    // 模拟打开浏览器
    if (cmd === "open_browser") {
      console.log(`[Mock] Opening browser: ${args?.url}`);
      window.open(args?.url, "_blank");
      return { success: true };
    }
    
    // 模拟打开终端
    if (cmd === "open_terminal") {
      console.log(`[Mock] Opening terminal`);
      return { success: true };
    }
    
    // 模拟执行 CLI 命令
    if (cmd === "execute_cli_command") {
      return {
        success: true,
        stdout: `Command executed: ${args?.command}\nOutput: Mock command output`,
        stderr: "",
        exit_code: 0,
        duration_ms: 150
      };
    }
    
    // 模拟测试后端连接
    if (cmd === "test_backend_connection") {
      return {
        success: true,
        message: "Connection successful"
      };
    }
    
    // 默认返回
    return null;
  };
}

export { invoke };
