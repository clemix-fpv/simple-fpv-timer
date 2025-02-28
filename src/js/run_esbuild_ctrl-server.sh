#!/bin/bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
pushd $SCRIPT_DIR
me=$(basename ${BASH_SOURCE[0]})

if command -v distrobox-enter; then
    exec distrobox-enter tumbleweed -- "$SCRIPT_DIR/$me"
    exit 0
fi


esbuild src/ctrld.ts --bundle --outfile=../server/www/ctrld.js --minify --watch --target=esnext --sourcemap

