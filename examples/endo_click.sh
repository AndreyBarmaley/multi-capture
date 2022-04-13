#!/bin/bash

export LD_LIBRARY_PATH=../dist/plugins:$LD_LIBARY_PATH

if [ -x ../dist/MultiCapture ]; then
    exec ../dist/MultiCapture -c endo_click.json
else
    echo "exec not found: ./dist/MultiCapture"
fi
