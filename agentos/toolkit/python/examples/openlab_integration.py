# OpenLab 与 AgentOS Python SDK 集成示例
#
# 版本: 1.0.0
# 最后更新: 2026-04-10
# 作者: AgentOS 集成团队
#
# 本示例展示如何在 OpenLab 应用中集成 AgentOS Python SDK，
# 实现以下功能：
# 1. 通过 AgentOS SDK 创建和管理任务
# 2. 使用 AgentOS 记忆系统存储和检索数据
# 3. 调用 AgentOS 技能执行特定操作
# 4. 监控任务执行状态和性能指标

import asyncio
import json
import logging
from datetime import datetime
from typing import Dict, Any, List, Optional

# 导入 AgentOS Python SDK
try:
    from agentos import AgentOSClient, Task, Memory, Skill
    from agentos.types import TaskStatus, MemoryType, SkillCategory
    from agentos.utils.event_emitter import EventEmitter, BuiltinEvents
    AGENTOS_AVAILABLE = True
except ImportError as e:
    print(f"⚠️  AgentOS SDK 未安装: {e}")
    print("请运行: pip install -e agentos/toolkit/python")
    AGENTOS_AVAILABLE = False

# 设置日志
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


class OpenLabAgentOSIntegration:
    """
    OpenLab 与 AgentOS 集成类
    
    提供 OpenLab 应用与 AgentOS 核心系统的无缝集成，
    基于五维正交系统设计原则实现。
    """
    
    def __init__(self, agentos_base_url: str = "http://localhost:8080"):
        """
        初始化集成
        
        Args:
            agentos_base_url: AgentOS 网关地址
        """
        if not AGENTOS_AVAILABLE:
            raise RuntimeError("AgentOS SDK 未安装，无法初始化集成")
        
        self.base_url = agentos_base_url
        self.client = AgentOSClient(base_url=agentos_base_url)
        self.event_emitter = EventEmitter()
        
        # 集成状态
        self.connected = False
        self.initialized = False
        
        logger.info(f"✅ OpenLab-AgentOS 集成初始化完成，连接到: {agentos_base_url}")
    
    async def connect(self) -> bool:
        """
        连接到 AgentOS 服务
        
        Returns:
            bool: 连接是否成功
        """
        try:
            # 检查服务健康状态
            health = await self.client.health.check()
            
            if health.get("status") == "healthy":
                self.connected = True
                logger.info(f"✅ 成功连接到 AgentOS 服务 (版本: {health.get('version', 'unknown')})")
                
                # 订阅系统事件
                self._setup_event_handlers()
                
                return True
            else:
                logger.error(f"❌ AgentOS 服务不健康: {health}")
                return False
                
        except Exception as e:
            logger.error(f"❌ 连接到 AgentOS 失败: {e}")
            return False
    
    def _setup_event_handlers(self):
        """设置事件处理器"""
        # 任务创建事件
        self.event_emitter.on(
            BuiltinEvents.TASK_CREATED,
            lambda event: logger.info(f"📝 任务创建: {event.data.get('task_id')}")
        )
        
        # 任务完成事件
        self.event_emitter.on(
            BuiltinEvents.TASK_COMPLETED,
            lambda event: logger.info(f"✅ 任务完成: {event.data.get('task_id')}")
        )
        
        # 内存存储事件
        self.event_emitter.on(
            BuiltinEvents.MEMORY_STORED,
            lambda event: logger.info(f"💾 记忆存储: {event.data.get('memory_id')}")
        )
        
        logger.info("✅ 事件处理器设置完成")
    
    async def create_research_task(self, 
                                  topic: str, 
                                  description: str,
                                  researcher_name: str) -> Optional[str]:
        """
        创建研究任务
        
        Args:
            topic: 研究主题
            description: 任务描述
            researcher_name: 研究员姓名
            
        Returns:
            Optional[str]: 任务ID，创建失败返回None
        """
        try:
            # 创建任务元数据
            task_metadata = {
                "type": "research",
                "topic": topic,
                "description": description,
                "researcher": researcher_name,
                "created_at": datetime.now().isoformat(),
                "priority": "medium",
                "estimated_duration_hours": 8,
                "tags": ["research", "openlab", "agentos"]
            }
            
            # 通过 AgentOS SDK 创建任务
            task = await self.client.task.create(
                name=f"研究任务: {topic}",
                description=description,
                metadata=task_metadata,
                priority="medium"
            )
            
            if task and task.id:
                logger.info(f"✅ 研究任务创建成功: {task.id}")
                
                # 触发任务创建事件
                self.event_emitter.emit(
                    BuiltinEvents.TASK_CREATED,
                    {"task_id": task.id, "topic": topic, "researcher": researcher_name}
                )
                
                return task.id
            else:
                logger.error("❌ 任务创建失败: 响应无效")
                return None
                
        except Exception as e:
            logger.error(f"❌ 创建研究任务失败: {e}")
            return None
    
    async def store_research_memory(self,
                                   task_id: str,
                                   content: str,
                                   source: str,
                                   tags: List[str]) -> Optional[str]:
        """
        存储研究记忆
        
        Args:
            task_id: 关联的任务ID
            content: 研究内容
            source: 数据来源
            tags: 标签列表
            
        Returns:
            Optional[str]: 记忆ID，存储失败返回None
        """
        try:
            # 准备记忆数据
            memory_data = {
                "task_id": task_id,
                "content": content,
                "source": source,
                "tags": tags,
                "type": "research_finding",
                "confidence": 0.85,
                "timestamp": datetime.now().isoformat()
            }
            
            # 通过 AgentOS SDK 存储记忆
            memory = await self.client.memory.store(
                content=content,
                metadata={
                    "task_id": task_id,
                    "source": source,
                    "type": "research_finding",
                    "tags": tags
                }
            )
            
            if memory and memory.id:
                logger.info(f"✅ 研究记忆存储成功: {memory.id}")
                
                # 触发记忆存储事件
                self.event_emitter.emit(
                    BuiltinEvents.MEMORY_STORED,
                    {"memory_id": memory.id, "task_id": task_id, "source": source}
                )
                
                return memory.id
            else:
                logger.error("❌ 记忆存储失败: 响应无效")
                return None
                
        except Exception as e:
            logger.error(f"❌ 存储研究记忆失败: {e}")
            return None
    
    async def execute_data_analysis_skill(self,
                                         data: Dict[str, Any],
                                         analysis_type: str = "statistical") -> Optional[Dict[str, Any]]:
        """
        执行数据分析技能
        
        Args:
            data: 待分析的数据
            analysis_type: 分析类型
            
        Returns:
            Optional[Dict[str, Any]]: 分析结果
        """
        try:
            # 准备技能参数
            skill_params = {
                "data": data,
                "analysis_type": analysis_type,
                "timestamp": datetime.now().isoformat()
            }
            
            # 通过 AgentOS SDK 执行技能
            result = await self.client.skill.execute(
                skill_name="data_analysis",
                parameters=skill_params,
                timeout=300  # 5分钟超时
            )
            
            if result:
                logger.info(f"✅ 数据分析技能执行成功")
                return result
            else:
                logger.error("❌ 技能执行失败: 无结果返回")
                return None
                
        except Exception as e:
            logger.error(f"❌ 执行数据分析技能失败: {e}")
            return None
    
    async def search_related_research(self,
                                     query: str,
                                     limit: int = 10) -> List[Dict[str, Any]]:
        """
        搜索相关研究记忆
        
        Args:
            query: 搜索查询
            limit: 返回结果数量限制
            
        Returns:
            List[Dict[str, Any]]: 相关记忆列表
        """
        try:
            # 通过 AgentOS SDK 搜索记忆
            memories = await self.client.memory.search(
                query=query,
                limit=limit,
                filters={"type": "research_finding"}
            )
            
            if memories:
                logger.info(f"✅ 找到 {len(memories)} 条相关研究记忆")
                return memories
            else:
                logger.info("ℹ️  未找到相关研究记忆")
                return []
                
        except Exception as e:
            logger.error(f"❌ 搜索研究记忆失败: {e}")
            return []
    
    async def generate_research_report(self,
                                      task_id: str,
                                      template: str = "academic") -> Optional[str]:
        """
        生成研究报告
        
        Args:
            task_id: 任务ID
            template: 报告模板
            
        Returns:
            Optional[str]: 报告内容
        """
        try:
            # 搜索任务相关记忆
            memories = await self.search_related_research(f"task_id:{task_id}")
            
            if not memories:
                logger.warning(f"⚠️  任务 {task_id} 无相关记忆，无法生成报告")
                return None
            
            # 准备报告数据
            report_data = {
                "task_id": task_id,
                "memories": memories,
                "template": template,
                "generated_at": datetime.now().isoformat(),
                "summary": f"基于 {len(memories)} 条记忆生成的研究报告"
            }
            
            # 执行报告生成技能
            report = await self.execute_data_analysis_skill(report_data, "report_generation")
            
            if report:
                logger.info(f"✅ 研究报告生成成功，长度: {len(str(report))} 字符")
                return report
            else:
                logger.error("❌ 报告生成失败")
                return None
                
        except Exception as e:
            logger.error(f"❌ 生成研究报告失败: {e}")
            return None
    
    async def monitor_task_progress(self, task_id: str) -> Dict[str, Any]:
        """
        监控任务进度
        
        Args:
            task_id: 任务ID
            
        Returns:
            Dict[str, Any]: 进度信息
        """
        try:
            # 获取任务状态
            task = await self.client.task.get(task_id)
            
            if not task:
                return {"error": "任务不存在"}
            
            # 搜索任务相关记忆
            memories = await self.search_related_research(f"task_id:{task_id}")
            
            # 构建进度报告
            progress_report = {
                "task_id": task_id,
                "status": task.status.value if hasattr(task.status, 'value') else str(task.status),
                "created_at": task.created_at if hasattr(task, 'created_at') else None,
                "updated_at": task.updated_at if hasattr(task, 'updated_at') else None,
                "memory_count": len(memories),
                "memories_preview": memories[:3] if memories else [],
                "completion_percentage": self._calculate_completion_percentage(task, memories)
            }
            
            logger.info(f"📊 任务进度: {progress_report['completion_percentage']}% 完成")
            return progress_report
            
        except Exception as e:
            logger.error(f"❌ 监控任务进度失败: {e}")
            return {"error": str(e)}
    
    def _calculate_completion_percentage(self, task, memories: List) -> float:
        """
        计算任务完成百分比
        
        Args:
            task: 任务对象
            memories: 相关记忆列表
            
        Returns:
            float: 完成百分比 (0-100)
        """
        # 简单的完成度计算逻辑
        # 实际应用中应根据业务逻辑调整
        
        if task.status == "completed":
            return 100.0
        
        # 基于记忆数量的简单估算
        base_progress = 30.0  # 基础进度
        memory_bonus = min(len(memories) * 10.0, 70.0)  # 每项记忆增加10%，最多70%
        
        return min(base_progress + memory_bonus, 95.0)  # 最大95%，直到标记完成
    
    async def close(self):
        """关闭集成连接"""
        self.connected = False
        self.initialized = False
        logger.info("🔌 OpenLab-AgentOS 集成连接已关闭")


# ============================================================================
# 示例使用代码
# ============================================================================

async def example_basic_integration():
    """
    基础集成示例
    
    演示 OpenLab 与 AgentOS 的基本集成流程
    """
    print("=" * 70)
    print("OpenLab 与 AgentOS 集成示例 - 基础流程")
    print("=" * 70)
    
    # 1. 初始化集成
    integration = OpenLabAgentOSIntegration()
    
    # 2. 连接到 AgentOS
    connected = await integration.connect()
    if not connected:
        print("❌ 连接失败，退出示例")
        return
    
    try:
        # 3. 创建研究任务
        print("\n📝 步骤1: 创建研究任务")
        task_id = await integration.create_research_task(
            topic="人工智能伦理研究",
            description="研究人工智能发展中的伦理问题及解决方案",
            researcher_name="张博士"
        )
        
        if not task_id:
            print("❌ 任务创建失败")
            return
        
        print(f"✅ 任务创建成功: {task_id}")
        
        # 4. 存储研究记忆
        print("\n💾 步骤2: 存储研究记忆")
        memory_id = await integration.store_research_memory(
            task_id=task_id,
            content="研究表明，AI透明度是伦理框架的核心要素之一。",
            source="学术论文: 'AI Ethics in Practice'",
            tags=["ethics", "transparency", "ai"]
        )
        
        if memory_id:
            print(f"✅ 记忆存储成功: {memory_id}")
        
        # 存储更多记忆
        await integration.store_research_memory(
            task_id=task_id,
            content="数据隐私保护需要技术手段与法律框架相结合。",
            source="行业报告: 'Privacy in AI Systems'",
            tags=["privacy", "regulation", "data_protection"]
        )
        
        # 5. 搜索相关研究
        print("\n🔍 步骤3: 搜索相关研究")
        related_memories = await integration.search_related_research("伦理 AI")
        print(f"✅ 找到 {len(related_memories)} 条相关记忆")
        
        # 6. 监控任务进度
        print("\n📊 步骤4: 监控任务进度")
        progress = await integration.monitor_task_progress(task_id)
        print(f"✅ 任务进度: {progress.get('completion_percentage', 0)}%")
        print(f"   状态: {progress.get('status', 'unknown')}")
        print(f"   相关记忆: {progress.get('memory_count', 0)} 条")
        
        # 7. 生成研究报告
        print("\n📄 步骤5: 生成研究报告")
        report = await integration.generate_research_report(task_id)
        
        if report:
            print(f"✅ 报告生成成功")
            print(f"   报告预览: {str(report)[:200]}...")
        else:
            print("ℹ️  报告生成跳过（需要更多数据）")
        
        # 8. 执行数据分析技能
        print("\n📈 步骤6: 执行数据分析技能")
        analysis_data = {
            "ethical_issues": ["bias", "privacy", "accountability", "transparency"],
            "solutions": ["auditing", "explainability", "regulation", "education"],
            "stakeholders": ["developers", "users", "regulators", "society"]
        }
        
        analysis_result = await integration.execute_data_analysis_skill(analysis_data)
        if analysis_result:
            print(f"✅ 数据分析完成")
            print(f"   结果类型: {type(analysis_result).__name__}")
        
    finally:
        # 9. 清理资源
        print("\n🧹 步骤7: 清理资源")
        await integration.close()
        print("✅ 集成连接已关闭")


async def example_advanced_workflow():
    """
    高级工作流示例
    
    演示复杂的研究工作流集成
    """
    print("\n" + "=" * 70)
    print("OpenLab 与 AgentOS 集成示例 - 高级工作流")
    print("=" * 70)
    
    integration = OpenLabAgentOSIntegration()
    
    if not await integration.connect():
        return
    
    try:
        # 创建多个相关任务
        tasks = []
        
        print("\n📋 创建多任务研究项目")
        
        research_topics = [
            ("AI伦理框架比较", "比较不同国家和组织的AI伦理框架"),
            ("技术实施方案", "研究AI伦理的技术实现方案"),
            ("政策影响分析", "分析AI伦理政策对产业的影响")
        ]
        
        for topic, description in research_topics:
            task_id = await integration.create_research_task(
                topic=topic,
                description=description,
                researcher_name="研究团队"
            )
            
            if task_id:
                tasks.append({"id": task_id, "topic": topic})
                print(f"  ✅ 创建任务: {topic} ({task_id})")
        
        # 模拟跨任务知识共享
        print("\n🤝 跨任务知识共享")
        
        shared_knowledge = [
            "欧盟的AI法案提出了基于风险的监管框架",
            "IEEE的伦理对齐标准强调透明度和可解释性",
            "中国的AI治理原则注重安全可控和包容共享"
        ]
        
        for knowledge in shared_knowledge:
            for task in tasks:
                await integration.store_research_memory(
                    task_id=task["id"],
                    content=knowledge,
                    source="跨领域知识库",
                    tags=["shared_knowledge", "cross_domain", "regulation"]
                )
        
        print(f"✅ 共享 {len(shared_knowledge)} 条知识到 {len(tasks)} 个任务")
        
        # 批量进度监控
        print("\n📊 批量进度监控")
        
        for task in tasks:
            progress = await integration.monitor_task_progress(task["id"])
            print(f"  📈 {task['topic']}: {progress.get('completion_percentage', 0)}% 完成")
        
    finally:
        await integration.close()


# ============================================================================
# 主程序
# ============================================================================

async def main():
    """
    主函数
    
    运行所有集成示例
    """
    print("🚀 启动 OpenLab 与 AgentOS 集成示例")
    print("基于五维正交系统设计原则")
    print()
    
    # 检查 AgentOS SDK 可用性
    if not AGENTOS_AVAILABLE:
        print("❌ AgentOS SDK 不可用")
        print("请先安装 AgentOS Python SDK:")
        print("  pip install -e agentos/toolkit/python")
        print()
        print("如果您正在开发环境中，可以模拟运行:")
        print("  python -c \"print('模拟运行: 集成功能正常')\"")
        return
    
    try:
        # 运行基础示例
        await example_basic_integration()
        
        # 运行高级示例
        await example_advanced_workflow()
        
        print("\n" + "=" * 70)
        print("🎉 所有示例执行完成")
        print("=" * 70)
        
        print("\n📚 下一步:")
        print("1. 查看 OpenLab 文档: agentos/openlab/README.md")
        print("2. 探索更多 AgentOS SDK 功能")
        print("3. 集成到您的 OpenLab 应用中")
        print("4. 贡献集成改进代码")
        
    except Exception as e:
        print(f"\n❌ 示例执行失败: {e}")
        import traceback
        traceback.print_exc()


if __name__ == "__main__":
    # 运行主程序
    asyncio.run(main())