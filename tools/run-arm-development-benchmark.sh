#!/usr/bin/env bash

set -euo pipefail
case "$REPETITIONS" in 1|3|5) ;; *) exit 1 ;; esac
test "$(uname -m)" = aarch64
work=$(mktemp -d "$RUNNER_TEMP/escargot-arm-benchmark.XXXXXX")
container="escargot-arm-benchmark-${GITHUB_RUN_ID}-${GITHUB_RUN_ATTEMPT}"
cleanup() {
  docker rm -f "$container" >/dev/null 2>&1 || true
  rm -rf "$work"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
lock_dir="$HOME/.cache/escargot-locks"
install -d -m 700 "$lock_dir"
exec 9>"$lock_dir/performance-host.lock"
echo 'Waiting for exclusive access to the performance host'
flock -x -w 3600 9
printf 'Source: %s@%s\nHarness: %s\nRunner: %s\n' \
  "$SOURCE_REPOSITORY" "$SOURCE_REVISION" "$GITHUB_SHA" "$RUNNER_NAME"
lscpu
cpu=$(python3 -c 'import os; print(min(os.sched_getaffinity(0)))')
printf 'Benchmark CPU: %s; repetitions: %s; WTB minSamples: 3\n' "$cpu" "$REPETITIONS"
echo '::group::Fetch source and benchmark dependencies'
git init "$work/source"
git -C "$work/source" remote add origin "https://github.com/$SOURCE_REPOSITORY.git"
git -C "$work/source" fetch --depth 1 origin "$SOURCE_REVISION"
git -C "$work/source" checkout --detach FETCH_HEAD
test "$(git -C "$work/source" rev-parse HEAD)" = "$SOURCE_REVISION"
git -C "$work/source" submodule update --init --jobs 8 third_party/GCutil third_party/walrus
if [ -n "$BENCHMARK_FILE" ]; then
  export BENCHMARK_SOURCE="$work/source"
  python3 - <<'PY'
import os
from pathlib import Path
source = Path(os.environ['BENCHMARK_SOURCE']).resolve()
script = (source / os.environ['BENCHMARK_FILE']).resolve()
if not script.is_relative_to(source) or not script.is_file():
    raise RuntimeError('Benchmark file must be an existing file inside the source checkout')
PY
  printf 'Benchmark file: %s (from source revision)\n' "$BENCHMARK_FILE"
else
  echo 'Benchmark file: tools/arm-benchmark-suites.sh (from bench revision)'
fi
git submodule update --init --jobs 8 test/vendortest test/octane test/web-tooling-benchmark
# Reuse monthly's pinned dependency setup and three-sample WTB bundle.
python3 -u - <<'PY'
import importlib.util, sys
sys.path.insert(0, 'tools')
spec = importlib.util.spec_from_file_location('monthly', 'tools/monthly-engine-benchmark.py')
monthly = importlib.util.module_from_spec(spec)
spec.loader.exec_module(monthly)
monthly.build_wtb()
PY
echo '::endgroup::'
cat > "$work/benchmark.sh" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
bench=$1
work=$2
arch=$3
repetitions=$4
cpu=$5
options=(-DCMAKE_BUILD_TYPE=Release -DESCARGOT_DEPLOY=ON
  -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
  -DESCARGOT_THREADING=ON -DESCARGOT_TCO=ON -DESCARGOT_ENABLE_SHELL=ON)
if [ "$arch" = arm32 ]; then
  python3 -c 'import struct; assert struct.calcsize("P") == 4, "Expected native ARM32 userland"'
  options+=(-DESCARGOT_ARCH=arm -DCMAKE_SYSTEM_PROCESSOR=arm -DESCARGOT_TEMPORAL=OFF)
elif [ -d /usr/icu78-64 ]; then
  export LDFLAGS='-L/usr/icu78-64/lib -Wl,-rpath=/usr/icu78-64/lib'
  export PKG_CONFIG_PATH=/usr/icu78-64/lib/pkgconfig
  export LD_LIBRARY_PATH=/usr/icu78-64/lib
fi
echo "::group::Build $arch"
gcc --version
printf 'CMake options: %s\n' "${options[*]}"
cmake -S "$work/source" -B "$work/build-$arch" -GNinja "${options[@]}"
cmake --build "$work/build-$arch" --target escargot --parallel 8
engine="$work/build-$arch/escargot"
file "$engine"
readelf -h "$engine"
echo '::endgroup::'
script="$bench/tools/arm-benchmark-suites.sh"
if [ -n "$BENCHMARK_FILE" ]; then
  script="$work/source/$BENCHMARK_FILE"
fi
bash "$script" "$engine" "$arch" "$repetitions" "$cpu" "$bench" "$work"

SH
echo '::group::Prepare native ARM32 userland'
docker pull --platform linux/arm/v7 arm32v7/ubuntu:24.04
image=$(docker image inspect arm32v7/ubuntu:24.04 --format '{{index .RepoDigests 0}}')
printf 'ARM32 image: %s\n' "$image"
echo '::endgroup::'
docker run --rm --name "$container" --platform linux/arm/v7 \
  -v "$work:/work" -v "$GITHUB_WORKSPACE:/bench" -w /work \
  -e BENCHMARK_UID="$(id -u)" -e BENCHMARK_GID="$(id -g)" \
  -e REPETITIONS -e BENCHMARK_CPU="$cpu" -e BENCHMARK_FILE \
  "$image" bash -euo pipefail -c '
    echo "::group::Install ARM32 build dependencies"
    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    apt-get install -y --no-install-recommends ca-certificates cmake ninja-build \
      pkg-config libicu-dev gcc g++ make python3 perl file binutils util-linux
    echo "::endgroup::"
    exec setpriv --reuid="$BENCHMARK_UID" --regid="$BENCHMARK_GID" --clear-groups \
      bash /work/benchmark.sh /bench /work arm32 "$REPETITIONS" "$BENCHMARK_CPU"
  '
bash "$work/benchmark.sh" "$GITHUB_WORKSPACE" "$work" arm64 "$REPETITIONS" "$cpu"
