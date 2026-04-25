package agentos

import (
	"log"
	"os"
)

const (
	Version = "3.0.0"
	Author  = "SpharxWorks"
	License = "MIT"
)

var defaultLogger = log.New(os.Stderr, "[AgentOS] ", log.LstdFlags|log.Lshortfile)

func GetLogger() *log.Logger {
	return defaultLogger
}

func SetLogger(l *log.Logger) {
	if l != nil {
		defaultLogger = l
	}
}
