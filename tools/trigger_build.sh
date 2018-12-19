#!/bin/bash
cmake -H. -Bbw_output -DESCARGOT_HOST=linux -DESCARGOT_ARCH=x64 -DESCARGOT_MODE=release -DESCARGOT_OUTPUT=bin
make -s -Cbw_output -j2
