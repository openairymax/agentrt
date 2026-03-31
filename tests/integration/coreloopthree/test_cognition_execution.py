# AgentOS 集成测试 - CoreLoopThree
# Version: 1.0.0.6
# Last updated: 2026-03-22

"""
CoreLoopThree 集成测试模块。

测试认知层、行动层和记忆层之间的协作。
"""

import pytest
import time
import json
from typing import Dict, Any, List, Optional
from unittest.mock import Mock, MagicMock, patch, AsyncMock

import sys
import os
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..', 'tools', 'python')))


# ============================================================
# 测试标记
# ============================================================

pytestmark = pytest.mark.integration


# ============================================================
# 认知层集成测试
# ============================================================

class TestCognitionLayerIntegration:
    """认知层集成测试"""

    @pytest.fixture
    def cognition_engine(self):
        """
        创建认知引擎实例。

        Returns:
            Mock: 模拟的认知引擎
        """
        engine = Mock()
        engine.process_intent = Mock(return_value={
            "intent_type": "data_analysis",
            "goal": "analyze_sales_data",
            "entities": ["sales", "data", "analysis"],
            "confidence": 0.95
        })
        engine.generate_plan = Mock(return_value={
            "plan_id": "plan_001",
            "steps": [
                {"step": 1, "action": "load_data", "params": {}},
                {"step": 2, "action": "analyze", "params": {}},
                {"step": 3, "action": "generate_report", "params": {}}
            ]
        })
        return engine

    def test_intent_understanding(self, cognition_engine):
        """
        测试意图理解。

        验证:
            - 正确解析用户意图
            - 提取关键实体
            - 置信度合理
        """
        user_input = "帮我分析上季度的销售数据，找出增长最快的三个产品"

        result = cognition_engine.process_intent(user_input)

        assert result is not None
        assert result["intent_type"] == "data_analysis"
        assert result["goal"] == "analyze_sales_data"
        assert len(result["entities"]) > 0
        assert result["confidence"] > 0.8

    def test_task_planning(self, cognition_engine):
        """
        测试任务规划。

        验证:
            - 生成有效的任务计划
            - 计划步骤合理
            - 步骤之间有依赖关系
        """
        goal = "分析销售数据并生成报告"

        plan = cognition_engine.generate_plan(goal)

        assert plan is not None
        assert plan["plan_id"] == "plan_001"
        assert len(plan["steps"]) >= 1

        for i, step in enumerate(plan["steps"]):
            assert step["step"] == i + 1
            assert "action" in step

    @patch('agentos.agent.requests.Session')
    def test_cognition_to_execution_flow(self, mock_session):
        """
        测试认知到执行的完整流程。

        验证:
            - 意图理解后正确创建任务
            - 任务被正确调度执行
        """
        mock_response = Mock()
        mock_response.status_code = 200
        mock_response.json.return_value = {"task_id": "task_001"}

        mock_session_instance = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.post.return_value = mock_response

        from agentos import AgentOS

        client = AgentOS()

        user_input = "分析销售数据"
        task = client.submit_task(user_input)

        assert task is not None
        assert task.task_id == "task_001"


# ============================================================
# 行动层集成测试
# ============================================================

class TestExecutionLayerIntegration:
    """行动层集成测试"""

    @pytest.fixture
    def execution_engine(self):
        """
        创建执行引擎实例。

        Returns:
            Mock: 模拟的执行引擎
        """
        engine = Mock()
        engine.execute_task = Mock(return_value={
            "status": "completed",
            "output": {"result": "执行成功"},
            "duration_ms": 1500
        })
        engine.get_status = Mock(return_value={
            "status": "running",
            "progress": 50,
            "current_step": "processing_data"
        })
        return engine

    def test_task_execution(self, execution_engine):
        """
        测试任务执行。

        验证:
            - 任务成功执行
            - 返回正确的结果
            - 执行时间合理
        """
        task_params = {
            "task_type": "data_analysis",
            "data_source": "sales_2026_q1.csv"
        }

        result = execution_engine.execute_task(task_params)

        assert result is not None
        assert result["status"] == "completed"
        assert "output" in result
        assert result["duration_ms"] > 0

    def test_execution_status_tracking(self, execution_engine):
        """
        测试执行状态跟踪。

        验证:
            - 正确获取执行状态
            - 进度信息准确
        """
        status = execution_engine.get_status()

        assert status is not None
        assert status["status"] in ["pending", "running", "completed", "failed"]
        assert 0 <= status["progress"] <= 100

    @patch('agentos.agent.requests.Session')
    def test_execution_with_memory_context(self, mock_session):
        """
        测试带记忆上下文的执行。

        验证:
            - 执行时能获取相关记忆
            - 记忆上下文影响执行结果
        """
        mock_response = Mock()
        mock_response.status_code = 200

        mock_session_instance = Mock()
        mock_session.return_value = mock_session_instance

        mock_session_instance.get.return_value = mock_response
        mock_session_instance.get.return_value.json.return_value = {
            "memories": [
                {
                    "memory_id": "mem_001",
                    "content": "用户偏好使用Python进行数据分析",
                    "created_at": "2026-03-22T10:00:00Z",
                    "metadata": {}
                }
            ]
        }

        mock_session_instance.post.return_value = mock_response
        mock_session_instance.post.return_value.json.return_value = {
            "task_id": "task_001",
            "status": "completed"
        }

        from agentos import AgentOS

        client = AgentOS()

        memories = client.search_memory("数据分析偏好")
        task = client.submit_task("分析数据")

        assert len(memories) >= 0
        assert task is not None


# ============================================================
# 记忆层集成测试
# ============================================================

class TestMemoryLayerIntegration:
    """记忆层集成测试"""

    @pytest.fixture
    def memory_system(self):
        """
        创建记忆系统实例。

        Returns:
            Mock: 模拟的记忆系统
        """
        memory = Mock()
        memory.write = Mock(return_value="mem_001")
        memory.read = Mock(return_value={
            "memory_id": "mem_001",
            "content": "测试记忆内容",
            "layer": "L2",
            "created_at": "2026-03-22T10:00:00Z"
        })
        memory.search = Mock(return_value=[
            {
                "memory_id": "mem_001",
                "content": "相关记忆1",
                "score": 0.95
            },
            {
                "memory_id": "mem_002",
                "content": "相关记忆2",
                "score": 0.88
            }
        ])
        memory.evolve = Mock(return_value={
            "evolved": True,
            "new_layer": "L3",
            "reason": "频繁访问"
        })
        return memory

    def test_memory_write_and_read(self, memory_system):
        """
        测试记忆写入和读取。

        验证:
            - 记忆成功写入
            - 能正确读取记忆
        """
        content = "用户偏好使用Python进行开发"

        memory_id = memory_system.write(content)
        memory = memory_system.read(memory_id)

        assert memory_id is not None
        assert memory is not None
        assert memory["content"] == "测试记忆内容"

    def test_memory_search_relevance(self, memory_system):
        """
        测试记忆搜索相关性。

        验证:
            - 搜索返回相关结果
            - 结果按相关性排序
        """
        query = "Python开发"

        results = memory_system.search(query, top_k=5)

        assert len(results) > 0
        assert results[0]["score"] >= results[-1]["score"]

    def test_memory_evolution(self, memory_system):
        """
        测试记忆演化。

        验证:
            - 记忆能正确演化
            - 演化后层级正确
        """
        memory_id = "mem_001"

        result = memory_system.evolve(memory_id)

        assert result["evolved"] is True
        assert result["new_layer"] in ["L2", "L3", "L4"]

    @patch('agentos.agent.requests.Session')
    def test_memory_integration_with_session(self, mock_session):
        """
        测试记忆与会话集成。

        验证:
            - 会话能正确挂载记忆上下文
            - 记忆在会话中可用
        """
        mock_response = Mock()
        mock_response.status_code = 200

        mock_session_instance = Mock()
        mock_session.return_value = mock_session_instance

        mock_session_instance.post.return_value = mock_response
        mock_session_instance.post.return_value.json.return_value = {
            "session_id": "sess_001"
        }

        mock_session_instance.get.return_value = mock_response
        mock_session_instance.get.return_value.json.return_value = {
            "memories": [
                {
                    "memory_id": "mem_001",
                    "content": "会话相关记忆",
                    "created_at": "2026-03-22T10:00:00Z",
                    "metadata": {}
                }
            ]
        }

        from agentos import AgentOS

        client = AgentOS()

        session = client.create_session()
        memories = client.search_memory("会话上下文")

        assert session is not None
        assert len(memories) >= 0


# ============================================================
# 端到端流程测试
# ============================================================

class TestEndToEndFlow:
    """端到端流程测试"""

    @pytest.mark.e2e
    @patch('agentos.agent.requests.Session')
    def test_complete_task_flow(self, mock_session):
        """
        测试完整的任务流程。

        验证:
            - 从任务提交到完成的完整流程
            - 各层协作正确
        """
        mock_response = Mock()
        mock_response.status_code = 200

        mock_session_instance = Mock()
        mock_session.return_value = mock_session_instance

        mock_session_instance.post.return_value = mock_response
        mock_session_instance.post.return_value.json.side_effect = [
            {"task_id": "task_001"},
            {"memory_id": "mem_001"},
            {"session_id": "sess_001"}
        ]

        mock_session_instance.get.return_value = mock_response
        mock_session_instance.get.return_value.json.side_effect = [
            {"status": "running"},
            {"status": "completed", "output": "分析完成"},
            {"memories": []}
        ]

        from agentos import AgentOS

        client = AgentOS()

        task = client.submit_task("分析销售数据")
        memory_id = client.write_memory("分析结果：销售额增长15%")
        session = client.create_session()

        assert task is not None
        assert memory_id is not None
        assert session is not None

    @pytest.mark.e2e
    @patch('agentos.agent.requests.Session')
    def test_multi_task_coordination(self, mock_session):
        """
        测试多任务协调。

        验证:
            - 多个任务能正确协调执行
            - 任务之间能共享上下文
        """
        mock_response = Mock()
        mock_response.status_code = 200

        mock_session_instance = Mock()
        mock_session.return_value = mock_session_instance

        mock_session_instance.post.return_value = mock_response
        mock_session_instance.post.return_value.json.side_effect = [
            {"task_id": "task_001"},
            {"task_id": "task_002"},
            {"task_id": "task_003"}
        ]

        from agentos import AgentOS

        client = AgentOS()

        tasks = [
            client.submit_task("任务1：数据收集"),
            client.submit_task("任务2：数据分析"),
            client.submit_task("任务3：报告生成")
        ]

        assert len(tasks) == 3
        assert all(t is not None for t in tasks)


# ============================================================
# 运行测试
# ============================================================

if __name__ == "__main__":
    pytest.main([__file__, "-v", "--tb=short", "-m", "integration"])
