#!/usr/bin/env bash

# Arguments: engine, architecture, repetitions, CPU, harness checkout, work directory.
set -euo pipefail
engine=$1
arch=$2
repetitions=$3
cpu=$4
bench=$5
work=$6

for ((run=1; run<=repetitions; run++)); do
  echo "::group::$arch run $run/$repetitions: WTB"
  (cd "$bench/test/web-tooling-benchmark" && taskset -c "$cpu" "$engine" dist/cli.js) \
    | tee "$work/wtb-$arch.log"
  grep -q '^Geometric mean:' "$work/wtb-$arch.log"
  echo '::endgroup::'
done
