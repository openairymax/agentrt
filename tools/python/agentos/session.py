# AgentOS Python SDK Session
# Version: 1.0.0.5
# Last updated: 2026-03-21

"""
Session class implementation for the AgentOS Python SDK.
"""

from typing import Optional, Dict, Any

from .exceptions import SessionError

class Session:
    """Session class for managing AgentOS sessions.
    
    This class provides methods to set and get session context, and close the session.
    """
    
    def __init__(self, client, session_id: str):
        """
        Initialize a Session object.
        
        Args:
            client: The AgentOS client instance.
            session_id: The session ID.
        """
        self.client = client
        self.session_id = session_id
    
    def set_context(self, key: str, value: Any) -> bool:
        """
        Set a context value for the session.
        
        Args:
            key: The context key.
            value: The context value.
        
        Returns:
            True if the context was set successfully.
        
        Raises:
            SessionError: If there's an error setting the context.
        """
        try:
            data = {"key": key, "value": value}
            response = self.client._request("POST", f"/api/sessions/{self.session_id}/context", data)
            return response.get("success", False)
        except Exception as e:
            raise SessionError(f"Error setting session context: {str(e)}")
    
    async def set_context_async(self, key: str, value: Any) -> bool:
        """
        Set a context value for the session asynchronously.
        
        Args:
            key: The context key.
            value: The context value.
        
        Returns:
            True if the context was set successfully.
        
        Raises:
            SessionError: If there's an error setting the context.
        """
        try:
            data = {"key": key, "value": value}
            response = await self.client._request("POST", f"/api/sessions/{self.session_id}/context", data)
            return response.get("success", False)
        except Exception as e:
            raise SessionError(f"Error setting session context: {str(e)}")
    
    def get_context(self, key: str) -> Optional[Any]:
        """
        Get a context value from the session.
        
        Args:
            key: The context key.
        
        Returns:
            The context value, or None if the key doesn't exist.
        
        Raises:
            SessionError: If there's an error getting the context.
        """
        try:
            response = self.client._request("GET", f"/api/sessions/{self.session_id}/context/{key}")
            return response.get("value")
        except Exception as e:
            raise SessionError(f"Error getting session context: {str(e)}")
    
    async def get_context_async(self, key: str) -> Optional[Any]:
        """
        Get a context value from the session asynchronously.
        
        Args:
            key: The context key.
        
        Returns:
            The context value, or None if the key doesn't exist.
        
        Raises:
            SessionError: If there's an error getting the context.
        """
        try:
            response = await self.client._request("GET", f"/api/sessions/{self.session_id}/context/{key}")
            return response.get("value")
        except Exception as e:
            raise SessionError(f"Error getting session context: {str(e)}")
    
    def close(self) -> bool:
        """
        Close the session.
        
        Returns:
            True if the session was closed successfully.
        
        Raises:
            SessionError: If there's an error closing the session.
        """
        try:
            response = self.client._request("DELETE", f"/api/sessions/{self.session_id}")
            return response.get("success", False)
        except Exception as e:
            raise SessionError(f"Error closing session: {str(e)}")
    
    async def close_async(self) -> bool:
        """
        Close the session asynchronously.
        
        Returns:
            True if the session was closed successfully.
        
        Raises:
            SessionError: If there's an error closing the session.
        """
        try:
            response = await self.client._request("DELETE", f"/api/sessions/{self.session_id}")
            return response.get("success", False)
        except Exception as e:
            raise SessionError(f"Error closing session: {str(e)}")
