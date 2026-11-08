#!/usr/bin/env bash
# Run a command with a 32 GB virtual-memory limit and a 10-minute timeout.
# Usage: scripts/run_limited.sh <command> [args...]
set -u
MEM_KB=$((32 * 1024 * 1024))   # 32 GB
TIME_LIMIT=600                 # seconds
ulimit -v "$MEM_KB" || { echo "failed to set memory limit" >&2; exit 125; }
exec timeout --signal=KILL "$TIME_LIMIT" "$@"
