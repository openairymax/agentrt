// AgentOS Go SDK Skill
// Version: 1.0.0.5
// Last updated: 2026-03-21

package agentos

import (
	"context"
	"errors"
	"fmt"
)

// Skill represents a skill in the AgentOS system
type Skill struct {
	client *Client
	name   string
}

// NewSkill creates a new Skill object
func NewSkill(client *Client, skillName string) *Skill {
	return &Skill{
		client: client,
		name:   skillName,
	}
}

// Execute executes the skill with the given parameters
func (s *Skill) Execute(ctx context.Context, parameters map[string]interface{}) (SkillResult, error) {
	path := fmt.Sprintf("/api/skills/%s/execute", s.name)
	
	if parameters == nil {
		parameters = make(map[string]interface{})
	}
	
	data := map[string]interface{}{
		"parameters": parameters,
	}
	
	response, err := s.client.request(ctx, "POST", path, data)
	if err != nil {
		return SkillResult{}, err
	}
	
	success, ok := response["success"].(bool)
	if !ok {
		return SkillResult{}, errors.New("invalid response: missing success")
	}
	
	output, _ := response["output"]
	error, _ := response["error"].(string)
	
	return SkillResult{
		Success: success,
		Output:  output,
		Error:   error,
	}, nil
}

// GetInfo gets information about the skill
func (s *Skill) GetInfo(ctx context.Context) (SkillInfo, error) {
	path := fmt.Sprintf("/api/skills/%s", s.name)
	
	response, err := s.client.request(ctx, "GET", path, nil)
	if err != nil {
		return SkillInfo{}, err
	}
	
	description, _ := response["description"].(string)
	version, _ := response["version"].(string)
	parameters, _ := response["parameters"].(map[string]interface{})
	if parameters == nil {
		parameters = make(map[string]interface{})
	}
	
	return SkillInfo{
		Name:        s.name,
		Description: description,
		Version:     version,
		Parameters:  parameters,
	}, nil
}

// SkillResult represents the result of a skill execution
type SkillResult struct {
	Success bool        `json:"success"`
	Output  interface{} `json:"output"`
	Error   string      `json:"error"`
}

// SkillInfo represents information about a skill
type SkillInfo struct {
	Name        string                 `json:"skill_name"`
	Description string                 `json:"description"`
	Version     string                 `json:"version"`
	Parameters  map[string]interface{} `json:"parameters"`
}
