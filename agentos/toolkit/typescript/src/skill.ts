// AgentOS TypeScript SDK Skill
// Version: 3.0.0
// Last updated: 2026-04-04

import { SkillInfo, SkillResult } from './types';
import { SkillError } from './errors';
import { AgentOS } from './agent';

/** AgentOS 技能管理类 */
export class Skill {
  private client: AgentOS;
  private skillName: string;

  /** 创建新的 Skill 对象 */
  constructor(client: AgentOS, skillName: string) {
    this.client = client;
    this.skillName = skillName;
  }

  /** 获取技能名�?*/
  get name(): string {
    return this.skillName;
  }

  /** 执行技能 */
  async execute<T = unknown>(parameters?: Record<string, unknown>): Promise<SkillResult<T>> {
    const response = await this.client.request<{
      success: boolean;
      output?: T;
      error?: string;
    }>('POST', `/api/v1/skills/${this.skillName}/execute`, {
      parameters: parameters || {},
    });

    return {
      success: response.success,
      output: response.output,
      error: response.error,
    };
  }

  /** 获取技能信息 */
  async getInfo(): Promise<SkillInfo> {
    const response = await this.client.request<{
      skill_id?: string;
      description: string;
      version: string;
      parameters?: Record<string, any>;
      enabled?: boolean;
    }>('GET', `/api/v1/skills/${this.skillName}`);

    return {
      name: this.skillName,
      description: response.description || '',
      version: response.version || '',
      parameters: response.parameters || {},
    };
  }

  /** 卸载技�?*/
  async unload(): Promise<boolean> {
    const response = await this.client.request<{ success: boolean }>(
      'DELETE',
      `/api/v1/skills/${this.skillName}`,
    );
    return response.success;
  }
}
