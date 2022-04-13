#!/bin/bash

export LD_LIBRARY_PATH=../dist/plugins:$LD_LIBARY_PATH

if [ -x ../dist/MultiCapture ]; then
    exec ../dist/MultiCapture -c vnc_client.json
else
    echo "exec not found: ../dist/MultiCapture"
fi
