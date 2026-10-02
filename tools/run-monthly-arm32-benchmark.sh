#!/usr/bin/env bash
set -euo pipefail

case "$(uname -m)" in
    aarch64|arm64) ;;
    *) echo 'ARM32 comparisons require the native ARM64 benchmark host' >&2; exit 1 ;;
esac

workspace="${GITHUB_WORKSPACE:-$(cd "$(dirname "$0")/.." && pwd)}"
cache="$HOME/.cache/escargot-monthly-engines/arm32"
image="${ARM32_BENCHMARK_IMAGE:-arm32v7/ubuntu:24.04}"
mkdir -p "$cache" "$workspace/monthly-engine-report-arm32"
docker pull --platform linux/arm/v7 "$image"
image_digest="$(docker image inspect "$image" --format '{{index .RepoDigests 0}}')"

# ARMv7 executes natively on this runner. Use its regular host UID for all
# builds and reports so container-owned files cannot poison later checkouts.
docker run --rm --platform linux/arm/v7 --hostname "$(hostname)" \
    -v "$workspace:/work" -v "$cache:/engine-cache" -w /work \
    -e GITHUB_WORKSPACE=/work -e HOME=/engine-cache \
    -e MONTHLY_ENGINE_CACHE=/engine-cache \
    -e BENCHMARK_ARCHITECTURE=arm32 \
    -e BENCHMARK_REPORT_DIR=/work/monthly-engine-report-arm32 \
    -e BENCHMARK_CONTAINER_IMAGE="$image_digest" \
    -e ESCARGOT_SOURCE_REVISION="${ESCARGOT_SOURCE_REVISION:-$(git -C "$workspace" rev-parse HEAD)}" \
    -e BENCHMARK_CPU="${BENCHMARK_CPU:-$(python3 -c 'import os; print(min(os.sched_getaffinity(0)))')}" \
    -e BENCHMARK_UID="$(id -u)" -e BENCHMARK_GID="$(id -g)" \
    "$image_digest" bash -euo pipefail -c '
        export DEBIAN_FRONTEND=noninteractive
        apt-get update
        apt-get install -y --no-install-recommends ca-certificates git cmake \
            ninja-build pkg-config libicu-dev gcc g++ make python3 nodejs npm \
            time util-linux
        exec setpriv --reuid="$BENCHMARK_UID" --regid="$BENCHMARK_GID" \
            --clear-groups python3 -u tools/monthly-engine-benchmark.py
    '
