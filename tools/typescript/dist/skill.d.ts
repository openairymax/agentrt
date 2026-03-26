import { SkillInfo, SkillResult } from './types';
import { AgentOS } from './agent';
/** AgentOS 技能管理类 */
export declare class Skill {
    private client;
    private skillName;
    /** 创建新的 Skill 对象 */
    constructor(client: AgentOS, skillName: string);
    /** 获取技能名称 */
    get name(): string;
    /** 执行技能 */
    execute(parameters?: Record<string, any>): Promise<SkillResult>;
    /** 获取技能信息 */
    getInfo(): Promise<SkillInfo>;
    /** 卸载技能 */
    unload(): Promise<boolean>;
}
