#!/usr/bin/env bash
# Refresh Shinto overlay colors after `omarchy theme set`.
set -euo pipefail
if command -v shinto >/dev/null; then
  shinto theme
fi
