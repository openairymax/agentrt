"use strict";
// AgentOS TypeScript SDK Tests
// Version: 1.0.0.5
// Last updated: 2026-03-21
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
const axios_1 = __importDefault(require("axios"));
const agent_1 = require("./agent");
// Mock axios
jest.mock('axios');
const mockedAxios = axios_1.default;
describe('AgentOS', () => {
    let agent;
    beforeEach(() => {
        agent = new agent_1.AgentOS({ endpoint: 'http://localhost:18789' });
    });
    afterEach(() => {
        jest.clearAllMocks();
    });
    test('should create a new AgentOS client', () => {
        expect(agent).toBeDefined();
    });
    test('should submit a task', async () => {
        // Mock response
        const mockResponse = {
            data: { task_id: 'test-task-id' },
            status: 200,
            statusText: 'OK',
            headers: {},
            config: {},
        };
        mockedAxios.create.mockReturnValue({
            post: jest.fn().mockResolvedValue(mockResponse),
            get: jest.fn(),
            delete: jest.fn(),
            interceptors: { response: { use: jest.fn() } },
        });
        // Create a new agent to use the mocked axios
        const testAgent = new agent_1.AgentOS({ endpoint: 'http://localhost:18789' });
        const task = await testAgent.submitTask('Test task');
        expect(task.id).toBe('test-task-id');
    });
    test('should write a memory', async () => {
        // Mock response
        const mockResponse = {
            data: { memory_id: 'test-memory-id' },
            status: 200,
            statusText: 'OK',
            headers: {},
            config: {},
        };
        mockedAxios.create.mockReturnValue({
            post: jest.fn().mockResolvedValue(mockResponse),
            get: jest.fn(),
            delete: jest.fn(),
            interceptors: { response: { use: jest.fn() } },
        });
        // Create a new agent to use the mocked axios
        const testAgent = new agent_1.AgentOS({ endpoint: 'http://localhost:18789' });
        const memoryId = await testAgent.writeMemory('Test memory');
        expect(memoryId).toBe('test-memory-id');
    });
    test('should search memories', async () => {
        // Mock response
        const mockResponse = {
            data: {
                memories: [
                    {
                        memory_id: 'test-memory-id',
                        content: 'Test memory',
                        created_at: '2026-03-21T00:00:00Z',
                        metadata: {},
                    },
                ],
            },
            status: 200,
            statusText: 'OK',
            headers: {},
            config: {},
        };
        mockedAxios.create.mockReturnValue({
            get: jest.fn().mockResolvedValue(mockResponse),
            post: jest.fn(),
            delete: jest.fn(),
            interceptors: { response: { use: jest.fn() } },
        });
        // Create a new agent to use the mocked axios
        const testAgent = new agent_1.AgentOS({ endpoint: 'http://localhost:18789' });
        const memories = await testAgent.searchMemory('test');
        expect(memories).toHaveLength(1);
        expect(memories[0].memoryId).toBe('test-memory-id');
    });
    test('should get a memory', async () => {
        // Mock response
        const mockResponse = {
            data: {
                memory_id: 'test-memory-id',
                content: 'Test memory',
                created_at: '2026-03-21T00:00:00Z',
                metadata: {},
            },
            status: 200,
            statusText: 'OK',
            headers: {},
            config: {},
        };
        mockedAxios.create.mockReturnValue({
            get: jest.fn().mockResolvedValue(mockResponse),
            post: jest.fn(),
            delete: jest.fn(),
            interceptors: { response: { use: jest.fn() } },
        });
        // Create a new agent to use the mocked axios
        const testAgent = new agent_1.AgentOS({ endpoint: 'http://localhost:18789' });
        const memory = await testAgent.getMemory('test-memory-id');
        expect(memory.memoryId).toBe('test-memory-id');
        expect(memory.content).toBe('Test memory');
    });
    test('should delete a memory', async () => {
        // Mock response
        const mockResponse = {
            data: { success: true },
            status: 200,
            statusText: 'OK',
            headers: {},
            config: {},
        };
        mockedAxios.create.mockReturnValue({
            delete: jest.fn().mockResolvedValue(mockResponse),
            get: jest.fn(),
            post: jest.fn(),
            interceptors: { response: { use: jest.fn() } },
        });
        // Create a new agent to use the mocked axios
        const testAgent = new agent_1.AgentOS({ endpoint: 'http://localhost:18789' });
        const result = await testAgent.deleteMemory('test-memory-id');
        expect(result).toBe(true);
    });
    test('should create a session', async () => {
        // Mock response
        const mockResponse = {
            data: { session_id: 'test-session-id' },
            status: 200,
            statusText: 'OK',
            headers: {},
            config: {},
        };
        mockedAxios.create.mockReturnValue({
            post: jest.fn().mockResolvedValue(mockResponse),
            get: jest.fn(),
            delete: jest.fn(),
            interceptors: { response: { use: jest.fn() } },
        });
        // Create a new agent to use the mocked axios
        const testAgent = new agent_1.AgentOS({ endpoint: 'http://localhost:18789' });
        const session = await testAgent.createSession();
        expect(session.id).toBe('test-session-id');
    });
    test('should load a skill', async () => {
        const skill = await agent.loadSkill('test-skill');
        expect(skill.name).toBe('test-skill');
    });
});
