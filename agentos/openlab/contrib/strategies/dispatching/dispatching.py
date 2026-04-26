"""Dispatching strategy module."""

class DispatchingStrategy:
    """Dispatching strategy for task routing."""
    def __init__(self, config=None):
        self.config = config or {}
    async def dispatch(self, task):
        raise NotImplementedError("DispatchingStrategy.dispatch not yet implemented")
