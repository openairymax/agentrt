import { invoke } from "../utils/tauriCompat";

export interface SystemInfo {
  os: string;
  os_version: string;
  architecture: string;
  cpu_cores: number;
  total_memory_gb: number;
  free_memory_gb: number;
  hostname: string;
}

export interface ServiceStatus {
  name: string;
  status: string;
  healthy: boolean;
  uptime_seconds?: number | null;
  port?: number | null;
}

export interface AgentInfo {
  id: string;
  name: string;
  type?: string;
  status: string;
  task_count?: number;
  last_active?: string | null;
  description?: string;
  capabilities?: string[];
  config?: Record<string, unknown>;
  created_at?: string;
}

export interface TaskInfo {
  id: string;
  agent_id?: string;
  name?: string;
  type?: string;
  status: "pending" | "running" | "completed" | "failed" | "cancelled";
  progress: number;
  created_at: string;
  updated_at?: string | null;
  result?: unknown;
  error?: string;
}

export interface LLMProviderConfig {
  id: string;
  name: string;
  type: "openai" | "anthropic" | "localai" | "custom";
  api_key?: string;
  base_url: string;
  model: string;
  configured: boolean;
  max_tokens?: number;
  temperature?: number;
}

export interface LLMChatMessage {
  role: "system" | "user" | "assistant" | "tool";
  content: string;
  tool_calls?: ToolCall[];
  tool_call_id?: string;
}

export interface LLMChatRequest {
  provider_id: string;
  messages: LLMChatMessage[];
  model?: string;
  temperature?: number;
  max_tokens?: number;
  stream?: boolean;
  tools?: ToolDefinition[];
}

export interface LLMChatResponse {
  id: string;
  content: string;
  role: "assistant";
  model: string;
  finish_reason: "stop" | "length" | "tool_calls" | "content_filter";
  usage: { prompt_tokens: number; completion_tokens: number; total_tokens: number };
  tool_calls?: ToolCall[];
}

export interface LLMStreamChunk {
  id: string;
  delta: { content?: string; tool_calls?: ToolCall[] };
  finish_reason: string | null;
  usage?: { prompt_tokens: number; completion_tokens: number; total_tokens: number };
}

export interface MemoryEntry {
  id: string;
  type: "conversation" | "fact" | "skill" | "preference" | "error" | "observation";
  content: string;
  source?: string;
  metadata?: Record<string, unknown>;
  embedding?: number[];
  relevance?: number;
  tokens: number;
  created_at: string;
  updated_at?: string;
}

export interface MemoryStoreOptions {
  type: MemoryEntry["type"];
  content: string;
  source?: string;
  metadata?: Record<string, unknown>;
}

export interface MemorySearchOptions {
  query: string;
  limit?: number;
  type?: MemoryEntry["type"];
  min_relevance?: number;
}

export interface ToolDefinition {
  type: "function";
  function: {
    name: string;
    description: string;
    parameters: Record<string, unknown>;
    required?: string[];
  };
}

export interface ToolCall {
  id: string;
  type: "function";
  function: {
    name: string;
    arguments: string;
  };
}

export interface ToolResult {
  tool_call_id: string;
  output: string;
}

export interface CognitiveStep {
  phase: "perception" | "reasoning" | "action" | "reflection" | "idle";
  thought: string;
  detail?: string;
  timestamp: Date;
  tool_call?: ToolCall;
  tool_result?: unknown;
}

export interface AgentRuntimeMetrics {
  cycle_count: number;
  tool_call_count: number;
  memory_entries_count: number;
  avg_latency_ms: number;
  success_rate: number;
  total_tokens_consumed: number;
}

export interface VersionInfo {
  app_version: string;
  build_time: string;
  git_commit: string;
  rust_version: string;
  tauri_version: string;
}

export interface UpdateInfo {
  current_version: string;
  latest_version: string;
  update_available: boolean;
  release_url: string;
  release_notes: string;
}

// ==================== File System Types ====================

export interface FileInfo {
  name: string;
  path: string;
  is_dir: boolean;
  size_bytes: number;
  modified_at: string;
  permissions: string;
}

export interface DirectoryListing {
  path: string;
  files: FileInfo[];
  total_size: number;
  file_count: number;
  dir_count: number;
}

// ==================== Process Types ====================

export interface ProcessInfo {
  pid: number;
  name: string;
  cpu_percent: number;
  memory_mb: number;
  status: string;
  command: string;
  started_at: string;
}

// ==================== Network Types ====================

export interface NetworkInterface {
  name: string;
  ipv4: string;
  ipv6?: string;
  mac: string;
  is_up: boolean;
  bytes_sent: number;
  bytes_recv: number;
}

export interface PortCheckResult {
  port: number;
  host: string;
  open: boolean;
  latency_ms?: number;
  service?: string;
}

// ==================== Agent Lifecycle Types ====================

export interface AgentConfig {
  id: string;
  name: string;
  type: string;
  description?: string;
  model?: string;
  system_prompt?: string;
  tools?: string[];
  auto_start: boolean;
  max_concurrent_tasks: number;
  memory_config?: { max_entries: number; retention_days: number };
}

// ==================== System Monitor Types ====================

export interface SystemMonitorData {
  cpu: { usage_percent: number; cores: Array<{ usage: number; temp?: number }> };
  memory: { total_gb: number; used_gb: number; free_gb: number; percent: number };
  disk: { total_gb: number; used_gb: number; free_gb: number; percent: number; read_speed?: number; write_speed?: number };
  network: NetworkInterface[];
  uptime_seconds: number;
  load_average?: [number, number, number];
}

class AgentOSSDKError extends Error {
  constructor(
    message: string,
    public code: string,
    public originalError?: unknown
  ) {
    super(message);
    this.name = "AgentOSSDKError";
  }
}

async function sdkInvoke<T>(cmd: string, args?: Record<string, unknown>): Promise<T> {
  return invoke<T>(cmd, args as any);
}

function wrapError(op: string, err: unknown): never {
  const msg = err instanceof Error ? err.message : String(err);
  throw new AgentOSSDKError(`[${op}] ${msg}`, "SDK_ERROR", err);
}

export class AgentOSSDK {
  private cache = new Map<string, { data: unknown; timestamp: number }>();
  private readonly CACHE_TTL = 5000;

  private cachedGet<T>(key: string, fetcher: () => Promise<T>): Promise<T> {
    const cached = this.cache.get(key);
    if (cached && Date.now() - cached.timestamp < this.CACHE_TTL) {
      return Promise.resolve(cached.data as T);
    }
    return fetcher().then(data => {
      this.cache.set(key, { data, timestamp: Date.now() });
      return data;
    });
  }

  private invalidateCache(pattern?: string) {
    if (!pattern) { this.cache.clear(); return; }
    for (const key of this.cache.keys()) {
      if (key.includes(pattern)) this.cache.delete(key);
    }
  }

  async getSystemInfo(): Promise<SystemInfo> { try { return await sdkInvoke<SystemInfo>("get_system_info"); } catch (e) { wrapError("getSystemInfo", e); } }

  async executeCliCommand(command: string, args: string[]): Promise<{ success: boolean; stdout: string; stderr: string; exit_code: number }> { try { return await sdkInvoke("execute_cli_command", { command, args }); } catch (e) { wrapError("executeCliCommand", e); } }

  async getServiceStatus(): Promise<ServiceStatus[]> { try { return this.cachedGet("service_status", () => sdkInvoke<ServiceStatus[]>("get_service_status")); } catch (e) { wrapError("getServiceStatus", e); } }

  async getHealthStatus(): Promise<{ services: ServiceStatus[]; overall: string }> { try { const services = await this.getServiceStatus(); return { services, overall: services.every(s => s.healthy) ? "healthy" : services.some(s => s.healthy) ? "degraded" : "unhealthy" }; } catch (e) { wrapError("getHealthStatus", e); } }

  async startServices(mode: "dev" | "prod" = "dev"): Promise<void> { try { this.invalidateCache("service"); await sdkInvoke("start_services", { mode }); } catch (e) { wrapError("startServices", e); } }

  async stopServices(): Promise<void> { try { this.invalidateCache("service"); await sdkInvoke("stop_services"); } catch (e) { wrapError("stopServices", e); } }

  async restartServices(mode: "dev" | "prod" = "dev"): Promise<void> { try { this.invalidateCache("service"); await sdkInvoke("restart_services", { mode }); } catch (e) { wrapError("restartServices", e); } }

  async listAgents(): Promise<AgentInfo[]> { try { return await sdkInvoke<AgentInfo[]>("list_agents"); } catch (e) { wrapError("listAgents", e); } }

  async getAgentDetails(agentId: string): Promise<AgentInfo> { try { return await sdkInvoke<AgentInfo>("get_agent_details", { agent_id: agentId }); } catch (e) { wrapError("getAgentDetails", e); } }

  async registerAgent(name: string, type: string, description?: string): Promise<AgentInfo> { try { this.invalidateCache("agent"); return await sdkInvoke<AgentInfo>("register_agent", { agent_name: name, agent_type: type, description: description || "" }); } catch (e) { wrapError("registerAgent", e); } }

  async submitTask(agentId: string, taskName: string, params?: Record<string, unknown>, priority?: string): Promise<TaskInfo> { try { return await sdkInvoke<TaskInfo>("submit_task", { agent_id: agentId, task_description: JSON.stringify({ name: taskName, ...params }), priority }); } catch (e) { wrapError("submitTask", e); } }

  async getTaskStatus(taskId: string): Promise<TaskInfo> { try { return await sdkInvoke<TaskInfo>("get_task_status", { task_id: taskId }); } catch (e) { wrapError("getTaskStatus", e); } }

  async listTasks(): Promise<TaskInfo[]> { try { return await sdkInvoke<TaskInfo[]>("list_tasks"); } catch (e) { wrapError("listTasks", e); } }

  async cancelTask(taskId: string): Promise<void> { try { await sdkInvoke("cancel_task", { task_id: taskId }); } catch (e) { wrapError("cancelTask", e); } }

  async deleteTask(taskId: string): Promise<void> { try { await sdkInvoke("delete_task", { task_id: taskId }); } catch (e) { wrapError("deleteTask", e); } }

  async stopTask(taskId: string): Promise<TaskInfo> { try { return await sdkInvoke<TaskInfo>("stop_task", { task_id: taskId }); } catch (e) { wrapError("stopTask", e); } }

  async restartTask(taskId: string): Promise<TaskInfo> { try { return await sdkInvoke<TaskInfo>("restart_task", { task_id: taskId }); } catch (e) { wrapError("restartTask", e); } }

  async chat(request: LLMChatRequest): Promise<LLMChatResponse> { try { return await sdkInvoke<LLMChatResponse>("llm_chat", request as unknown as Record<string, unknown>); } catch (e) { wrapError("llm_chat", e); } }

  async testLLMConnection(providerId: string): Promise<{ success: boolean; message: string; latency_ms?: number; models?: string[] }> { try { return await sdkInvoke("test_llm_connection", { provider_id: providerId }); } catch (e) { wrapError("testLLMConnection", e); } }

  async listLLMProviders(): Promise<LLMProviderConfig[]> { try { return await sdkInvoke<LLMProviderConfig[]>("list_llm_providers"); } catch (e) { wrapError("listLLMProviders", e); } }

  async saveLLMProvider(config: Partial<LLMProviderConfig> & { id?: string }): Promise<LLMProviderConfig> { try { return await sdkInvoke<LLMProviderConfig>("save_llm_provider", { config: config as unknown as Record<string, unknown> }); } catch (e) { wrapError("saveLLMProvider", e); } }

  async deleteLLMProvider(providerId: string): Promise<void> { try { await sdkInvoke("delete_llm_provider", { provider_id: providerId }); } catch (e) { wrapError("deleteLLMProvider", e); } }

  async memoryStore(options: MemoryStoreOptions): Promise<MemoryEntry> { try { return await sdkInvoke<MemoryEntry>("memory_store", options as unknown as Record<string, unknown>); } catch (e) { wrapError("memory_store", e); } }

  async memorySearch(options: MemorySearchOptions): Promise<MemoryEntry[]> { try { return await sdkInvoke<MemoryEntry[]>("memory_search", options as unknown as Record<string, unknown>); } catch (e) { wrapError("memory_search", e); } }

  async memoryList(type?: MemoryEntry["type"], limit?: number): Promise<MemoryEntry[]> { try { return await sdkInvoke<MemoryEntry[]>("memory_list", { type: type || "", limit: limit || 100 }); } catch (e) { wrapError("memoryList", e); } }

  async memoryDelete(memoryId: string): Promise<void> { try { await sdkInvoke("memory_delete", { memory_id: memoryId }); } catch (e) { wrapError("memoryDelete", e); } }

  async memoryClear(type?: MemoryEntry["type"]): Promise<number> { try { return await sdkInvoke<number>("memory_clear", { type: type || "" }); } catch (e) { wrapError("memoryClear", e); } }

  async memoryEvolve(): Promise<{ evolved: number; layers: Array<{ layer: string; before: number; after: number }> }> { try { return await sdkInvoke("memory_evolve"); } catch (e) { wrapError("memoryEvolve", e); } }

  async getContextWindowStats(): Promise<{ total_tokens: number; max_tokens: number; used_percent: number; breakdown: { system: number; history: number; tools: number; output: number } }> { try { return await sdkInvoke("context_window_stats"); } catch (e) { wrapError("getContextWindowStats", e); } }

  async runCognitiveLoop(input: string, tools?: ToolDefinition[]): Promise<CognitiveStep[]> { try { return await sdkInvoke<CognitiveStep[]>("run_cognitive_loop", { input, tools: tools || [] }); } catch (e) { wrapError("runCognitiveLoop", e); } }

  async callTool(name: string, args: Record<string, unknown>): Promise<ToolResult> { try { return await sdkInvoke<ToolResult>("call_tool", { name, arguments: JSON.stringify(args) }); } catch (e) { wrapError("callTool", e); } }

  async listAvailableTools(): Promise<Array<{ name: string; description: string; category: string; schema: Record<string, unknown> }>> { try { return await sdkInvoke("list_tools"); } catch (e) { wrapError("listTools", e); } }

  async executeTool(name: string, args: Record<string, unknown>): Promise<ToolResult> { try { return await sdkInvoke<ToolResult>("call_tool", { name, arguments: JSON.stringify(args) }); } catch (e) { wrapError("executeTool", e); } }

  async registerTool(tool: { name: string; description: string; category: string; schema: Record<string, unknown> }): Promise<void> { try { await sdkInvoke("register_tool", tool); } catch (e) { wrapError("registerTool", e); } }

  async getRuntimeMetrics(): Promise<AgentRuntimeMetrics> { try { return await sdkInvoke<AgentRuntimeMetrics>("runtime_metrics"); } catch (e) { wrapError("getRuntimeMetrics", e); } }

  async readConfigFile(path: string): Promise<string> { try { return await sdkInvoke<string>("read_config_file", { path }); } catch (e) { wrapError("readConfigFile", e); } }

  async writeConfigFile(path: string, content: string): Promise<void> { try { await sdkInvoke("write_config_file", { path, content }); } catch (e) { wrapError("writeConfigFile", e); } }

  async getLogs(service?: string, tail?: number): Promise<string> { try { return await sdkInvoke<string>("get_logs", { service: service || "", tail: tail || 100 }); } catch (e) { wrapError("getLogs", e); } }

  async openBrowser(url: string): Promise<void> { try { await sdkInvoke("open_browser", { url }); } catch (e) { wrapError("openBrowser", e); } }

  async openTerminal(workingDir?: string): Promise<void> { try { await sdkInvoke("open_terminal", { working_dir: workingDir || "" }); } catch (e) { wrapError("openTerminal", e); } }

  async getVersionInfo(): Promise<VersionInfo> { try { return this.cachedGet("version_info", () => sdkInvoke<VersionInfo>("get_version_info")); } catch (e) { wrapError("getVersionInfo", e); } }

  async checkForUpdates(): Promise<UpdateInfo> { try { return await sdkInvoke<UpdateInfo>("check_for_updates"); } catch (e) { wrapError("checkForUpdates", e); } }

  async installUpdate(): Promise<void> { try { await sdkInvoke("download_and_install_update"); } catch (e) { wrapError("installUpdate", e); } }

  // ==================== Settings ====================

  async saveSettings(settings: Record<string, unknown>): Promise<void> { try { await sdkInvoke("save_settings", { settings }); } catch (e) { wrapError("saveSettings", e); } }

  async loadSettings(): Promise<Record<string, unknown>> { try { return await sdkInvoke<Record<string, unknown>>("load_settings"); } catch (e) { wrapError("loadSettings", e); } }

  // ==================== Agent Lifecycle Management ====================

  async startAgent(agentId: string): Promise<AgentInfo> { try { this.invalidateCache("agent"); return await sdkInvoke<AgentInfo>("start_agent", { agent_id: agentId }); } catch (e) { wrapError("startAgent", e); } }

  async stopAgent(agentId: string): Promise<AgentInfo> { try { this.invalidateCache("agent"); return await sdkInvoke<AgentInfo>("stop_agent", { agent_id: agentId }); } catch (e) { wrapError("stopAgent", e); } }

  async getAgentConfig(agentId: string): Promise<AgentConfig> { try { return this.cachedGet(`agent_config_${agentId}`, () => sdkInvoke<AgentConfig>("get_agent_config", { agent_id: agentId })); } catch (e) { wrapError("getAgentConfig", e); } }

  async updateAgentConfig(agentId: string, config: Partial<AgentConfig>): Promise<AgentConfig> { try { this.invalidateCache("agent_config"); return await sdkInvoke<AgentConfig>("update_agent_config", { agent_id: agentId, config: config as unknown as Record<string, unknown> }); } catch (e) { wrapError("updateAgentConfig", e); } }

  // ==================== File System Operations ====================

  async listDirectory(path: string): Promise<DirectoryListing> { try { return await sdkInvoke<DirectoryListing>("list_directory", { path }); } catch (e) { wrapError("listDirectory", e); } }

  async readFile(path: string): Promise<string> { try { return await sdkInvoke<string>("read_file", { path }); } catch (e) { wrapError("readFile", e); } }

  async writeFile(path: string, content: string): Promise<void> { try { await sdkInvoke("write_file", { path, content }); } catch (e) { wrapError("writeFile", e); } }

  async deleteFile(path: string): Promise<void> { try { await sdkInvoke("delete_file", { path }); } catch (e) { wrapError("deleteFile", e); } }

  async copyFile(src: string, dst: string): Promise<void> { try { await sdkInvoke("copy_file", { src, dst }); } catch (e) { wrapError("copyFile", e); } }

  async moveFile(src: string, dst: string): Promise<void> { try { await sdkInvoke("move_file", { src, dst }); } catch (e) { wrapError("moveFile", e); } }

  async createDirectory(path: string): Promise<void> { try { await sdkInvoke("create_directory", { path }); } catch (e) { wrapError("createDirectory", e); } }

  // ==================== Process Management ====================

  async listProcesses(): Promise<ProcessInfo[]> { try { return await sdkInvoke<ProcessInfo[]>("list_processes"); } catch (e) { wrapError("listProcesses", e); } }

  async killProcess(pid: number, force?: boolean): Promise<void> { try { await sdkInvoke("kill_process", { pid, force: force || false }); } catch (e) { wrapError("killProcess", e); } }

  async getProcessInfo(pid: number): Promise<ProcessInfo> { try { return await sdkInvoke<ProcessInfo>("get_process_info", { pid }); } catch (e) { wrapError("getProcessInfo", e); } }

  // ==================== Network Diagnostics ====================

  async getNetworkInterfaces(): Promise<NetworkInterface[]> { try { return await sdkInvoke<NetworkInterface[]>("get_network_interfaces"); } catch (e) { wrapError("getNetworkInterfaces", e); } }

  async checkPort(host: string, port: number, timeoutMs?: number): Promise<PortCheckResult> { try { return await sdkInvoke<PortCheckResult>("check_port", { host, port, timeout_ms: timeoutMs || 3000 }); } catch (e) { wrapError("checkPort", e); } }

  async ping(host: string, count?: number): Promise<{ host: string; packets_sent: number; packets_received: number; packet_loss_percent: number; avg_latency_ms: number }> {
    try { return await sdkInvoke("ping", { host, count: count || 4 }); } catch (e) { wrapError("ping", e); }
  }

  async dnsLookup(hostname: string): Promise<{ hostname: string; addresses: string[] }> {
    try { return await sdkInvoke("dns_lookup", { hostname }); } catch (e) { wrapError("dnsLookup", e); }
  }

  // ==================== System Monitor ====================

  async getSystemMonitorData(): Promise<SystemMonitorData> { try { return this.cachedGet("system_monitor", () => sdkInvoke<SystemMonitorData>("system_monitor")); } catch (e) { wrapError("getSystemMonitorData", e); } }
}

declare global {
  interface Window {
    __agentos_stream_callback?: (chunk: LLMStreamChunk) => void;
    __agentos_stream_done?: (fullResponse: LLMChatResponse) => void;
  }
}

export const sdk = new AgentOSSDK();

export default sdk;
