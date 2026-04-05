# AgentOS Python SDK - Mock Client Implementation
# Version: 3.0.0
# Last updated: 2026-03-24

"""
Mock client implementation for testing purposes.

Provides a mock implementation of the APIClient interface that can be used
for unit testing without requiring a real AgentOS server.

Corresponds to Go SDK: client/mock.go
"""

from typing import Any, Callable, Dict, Optional
from ..client.client import APIClient, APIResponse, RequestOptions


class MockClient(APIClient):
    """
    Mock implementation of APIClient for testing.

    Allows customization of responses through callable functions.
    Each HTTP method can be configured with a custom handler.

    Example:
        >>> mock = MockClient()
        >>> mock.get_handler = lambda path, opts: APIResponse(success=True, data={"id": "123"})
        >>> response = mock.get("/api/v1/tasks/123")
        >>> print(response.data["id"])
        123
    """

    def __init__(self):
        """
        Initialize mock client with default handlers.

        Default handlers return "not implemented" error.
        Override specific handlers to customize behavior.
        """
        self._get_handler: Optional[Callable[[str, Optional[RequestOptions]], APIResponse]] = None
        self._post_handler: Optional[Callable[[str, Any, Optional[RequestOptions]], APIResponse]] = None
        self._put_handler: Optional[Callable[[str, Any, Optional[RequestOptions]], APIResponse]] = None
        self._delete_handler: Optional[Callable[[str, Optional[RequestOptions]], APIResponse]] = None

        # Storage for tracking calls
        self.calls: Dict[str, list] = {
            "get": [],
            "post": [],
            "put": [],
            "delete": [],
        }

    def get(self, path: str, opts: Optional[RequestOptions] = None) -> APIResponse:
        """
        Mock implementation of GET request.

        Args:
            path: API path
            opts: Request options

        Returns:
            APIResponse: Configured response or default error

        Raises:
            NotImplementedError: If no handler is configured
        """
        self.calls["get"].append({"path": path, "opts": opts})
        if self._get_handler is not None:
            return self._get_handler(path, opts)
        raise NotImplementedError("Mock GET handler not configured")

    def post(self, path: str, body: Any = None, opts: Optional[RequestOptions] = None) -> APIResponse:
        """
        Mock implementation of POST request.

        Args:
            path: API path
            body: Request body
            opts: Request options

        Returns:
            APIResponse: Configured response or default error

        Raises:
            NotImplementedError: If no handler is configured
        """
        self.calls["post"].append({"path": path, "body": body, "opts": opts})
        if self._post_handler is not None:
            return self._post_handler(path, body, opts)
        raise NotImplementedError("Mock POST handler not configured")

    def put(self, path: str, body: Any = None, opts: Optional[RequestOptions] = None) -> APIResponse:
        """
        Mock implementation of PUT request.

        Args:
            path: API path
            body: Request body
            opts: Request options

        Returns:
            APIResponse: Configured response or default error

        Raises:
            NotImplementedError: If no handler is configured
        """
        self.calls["put"].append({"path": path, "body": body, "opts": opts})
        if self._put_handler is not None:
            return self._put_handler(path, body, opts)
        raise NotImplementedError("Mock PUT handler not configured")

    def delete(self, path: str, opts: Optional[RequestOptions] = None) -> APIResponse:
        """
        Mock implementation of DELETE request.

        Args:
            path: API path
            opts: Request options

        Returns:
            APIResponse: Configured response or default error

        Raises:
            NotImplementedError: If no handler is configured
        """
        self.calls["delete"].append({"path": path, "opts": opts})
        if self._delete_handler is not None:
            return self._delete_handler(path, opts)
        raise NotImplementedError("Mock DELETE handler not configured")

    def reset(self) -> None:
        """
        Reset all call tracking and handlers.

        Clears the call history and removes all configured handlers.
        """
        self.calls = {
            "get": [],
            "post": [],
            "put": [],
            "delete": [],
        }
        self._get_handler = None
        self._post_handler = None
        self._put_handler = None
        self._delete_handler = None

    @property
    def get_handler(self) -> Optional[Callable[[str, Optional[RequestOptions]], APIResponse]]:
        """Get the configured GET handler."""
        return self._get_handler

    @get_handler.setter
    def get_handler(self, handler: Callable[[str, Optional[RequestOptions]], APIResponse]) -> None:
        """Set the GET handler."""
        self._get_handler = handler

    @property
    def post_handler(self) -> Optional[Callable[[str, Any, Optional[RequestOptions]], APIResponse]]:
        """Get the configured POST handler."""
        return self._post_handler

    @post_handler.setter
    def post_handler(self, handler: Callable[[str, Any, Optional[RequestOptions]], APIResponse]) -> None:
        """Set the POST handler."""
        self._post_handler = handler

    @property
    def put_handler(self) -> Optional[Callable[[str, Any, Optional[RequestOptions]], APIResponse]]:
        """Get the configured PUT handler."""
        return self._put_handler

    @put_handler.setter
    def put_handler(self, handler: Callable[[str, Any, Optional[RequestOptions]], APIResponse]) -> None:
        """Set the PUT handler."""
        self._put_handler = handler

    @property
    def delete_handler(self) -> Optional[Callable[[str, Optional[RequestOptions]], APIResponse]]:
        """Get the configured DELETE handler."""
        return self._delete_handler

    @delete_handler.setter
    def delete_handler(self, handler: Callable[[str, Optional[RequestOptions]], APIResponse]) -> None:
        """Set the DELETE handler."""
        self._delete_handler = handler
