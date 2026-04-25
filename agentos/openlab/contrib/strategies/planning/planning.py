"""Planning strategy module."""

class PlanningStrategy:
    """Planning strategy for task decomposition."""
    def __init__(self, config=None):
        self.config = config or {}
    async def plan(self, task):
        raise NotImplementedError("PlanningStrategy.plan not yet implemented")
