#!/bin/bash

if [[ -z "$PROFILING_BASE" ]]; then
    echo "--ERROR: js_profiling_graph repository should be exist"
    echo "[GUIDE]"
    echo "$ git clone git@10.113.64.74:webtf/js_profiling_graph.git"
    echo "$ export PROFILING_BASE=js_profiling_graph"
    exit 0;
fi
if [[ -z "$TESTDIR_BASE" ]]; then
    echo "--ERROR: js_full_tc repository should be exist"
    echo "[GUIDE]"
    echo "$ git clone git@10.113.64.74:webtf/js_full_tc.git test/testRepo "
    echo "$ export TESTDIR_BASE=test/testRepo"
    exit 0;
fi
num=$(date +%y%m%d_%H_%M_%S)
LOG_FILE="Log_"$num".res"

function update_data() {
  for engine in v8 jsc; do
    if [[ $engine == jsc ]]; then
        build_opt=("interp" "jit" "base")
    elif [[ $engine == v8 ]]; then
        build_opt=("" "full")
    fi
    for arch in x86; do #x86 x64
        for mode in "${build_opt[@]}"; do
            target=""
            if [[ $arch == x86 ]]; then
                target=$engine"32"
            else
                target=$engine
            fi
            if [[ $mode != "" ]]; then
                target=$target"."$mode
            fi
            echo "==================================" | tee $LOG_FILE
            echo $target $1 | tee $LOG_FILE
            echo "==================================" | tee $LOG_FILE
            # SunSpider time/memory
            ./tools/measure.sh $target time | tee $LOG_FILE
            ./tools/measure.sh $target memory | tee $LOG_FILE

            # Octane time/memory
            ./tools/measure.sh $target octane time | tee $LOG_FILE
            ./tools/measure.sh $target octane memory | tee $LOG_FILE

            if [[ $mode == "" ]]; then
                target_name=$engine
            else
                target_name=$engine"."$mode
            fi
            if [[ $mode == "base" ]]; then
                mode_name="full"
            else
                mode_name=$mode
            fi
            # Insert Result Data
            python $PROFILING_BASE/parsing.py $target_name $1 $mode_name $arch sunspider test/testRepo/out/sunspider_time.res test/testRepo/out/memory.res | tee $LOG_FILE
            python $PROFILING_BASE/parsing.py $target_name $1 $mode_name $arch octane test/testRepo/out/octane_score.res test/testRepo/out/octane_mem.res | tee $LOG_FILE
        done
    done
  done
}

update_data $1
