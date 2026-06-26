#!/usr/bin/env bash
# AgentRT OpenLab: videoedit application launcher
# Usage: ./run.sh [options]
#
# Copyright (c) 2026 SPHARX. All Rights Reserved.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_NAME="videoedit"

echo "[AgentRT] Starting ${APP_NAME} application..."
echo "[AgentRT] APP_DIR: ${SCRIPT_DIR}"

for cfg_file in "app.json" "manifest.json" "config.yaml"; do
    if [[ -f "${SCRIPT_DIR}/${cfg_file}" ]]; then
        echo "[AgentRT] Found config: ${cfg_file}"
        break
    fi
done

PYTHON_CMD="${PYTHON_CMD:-python3}"
if command -v "${PYTHON_CMD}" &> /dev/null; then
    echo "[AgentRT] Runtime: ${PYTHON_CMD}"
else
    echo "[AgentRT] WARNING: Python3 runtime not found on PATH"
fi

echo "[AgentRT] ${APP_NAME} launcher initialized. Application startup deferred to orchestrator."
exit 0
