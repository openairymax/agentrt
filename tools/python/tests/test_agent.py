# AgentOS Python SDK Tests
# Version: 1.0.0.5
# Last updated: 2026-03-21

import unittest
from unittest.mock import Mock, patch
import sys
import os

# Add the parent directory to the path
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

from agentos import AgentOS, AsyncAgentOS
from agentos.exceptions import AgentOSError, NetworkError, TimeoutError

class TestAgentOS(unittest.TestCase):
    """Test AgentOS client"""

    def setUp(self):
        """Set up test fixtures"""
        self.endpoint = "http://localhost:18789"

    @patch('agentos.agent.requests.Session')
    def test_submit_task(self, mock_session):
        """Test submit_task method"""
        # Mock the response
        mock_response = Mock()
        mock_response.status_code = 200
        mock_response.json.return_value = {"task_id": "test-task-id"}
        mock_session_instance = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.post.return_value = mock_response

        # Create client
        client = AgentOS(endpoint=self.endpoint)

        # Call the method
        task = client.submit_task("Test task")

        # Assertions
        self.assertEqual(task.task_id, "test-task-id")
        mock_session_instance.post.assert_called_once()

    @patch('agentos.agent.requests.Session')
    def test_write_memory(self, mock_session):
        """Test write_memory method"""
        # Mock the response
        mock_response = Mock()
        mock_response.status_code = 200
        mock_response.json.return_value = {"memory_id": "test-memory-id"}
        mock_session_instance = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.post.return_value = mock_response

        # Create client
        client = AgentOS(endpoint=self.endpoint)

        # Call the method
        memory_id = client.write_memory("Test memory")

        # Assertions
        self.assertEqual(memory_id, "test-memory-id")
        mock_session_instance.post.assert_called_once()

    @patch('agentos.agent.requests.Session')
    def test_search_memory(self, mock_session):
        """Test search_memory method"""
        # Mock the response
        mock_response = Mock()
        mock_response.status_code = 200
        mock_response.json.return_value = {
            "memories": [
                {
                    "memory_id": "test-memory-id",
                    "content": "Test memory",
                    "created_at": "2026-03-21T00:00:00Z",
                    "metadata": {}
                }
            ]
        }
        mock_session_instance = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.get.return_value = mock_response

        # Create client
        client = AgentOS(endpoint=self.endpoint)

        # Call the method
        memories = client.search_memory("test")

        # Assertions
        self.assertEqual(len(memories), 1)
        self.assertEqual(memories[0].memory_id, "test-memory-id")
        mock_session_instance.get.assert_called_once()

    @patch('agentos.agent.requests.Session')
    def test_get_memory(self, mock_session):
        """Test get_memory method"""
        # Mock the response
        mock_response = Mock()
        mock_response.status_code = 200
        mock_response.json.return_value = {
            "memory_id": "test-memory-id",
            "content": "Test memory",
            "created_at": "2026-03-21T00:00:00Z",
            "metadata": {}
        }
        mock_session_instance = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.get.return_value = mock_response

        # Create client
        client = AgentOS(endpoint=self.endpoint)

        # Call the method
        memory = client.get_memory("test-memory-id")

        # Assertions
        self.assertEqual(memory.memory_id, "test-memory-id")
        self.assertEqual(memory.content, "Test memory")
        mock_session_instance.get.assert_called_once()

    @patch('agentos.agent.requests.Session')
    def test_delete_memory(self, mock_session):
        """Test delete_memory method"""
        # Mock the response
        mock_response = Mock()
        mock_response.status_code = 200
        mock_response.json.return_value = {"success": True}
        mock_session_instance = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.delete.return_value = mock_response

        # Create client
        client = AgentOS(endpoint=self.endpoint)

        # Call the method
        result = client.delete_memory("test-memory-id")

        # Assertions
        self.assertTrue(result)
        mock_session_instance.delete.assert_called_once()

    @patch('agentos.agent.requests.Session')
    def test_create_session(self, mock_session):
        """Test create_session method"""
        # Mock the response
        mock_response = Mock()
        mock_response.status_code = 200
        mock_response.json.return_value = {"session_id": "test-session-id"}
        mock_session_instance = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.post.return_value = mock_response

        # Create client
        client = AgentOS(endpoint=self.endpoint)

        # Call the method
        session = client.create_session()

        # Assertions
        self.assertEqual(session.session_id, "test-session-id")
        mock_session_instance.post.assert_called_once()

    def test_load_skill(self):
        """Test load_skill method"""
        # Create client
        client = AgentOS(endpoint=self.endpoint)

        # Call the method
        skill = client.load_skill("test-skill")

        # Assertions
        self.assertEqual(skill.skill_name, "test-skill")

if __name__ == '__main__':
    unittest.main()
