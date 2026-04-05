# AgentOS Python SDK Skill
# Version: 3.0.0
# Last updated: 2026-04-04

"""
Skill class implementation for the AgentOS Python SDK.
"""

from typing import Optional, Dict, Any

from .types import SkillInfo, SkillResult
from .exceptions import SkillError

class Skill:
    """Skill class for managing AgentOS skills.
    
    This class provides methods to execute skills and get skill information.
    """
    
    def __init__(self, client, skill_name: str):
        """
        Initialize a Skill object.
        
        Args:
            client: The AgentOS client instance.
            skill_name: The skill name.
        """
        self.client = client
        self.skill_name = skill_name
    
    def execute(self, parameters: Optional[Dict[str, Any]] = None) -> SkillResult:
        """
        Execute the skill with the given parameters.
        
        Args:
            parameters: The skill parameters.
        
        Returns:
            The skill execution result.
        
        Raises:
            SkillError: If there's an error executing the skill.
        """
        try:
            data = {"parameters": parameters or {}}
            response = self.client._request("POST", f"/api/v1/skills/{self.skill_name}/execute", data)
            return SkillResult(
                success=response.get("success", False),
                output=response.get("output"),
                error=response.get("error")
            )
        except Exception as e:
            raise SkillError(f"Error executing skill: {str(e)}")
    
    async def execute_async(self, parameters: Optional[Dict[str, Any]] = None) -> SkillResult:
        """
        Execute the skill asynchronously with the given parameters.
        
        Args:
            parameters: The skill parameters.
        
        Returns:
            The skill execution result.
        
        Raises:
            SkillError: If there's an error executing the skill.
        """
        try:
            data = {"parameters": parameters or {}}
            response = await self.client._request("POST", f"/api/v1/skills/{self.skill_name}/execute", data)
            return SkillResult(
                success=response.get("success", False),
                output=response.get("output"),
                error=response.get("error")
            )
        except Exception as e:
            raise SkillError(f"Error executing skill: {str(e)}")
    
    def get_info(self) -> SkillInfo:
        """
        Get information about the skill.
        
        Returns:
            The skill information.
        
        Raises:
            SkillError: If there's an error getting the skill information.
        """
        try:
            response = self.client._request("GET", f"/api/v1/skills/{self.skill_name}")
            return SkillInfo(
                skill_name=self.skill_name,
                description=response.get("description", ""),
                version=response.get("version", ""),
                parameters=response.get("parameters", {})
            )
        except Exception as e:
            raise SkillError(f"Error getting skill information: {str(e)}")
    
    async def get_info_async(self) -> SkillInfo:
        """
        Get information about the skill asynchronously.
        
        Returns:
            The skill information.
        
        Raises:
            SkillError: If there's an error getting the skill information.
        """
        try:
            response = await self.client._request("GET", f"/api/v1/skills/{self.skill_name}")
            return SkillInfo(
                skill_name=self.skill_name,
                description=response.get("description", ""),
                version=response.get("version", ""),
                parameters=response.get("parameters", {})
            )
        except Exception as e:
            raise SkillError(f"Error getting skill information: {str(e)}")
