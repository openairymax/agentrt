#!/usr/bin/env python3
"""
AgentOS 测试模板 - 集成测试

使用方法:
1. 复制此文件到 integration/ 目录
2. 重命名为 test_<module>_integration.py
3. 实现集成测试用例

Version: 2.0.0
"""

import pytest
import requests
from pathlib import Path

from tests.utils import (
    TestDataGenerator,
    TestIsolation,
    PerformanceTester,
)


@pytest.mark.integration
class TestIntegration:
    """集成测试基类"""

    @pytest.fixture(scope="class")
    def service_endpoint(self):
        """获取服务端点"""
        return "http://localhost:18789"

    @pytest.fixture(scope="class")
    def test_client(self, service_endpoint):
        """创建测试客户端"""
        class TestClient:
            def __init__(self, endpoint):
                self.endpoint = endpoint

            def get(self, path, **kwargs):
                return requests.get(f"{self.endpoint}{path}", **kwargs)

            def post(self, path, **kwargs):
                return requests.post(f"{self.endpoint}{path}", **kwargs)

        return TestClient(service_endpoint)

    def test_service_health(self, test_client):
        """测试服务健康检查"""
        response = test_client.get("/health")
        assert response.status_code == 200

    def test_end_to_end_workflow(self, test_client):
        """测试端到端工作流"""
        # 创建任务
        task_data = TestDataGenerator.generate_task_data()
        response = test_client.post("/api/tasks", json=task_data)
        assert response.status_code in [200, 201]

        # 获取任务
        task_id = response.json().get("id")
        response = test_client.get(f"/api/tasks/{task_id}")
        assert response.status_code == 200

    @pytest.mark.slow
    def test_load_scenario(self, test_client):
        """测试负载场景"""
        with PerformanceTester("load_test") as timer:
            for _ in range(100):
                response = test_client.get("/health")
                assert response.status_code == 200

        assert timer.elapsed_ms < 5000  # 应该在 5 秒内完成


if __name__ == "__main__":
    pytest.main([__file__, "-v", "-m", "integration"])
