// AgentOS Go SDK Tests
// Version: 1.0.0.5
// Last updated: 2026-03-21

package agentos

import (
	"context"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"
)

func TestNewClient(t *testing.T) {
	client, err := NewClient("")
	if err != nil {
		t.Fatalf("NewClient() error = %v, want nil", err)
	}
	if client == nil {
		t.Fatalf("NewClient() client = nil, want non-nil")
	}
}

func TestNewClientWithTimeout(t *testing.T) {
	timeout := 10 * time.Second
	client, err := NewClientWithTimeout("http://localhost:18789", timeout)
	if err != nil {
		t.Fatalf("NewClientWithTimeout() error = %v, want nil", err)
	}
	if client == nil {
		t.Fatalf("NewClientWithTimeout() client = nil, want non-nil")
	}
}

func TestSubmitTask(t *testing.T) {
	// Create a test server
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodPost {
			t.Errorf("Expected POST request, got %s", r.Method)
		}
		if r.URL.Path != "/api/tasks" {
			t.Errorf("Expected path /api/tasks, got %s", r.URL.Path)
		}
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusOK)
		w.Write([]byte(`{"task_id": "test-task-id"}`))
	}))
	defer server.Close()

	// Create client
	client, err := NewClient(server.URL)
	if err != nil {
		t.Fatalf("NewClient() error = %v, want nil", err)
	}

	// Test submit_task
	taskID, err := client.SubmitTask(context.Background(), "Test task")
	if err != nil {
		t.Fatalf("SubmitTask() error = %v, want nil", err)
	}
	if taskID != "test-task-id" {
		t.Errorf("SubmitTask() taskID = %v, want test-task-id", taskID)
	}
}

func TestWriteMemory(t *testing.T) {
	// Create a test server
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodPost {
			t.Errorf("Expected POST request, got %s", r.Method)
		}
		if r.URL.Path != "/api/memories" {
			t.Errorf("Expected path /api/memories, got %s", r.URL.Path)
		}
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusOK)
		w.Write([]byte(`{"memory_id": "test-memory-id"}`))
	}))
	defer server.Close()

	// Create client
	client, err := NewClient(server.URL)
	if err != nil {
		t.Fatalf("NewClient() error = %v, want nil", err)
	}

	// Test write_memory
	memoryID, err := client.WriteMemory(context.Background(), "Test memory", nil)
	if err != nil {
		t.Fatalf("WriteMemory() error = %v, want nil", err)
	}
	if memoryID != "test-memory-id" {
		t.Errorf("WriteMemory() memoryID = %v, want test-memory-id", memoryID)
	}
}

func TestSearchMemory(t *testing.T) {
	// Create a test server
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodGet {
			t.Errorf("Expected GET request, got %s", r.Method)
		}
		if r.URL.Path != "/api/memories/search" {
			t.Errorf("Expected path /api/memories/search, got %s", r.URL.Path)
		}
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusOK)
		w.Write([]byte(`{"memories": [{"memory_id": "test-memory-id", "content": "Test memory", "created_at": "2026-03-21T00:00:00Z", "metadata": {}}]}`))
	}))
	defer server.Close()

	// Create client
	client, err := NewClient(server.URL)
	if err != nil {
		t.Fatalf("NewClient() error = %v, want nil", err)
	}

	// Test search_memory
	memories, err := client.SearchMemory(context.Background(), "test", 5)
	if err != nil {
		t.Fatalf("SearchMemory() error = %v, want nil", err)
	}
	if len(memories) != 1 {
		t.Errorf("SearchMemory() len(memories) = %v, want 1", len(memories))
	}
	if memories[0].ID != "test-memory-id" {
		t.Errorf("SearchMemory() memories[0].ID = %v, want test-memory-id", memories[0].ID)
	}
}

func TestGetMemory(t *testing.T) {
	// Create a test server
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodGet {
			t.Errorf("Expected GET request, got %s", r.Method)
		}
		if r.URL.Path != "/api/memories/test-memory-id" {
			t.Errorf("Expected path /api/memories/test-memory-id, got %s", r.URL.Path)
		}
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusOK)
		w.Write([]byte(`{"memory_id": "test-memory-id", "content": "Test memory", "created_at": "2026-03-21T00:00:00Z", "metadata": {}}`))
	}))
	defer server.Close()

	// Create client
	client, err := NewClient(server.URL)
	if err != nil {
		t.Fatalf("NewClient() error = %v, want nil", err)
	}

	// Test get_memory
	memory, err := client.GetMemory(context.Background(), "test-memory-id")
	if err != nil {
		t.Fatalf("GetMemory() error = %v, want nil", err)
	}
	if memory.ID != "test-memory-id" {
		t.Errorf("GetMemory() memory.ID = %v, want test-memory-id", memory.ID)
	}
	if memory.Content != "Test memory" {
		t.Errorf("GetMemory() memory.Content = %v, want Test memory", memory.Content)
	}
}

func TestDeleteMemory(t *testing.T) {
	// Create a test server
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodDelete {
			t.Errorf("Expected DELETE request, got %s", r.Method)
		}
		if r.URL.Path != "/api/memories/test-memory-id" {
			t.Errorf("Expected path /api/memories/test-memory-id, got %s", r.URL.Path)
		}
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusOK)
		w.Write([]byte(`{"success": true}`))
	}))
	defer server.Close()

	// Create client
	client, err := NewClient(server.URL)
	if err != nil {
		t.Fatalf("NewClient() error = %v, want nil", err)
	}

	// Test delete_memory
	success, err := client.DeleteMemory(context.Background(), "test-memory-id")
	if err != nil {
		t.Fatalf("DeleteMemory() error = %v, want nil", err)
	}
	if !success {
		t.Errorf("DeleteMemory() success = %v, want true", success)
	}
}

func TestCreateSession(t *testing.T) {
	// Create a test server
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodPost {
			t.Errorf("Expected POST request, got %s", r.Method)
		}
		if r.URL.Path != "/api/sessions" {
			t.Errorf("Expected path /api/sessions, got %s", r.URL.Path)
		}
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusOK)
		w.Write([]byte(`{"session_id": "test-session-id"}`))
	}))
	defer server.Close()

	// Create client
	client, err := NewClient(server.URL)
	if err != nil {
		t.Fatalf("NewClient() error = %v, want nil", err)
	}

	// Test create_session
	sessionID, err := client.CreateSession(context.Background())
	if err != nil {
		t.Fatalf("CreateSession() error = %v, want nil", err)
	}
	if sessionID != "test-session-id" {
		t.Errorf("CreateSession() sessionID = %v, want test-session-id", sessionID)
	}
}

func TestLoadSkill(t *testing.T) {
	// Create client
	client, err := NewClient("")
	if err != nil {
		t.Fatalf("NewClient() error = %v, want nil", err)
	}

	// Test load_skill
	skill, err := client.LoadSkill(context.Background(), "test-skill")
	if err != nil {
		t.Fatalf("LoadSkill() error = %v, want nil", err)
	}
	if skill == nil {
		t.Fatalf("LoadSkill() skill = nil, want non-nil")
	}
}
