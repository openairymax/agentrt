// AgentOS TypeScript SDK Errors
// Version: 1.0.0.5
// Last updated: 2026-03-21

/**
 * AgentOS error class
 */
export class AgentOSError extends Error {
  constructor(message: string) {
    super(message);
    this.name = 'AgentOSError';
  }
}

/**
 * Network error class
 */
export class NetworkError extends AgentOSError {
  constructor(message: string) {
    super(message);
    this.name = 'NetworkError';
  }
}

/**
 * HTTP error class
 */
export class HttpError extends AgentOSError {
  constructor(message: string, public statusCode: number) {
    super(message);
    this.name = 'HttpError';
  }
}

/**
 * Timeout error class
 */
export class TimeoutError extends AgentOSError {
  constructor(message: string) {
    super(message);
    this.name = 'TimeoutError';
  }
}

/**
 * Task error class
 */
export class TaskError extends AgentOSError {
  constructor(message: string) {
    super(message);
    this.name = 'TaskError';
  }
}

/**
 * Memory error class
 */
export class MemoryError extends AgentOSError {
  constructor(message: string) {
    super(message);
    this.name = 'MemoryError';
  }
}

/**
 * Session error class
 */
export class SessionError extends AgentOSError {
  constructor(message: string) {
    super(message);
    this.name = 'SessionError';
  }
}

/**
 * Skill error class
 */
export class SkillError extends AgentOSError {
  constructor(message: string) {
    super(message);
    this.name = 'SkillError';
  }
}
