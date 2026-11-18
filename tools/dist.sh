#!/bin/sh
# Collects the release files in dist/ after `make pkg`: the installable package, the .self and checksums, named
# hometidy_ps3_v<version> after VERSION. The package that retail/CFW consoles install is the finalized one
# (HomeTidy.gnpdrm.pkg). VERSION must have a section in CHANGELOG.md: that section is the release notes.
set -eu
cd "$(dirname "$0")/.."
version=$(tr -d ' \n' < VERSION)
sh tools/release_notes.sh "$version" > /dev/null
name="hometidy_ps3_v$version"
rm -rf dist
mkdir -p dist
cp HomeTidy.gnpdrm.pkg "dist/$name.pkg"
cp HomeTidy.self "dist/$name.self"
( cd dist && shasum -a 256 "$name.pkg" "$name.self" > SHA256SUMS && cat SHA256SUMS )
