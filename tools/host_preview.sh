#!/bin/sh
# Renders every screen of the UI on the host (software canvas, no console or SDL needed) into PNG files.
# Usage: tools/host_preview.sh [fake_console_dir] [out_dir] [width height]
set -eu
cd "$(dirname "$0")/.."
FAKE="${1:-/tmp/hometidy_fake}"; OUT="${2:-/tmp/hometidy_shots}"; shift 2 2>/dev/null || true
python3 tools/make_fake_console.py "$FAKE"
BIN="${TMPDIR:-/tmp}/hometidy_host"
${CXX:-c++} -std=c++20 -O2 -Wall -Wextra -Isource -Ithird_party/stb \
    host/main_host.cpp host/platform_host.cpp source/app.cpp source/gfx.cpp source/sorter.cpp source/default_rules.cpp \
    source/mygames.cpp source/library.cpp source/badge.cpp source/tasks.cpp source/texts.cpp source/fsx.cpp source/stb_impl.cpp -o "$BIN"
HOMETIDY_ROOT="$FAKE" HOMETIDY_SHOTS="$OUT" HOMETIDY_FONTS=data "$BIN" "$@"
echo "screens in $OUT"
