#!/bin/bash
cmake -H. -Bbw_output -DESCARGOT_HOST=linux -DESCARGOT_ARCH=x64 -DESCARGOT_MODE=release -DESCARGOT_OUTPUT=bin -GNinja
ninja -Cbw_output
