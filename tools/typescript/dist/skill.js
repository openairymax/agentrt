"use strict";
// AgentOS TypeScript SDK Skill
// Version: 2.0.0
// Last updated: 2026-03-23
Object.defineProperty(exports, "__esModule", { value: true });
exports.Skill = void 0;
/** AgentOS 技能管理类 */
class Skill {
    /** 创建新的 Skill 对象 */
    constructor(client, skillName) {
        this.client = client;
        this.skillName = skillName;
    }
    /** 获取技能名称 */
    get name() {
        return this.skillName;
    }
    /** 执行技能 */
    async execute(parameters) {
        const response = await this.client.request('POST', `/api/skills/${this.skillName}/execute`, {
            parameters: parameters || {},
        });
        return {
            success: response.success,
            output: response.output,
            error: response.error,
        };
    }
    /** 获取技能信息 */
    async getInfo() {
        var _a;
        const response = await this.client.request('GET', `/api/skills/${this.skillName}`);
        return {
            skillName: this.skillName,
            skillId: response.skill_id || this.skillName,
            description: response.description || '',
            version: response.version || '',
            parameters: response.parameters || {},
            enabled: (_a = response.enabled) !== null && _a !== void 0 ? _a : true,
        };
    }
    /** 卸载技能 */
    async unload() {
        const response = await this.client.request('DELETE', `/api/skills/${this.skillName}`);
        return response.success;
    }
}
exports.Skill = Skill;
