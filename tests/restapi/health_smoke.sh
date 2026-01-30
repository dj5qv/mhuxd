#!/usr/bin/env bash
set -euo pipefail

HOST="${HOST:-127.0.0.1}"
PORT="${PORT:-5052}"

url="http://${HOST}:${PORT}/api/v1/health"

if command -v curl >/dev/null 2>&1; then
  body=$(curl -fsS "$url")
elif command -v wget >/dev/null 2>&1; then
  body=$(wget -qO- "$url")
else
  echo "curl or wget required" >&2
  exit 1
fi

# Basic sanity checks without jq dependency
case "$body" in
  *"\"status\""*"\"ok\""*) ;;
  *) echo "Health check failed: missing status ok" >&2; exit 1;;
esac

case "$body" in
  *"\"version\""* ) ;;
  *) echo "Health check failed: missing version" >&2; exit 1;;
esac

echo "OK: $body"
