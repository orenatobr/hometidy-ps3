#!/bin/sh
# Builds and runs the host tests (no console needed) with the address and undefined-behaviour sanitizers.
# Optional: HOMETIDY_REAL_XML=path/to/mygames.xml checks a real game list.
set -eu
cd "$(dirname "$0")/.."
OUT="${TMPDIR:-/tmp}/hometidy_ps3_tests"
${CXX:-c++} -std=c++20 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined \
    -DAPP_VERSION=\"$(cat VERSION)\" \
    -Isource -Ithird_party/stb \
    tests/test_main.cpp source/sorter.cpp source/default_rules.cpp source/mygames.cpp source/fsx.cpp \
    source/library.cpp source/sha1.cpp source/covers.cpp source/coversync.cpp source/frame.cpp source/isoinfo.cpp source/badge.cpp source/tasks.cpp source/stb_impl.cpp source/gfx.cpp source/app.cpp \
    source/texts.cpp host/platform_host.cpp \
    -o "$OUT"
"$OUT"
