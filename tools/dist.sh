#!/bin/sh
# Collects the release files in dist/ after `make pkg`: the installable package, the .self and checksums.
# The package that retail/CFW consoles install is the finalized one (HomeTidy.gnpdrm.pkg), shipped as HomeTidy.pkg.
set -eu
cd "$(dirname "$0")/.."
rm -rf dist
mkdir -p dist
cp HomeTidy.gnpdrm.pkg dist/HomeTidy.pkg
cp HomeTidy.self dist/HomeTidy.self
( cd dist && shasum -a 256 HomeTidy.pkg HomeTidy.self > SHA256SUMS && cat SHA256SUMS )
