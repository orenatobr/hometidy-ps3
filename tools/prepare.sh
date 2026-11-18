#!/bin/sh
# Prepares what the package needs from VERSION before `make pkg`: build/app_ver (the NN.NN of PARAM.SFO),
# build/sfo.xml and the package art (build/art, drawn by tools/make_art.py with the version in it).
# Usage: tools/prepare.sh [project_root]. Safe to run again: the art is redrawn only when VERSION changes.
set -eu
root=${1:-$(dirname "$0")/..}
cd "$root"
version=$(tr -d ' \n' < VERSION)
case "$version" in
    [0-9]*.[0-9].[0-9]) ;;
    *) echo "VERSION must look like 1.0.2 (minor and patch below 10, the PARAM.SFO has two digits): $version" >&2; exit 1 ;;
esac
major=${version%%.*}; rest=${version#*.}; minor=${rest%%.*}; patch=${rest#*.}
app_ver=$(printf '%02d.%02d' "$major" $((minor * 10 + patch)))
mkdir -p build
printf '%s\n' "$app_ver" > build/app_ver
sed "s/@APP_VER@/$app_ver/" pkg/sfo.xml > build/sfo.xml
if [ "$(cat build/art/.version 2>/dev/null)" != "$version" ]; then
    python3 tools/make_art.py build/art
    printf '%s\n' "$version" > build/art/.version
fi
echo "$app_ver"
