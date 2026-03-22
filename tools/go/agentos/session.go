// AgentOS Go SDK Session
// Version: 1.0.0.5
// Last updated: 2026-03-21

package agentos

import (
	"context"
	"errors"
	"fmt"
)

// Session represents a session in the AgentOS system
type Session struct {
	client *Client
	ID     string
}

// NewSession creates a new Session object
func NewSession(client *Client, sessionID string) *Session {
	return &Session{
		client: client,
		ID:     sessionID,
	}
}

// SetContext sets a context value for the session
func (s *Session) SetContext(ctx context.Context, key string, value interface{}) (bool, error) {
	path := fmt.Sprintf("/api/sessions/%s/context", s.ID)
	
	data := map[string]interface{}{
		"key":   key,
		"value": value,
	}
	
	response, err := s.client.request(ctx, "POST", path, data)
	if err != nil {
		return false, err
	}
	
	success, ok := response["success"].(bool)
	if !ok {
		return false, errors.New("invalid response: missing success")
	}
	
	return success, nil
}

// GetContext gets a context value from the session
func (s *Session) GetContext(ctx context.Context, key string) (interface{}, error) {
	path := fmt.Sprintf("/api/sessions/%s/context/%s", s.ID, key)
	
	response, err := s.client.request(ctx, "GET", path, nil)
	if err != nil {
		return nil, err
	}
	
	value, ok := response["value"]
	if !ok {
		return nil, errors.New("invalid response: missing value")
	}
	
	return value, nil
}

// Close closes the session
func (s *Session) Close(ctx context.Context) (bool, error) {
	path := fmt.Sprintf("/api/sessions/%s", s.ID)
	
	response, err := s.client.request(ctx, "DELETE", path, nil)
	if err != nil {
		return false, err
	}
	
	success, ok := response["success"].(bool)
	if !ok {
		return false, errors.New("invalid response: missing success")
	}
	
	return success, nil
}
