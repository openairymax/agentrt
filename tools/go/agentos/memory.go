// AgentOS Go SDK Memory
// Version: 1.0.0.5
// Last updated: 2026-03-21

package agentos

// Memory represents a memory in the AgentOS system
type Memory struct {
	ID        string                 `json:"memory_id"`
	Content   string                 `json:"content"`
	CreatedAt string                 `json:"created_at"`
	Metadata  map[string]interface{} `json:"metadata"`
}

// NewMemory creates a new Memory object
func NewMemory(id, content, createdAt string, metadata map[string]interface{}) Memory {
	if metadata == nil {
		metadata = make(map[string]interface{})
	}
	
	return Memory{
		ID:        id,
		Content:   content,
		CreatedAt: createdAt,
		Metadata:  metadata,
	}
}
