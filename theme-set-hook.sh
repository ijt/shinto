#!/usr/bin/env bash
# Refresh Shinto overlay colors after `omarchy theme set`.
# Both the source-tree wrapper and the packaged binary accept --theme.
set -euo pipefail
if command -v shinto >/dev/null; then
  shinto --theme
fi
