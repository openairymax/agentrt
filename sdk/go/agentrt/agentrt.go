package agentrt

import (
	"log"
	"os"
)

const (
	Version = "0.1.0"
	Author  = "SPHARX Ltd."
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
