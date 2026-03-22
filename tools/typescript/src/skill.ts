// AgentOS TypeScript SDK Skill
// Version: 1.0.0.5
// Last updated: 2026-03-21

import { SkillInfo, SkillResult } from './types';
import { SkillError } from './errors';
import { AgentOS } from './agent';

/**
 * Skill class for managing AgentOS skills
 */
export class Skill {
  private client: AgentOS;
  private skillName: string;

  /**
   * Create a new Skill object
   * @param client AgentOS client
   * @param skillName Skill name
   */
  constructor(client: AgentOS, skillName: string) {
    this.client = client;
    this.skillName = skillName;
  }

  /**
   * Get the skill name
   * @returns Skill name
   */
  get name(): string {
    return this.skillName;
  }

  /**
   * Execute the skill with the given parameters
   * @param parameters Skill parameters
   * @returns Skill execution result
   */
  async execute(parameters?: Record<string, any>): Promise<SkillResult> {
    try {
      const response = await this.client['request']<{ success: boolean; output?: any; error?: string }>(
        'POST',
        `/api/skills/${this.skillName}/execute`,
        { parameters: parameters || {} }
      );

      return {
        success: response.success,
        output: response.output,
        error: response.error,
      };
    } catch (error) {
      throw new SkillError(`Error executing skill: ${error.message}`);
    }
  }

  /**
   * Get information about the skill
   * @returns Skill information
   */
  async getInfo(): Promise<SkillInfo> {
    try {
      const response = await this.client['request']<{ description: string; version: string; parameters?: Record<string, any> }>(
        'GET',
        `/api/skills/${this.skillName}`
      );

      return {
        skillName: this.skillName,
        description: response.description || '',
        version: response.version || '',
        parameters: response.parameters || {},
      };
    } catch (error) {
      throw new SkillError(`Error getting skill information: ${error.message}`);
    }
  }
}
