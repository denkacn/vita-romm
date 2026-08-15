#!/usr/bin/env bash
set -euo pipefail

echo "== current tools =="
command -v node || true
command -v npm || true
command -v vita || true

if ! command -v npm >/dev/null 2>&1; then
  echo "== installing nodejs/npm =="
  export DEBIAN_FRONTEND=noninteractive
  apt-get update
  apt-get install -y nodejs npm
fi

if ! command -v vita >/dev/null 2>&1; then
  echo "== installing vitacompanion-cli =="
  npm install -g vitacompanion-cli
fi

echo "== final tools =="
node --version
npm --version
command -v vita
vita -h || true
