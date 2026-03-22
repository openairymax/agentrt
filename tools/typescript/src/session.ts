// AgentOS TypeScript SDK Session
// Version: 1.0.0.5
// Last updated: 2026-03-21

import { SessionError } from './errors';
import { AgentOS } from './agent';

/**
 * Session class for managing AgentOS sessions
 */
export class Session {
  private client: AgentOS;
  private sessionId: string;

  /**
   * Create a new Session object
   * @param client AgentOS client
   * @param sessionId Session ID
   */
  constructor(client: AgentOS, sessionId: string) {
    this.client = client;
    this.sessionId = sessionId;
  }

  /**
   * Get the session ID
   * @returns Session ID
   */
  get id(): string {
    return this.sessionId;
  }

  /**
   * Set a context value for the session
   * @param key Context key
   * @param value Context value
   * @returns True if the context was set successfully
   */
  async setContext(key: string, value: any): Promise<boolean> {
    try {
      const response = await this.client['request']<{ success: boolean }>(
        'POST',
        `/api/sessions/${this.sessionId}/context`,
        { key, value }
      );

      return response.success;
    } catch (error) {
      throw new SessionError(`Error setting session context: ${error.message}`);
    }
  }

  /**
   * Get a context value from the session
   * @param key Context key
   * @returns Context value
   */
  async getContext(key: string): Promise<any> {
    try {
      const response = await this.client['request']<{ value: any }>(
        'GET',
        `/api/sessions/${this.sessionId}/context/${key}`
      );

      return response.value;
    } catch (error) {
      throw new SessionError(`Error getting session context: ${error.message}`);
    }
  }

  /**
   * Close the session
   * @returns True if the session was closed successfully
   */
  async close(): Promise<boolean> {
    try {
      const response = await this.client['request']<{ success: boolean }>(
        'DELETE',
        `/api/sessions/${this.sessionId}`
      );

      return response.success;
    } catch (error) {
      throw new SessionError(`Error closing session: ${error.message}`);
    }
  }
}
