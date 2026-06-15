#!/usr/bin/env bash

# fail fast
set -euxo pipefail

VERSION=$(cat VERSION)

# check that the repo is clean

if ! git diff-index --quiet HEAD --; then
    echo "Repo is not clean, aborting"
    exit 1
fi

# udpate the html and js source files

pio run -e pioarduino_esp32 -t frontend

if ! git diff-index --quiet HEAD --; then
    git commit -am "Update the web UI source files for version ${VERSION}"
fi

# The Doxygen class documentation is generated and published by the
# "Deploy docs to GitHub Pages" workflow on merge to main, not committed here.
