#!/usr/bin/env python3
"""Compare native ARM JavaScript engines on Web Tooling, Octane and SunSpider."""

import datetime
import argparse
import base64
from engine_memory_benchmark import METHOD, aggregate, measure_memory
import platform
import socket
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import statistics
import struct
import subprocess
import sys
import tarfile
import time
import urllib.request
import urllib.parse
import zipfile


ROOT = Path(__file__).resolve().parent.parent
WORK = Path(os.environ["GITHUB_WORKSPACE"])
ARCHITECTURE = os.environ.get("BENCHMARK_ARCHITECTURE", "arm64")
if ARCHITECTURE not in ("arm64", "arm32"):
    raise ValueError("BENCHMARK_ARCHITECTURE must be arm64 or arm32")
REPORT = Path(os.environ.get("BENCHMARK_REPORT_DIR", WORK / "monthly-engine-report"))
CACHE = Path(os.environ.get("MONTHLY_ENGINE_CACHE", Path.home() / ".cache" / "escargot-monthly-engines"))
JOBS = 8
CPU = int(os.environ.get("BENCHMARK_CPU", min(os.sched_getaffinity(0))))
if CPU not in os.sched_getaffinity(0):
    raise ValueError("BENCHMARK_CPU is outside the allowed CPU affinity")
LIBRARY_TRIPLET = "arm-linux-gnueabihf" if ARCHITECTURE == "arm32" else "aarch64-linux-gnu"
SYSTEM_LIBRARY_PATH = f"/lib/{LIBRARY_TRIPLET}:/usr/lib/{LIBRARY_TRIPLET}:/lib:/usr/lib"


def run(*args, cwd=None, env=None):
    print("+", " ".join(map(str, args)), flush=True)
    subprocess.run(args, cwd=cwd, env=env, check=True)


def fetch(url, path, headers=None, expected_sha256=None):
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and (not expected_sha256 or sha256(path) == expected_sha256):
        return
    temporary = path.with_name(path.name + ".tmp")
    request = urllib.request.Request(url, headers=headers or {})
    print(f"Downloading {url}", flush=True)
    with urllib.request.urlopen(request, timeout=60) as response, temporary.open("wb") as output:
        shutil.copyfileobj(response, output)
    if expected_sha256 and sha256(temporary) != expected_sha256:
        temporary.unlink()
        raise RuntimeError(f"SHA-256 mismatch: {url}")
    temporary.replace(path)


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def read_url(url):
    with urllib.request.urlopen(url, timeout=60) as response:
        return response.read()


def install_glibc():
    if ARCHITECTURE == "arm32":
        # All four engines use the same Ubuntu ARM32 userspace. The official
        # d8 archive requires glibc 2.27 or newer, available in Ubuntu 24.04.
        loader = Path("/lib/ld-linux-armhf.so.3")
        runtime_dir = Path("/lib/arm-linux-gnueabihf")
        version = subprocess.check_output([loader, "--version"], text=True).splitlines()[0]
        return loader, runtime_dir, {"version": version, "sha256": sha256(loader),
                                    "libc_sha256": sha256(runtime_dir / "libc.so.6"),
                                    "source": "Ubuntu 24.04 ARM32 container"}
    formula = json.loads(read_url("https://formulae.brew.sh/api/formula/glibc.json"))
    version = formula["versions"]["stable"]
    bottle = formula["bottle"]["stable"]["files"]["arm64_linux"]
    digest = bottle["sha256"]
    archive = CACHE / "downloads" / f"glibc-{version}-{digest}.tar.gz"
    token = json.loads(read_url("https://ghcr.io/token?scope=repository:homebrew/core/glibc:pull"))["token"]
    fetch(bottle["url"], archive, {"Authorization": f"Bearer {token}"}, digest)
    with tarfile.open(archive, "r:gz") as package:
        loader_member = next(member for member in package.getmembers()
                             if member.name.endswith("/lib/ld-linux-aarch64.so.1") and member.isfile())
        library_dir = CACHE / Path(loader_member.name).parent
        marker = library_dir / f".extracted-{digest}"
        if not marker.is_file():
            prefix = str(Path(loader_member.name).parent) + "/"
            for member in package.getmembers():
                if not member.name.startswith(prefix) or not member.isfile():
                    continue
                destination = CACHE / member.name
                destination.parent.mkdir(parents=True, exist_ok=True)
                with package.extractfile(member) as source, destination.open("wb") as output:
                    shutil.copyfileobj(source, output)
                destination.chmod(member.mode)
            marker.touch()
    loader = library_dir / "ld-linux-aarch64.so.1"
    run(loader, "--version")
    return loader, library_dir, {"version": version, "sha256": digest, "source": bottle["url"]}


def install_d8(loader, runtime_dir):
    formula = json.loads(read_url("https://formulae.brew.sh/api/formula/v8.json"))
    version = formula["versions"]["stable"]
    if ARCHITECTURE == "arm32":
        milestone = ".".join(version.split(".")[:2])
        name = f"official/{milestone}/v8-linux-arm32-rel-{version}.zip"
        metadata_url = "https://storage.googleapis.com/storage/v1/b/chromium-v8/o/" + urllib.parse.quote(name, safe="")
        metadata = json.loads(read_url(metadata_url))
        url = "https://storage.googleapis.com/chromium-v8/" + name + "?generation=" + metadata["generation"]
        archive = CACHE / "downloads" / f"v8-arm32-{version}.zip"
        fetch(url, archive)
        digest = sha256(archive)
        if base64.b64encode(hashlib.md5(archive.read_bytes()).digest()).decode() != metadata["md5Hash"]:
            archive.unlink()
            raise RuntimeError("Official ARM32 V8 archive checksum mismatch")
        directory = CACHE / f"v8-arm32-{version}-{digest}"
        directory.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(archive) as package:
            config = json.loads(package.read("v8_build_config.json"))
            if config["target_cpu"] != "arm" or config["simulator_run"] or config["debug_code"]:
                raise RuntimeError("Expected a native ARM32 V8 Release build")
            for filename in ("d8", "icudtl.dat", "snapshot_blob.bin", "v8_build_config.json"):
                (directory / filename).write_bytes(package.read(filename))
        executable = directory / "d8"
        executable.chmod(0o755)
        command = [loader, "--library-path", f"{runtime_dir}:{SYSTEM_LIBRARY_PATH}", executable]
        run(*command, "--version")
        return command, {"version": version, "sha256": digest, "source": url,
                         "build_config": config, "binary_sha256": sha256(executable)}
    bottle = formula["bottle"]["stable"]["files"]["arm64_linux"]
    digest = bottle["sha256"]
    archive = CACHE / "downloads" / f"v8-{version}-{digest}.tar.gz"
    token = json.loads(read_url("https://ghcr.io/token?scope=repository:homebrew/core/v8:pull"))["token"]
    fetch(bottle["url"], archive, {"Authorization": f"Bearer {token}"}, digest)
    with tarfile.open(archive, "r:gz") as package:
        executable_member = next(member for member in package.getmembers()
                                 if member.name.endswith("/libexec/d8") and member.isfile())
        executable = CACHE / executable_member.name
        marker = executable.parent.parent / f".extracted-{digest}"
        if not marker.is_file():
            prefix = str(Path(executable_member.name).parent) + "/"
            for member in package.getmembers():
                if not member.name.startswith(prefix) or not member.isfile():
                    continue
                destination = CACHE / member.name
                destination.parent.mkdir(parents=True, exist_ok=True)
                with package.extractfile(member) as source, destination.open("wb") as output:
                    shutil.copyfileobj(source, output)
                destination.chmod(member.mode)
            marker.touch()
    if not executable.is_file():
        raise RuntimeError("d8 was absent from the V8 bottle")
    # Use a private matching glibc loader and libraries. The host's older
    # glibc cannot launch current Homebrew bottles; no host install is changed.
    command = [loader, "--library-path", f"{runtime_dir}:{executable.parent}:{SYSTEM_LIBRARY_PATH}", executable]
    run(*command, "--version")
    return command, {"version": version, "sha256": digest, "source": bottle["url"]}


def install_qjs():
    home = read_url("https://bellard.org/quickjs/").decode("utf-8")
    releases = re.findall(r'quickjs-(\d{4}-\d{2}-\d{2})\.tar\.xz', home)
    if not releases:
        raise RuntimeError("No QuickJS source release found on bellard.org")
    version = max(releases)
    url = f"https://bellard.org/quickjs/quickjs-{version}.tar.xz"
    archive = CACHE / "downloads" / f"quickjs-{version}.tar.xz"
    fetch(url, archive)
    digest = sha256(archive)
    source_dir = CACHE / f"quickjs-{version}"
    executable = source_dir / "qjs"
    if not executable.is_file():
        with tarfile.open(archive, "r:xz") as package:
            prefix = f"quickjs-{version}/"
            for member in package.getmembers():
                if not member.name.startswith(prefix) or not member.isfile():
                    continue
                destination = CACHE / member.name
                destination.parent.mkdir(parents=True, exist_ok=True)
                with package.extractfile(member) as source, destination.open("wb") as output:
                    shutil.copyfileobj(source, output)
                destination.chmod(member.mode)
        run("make", f"-j{JOBS}", "qjs", cwd=source_dir)
    return executable, {"version": version, "sha256": digest, "source": url}


def build_escargot():
    build = WORK / "out" / f"monthly-engine-release-{ARCHITECTURE}"
    options = [
        "-DCMAKE_BUILD_TYPE=Release", "-DESCARGOT_DEPLOY=ON",
        "-DESCARGOT_THREADING=ON", "-DESCARGOT_TCO=ON",
        "-DESCARGOT_ENABLE_SHELL=ON",
    ]
    if ARCHITECTURE == "arm32":
        options += ["-DESCARGOT_ARCH=arm", "-DCMAKE_SYSTEM_PROCESSOR=arm",
                    "-DESCARGOT_TEMPORAL=OFF"]
    env = os.environ.copy()
    icu = Path("/usr/icu78-64")
    if ARCHITECTURE == "arm64" and icu.is_dir():
        env["LDFLAGS"] = f"-L{icu}/lib -Wl,-rpath={icu}/lib"
        env["PKG_CONFIG_PATH"] = f"{icu}/lib/pkgconfig"
        env["LD_LIBRARY_PATH"] = f"{icu}/lib"
    run("cmake", "-S", str(ROOT), "-B", str(build), "-GNinja", *options, env=env)
    run("ninja", "-C", str(build), f"-j{JOBS}", "escargot", env=env)
    return build / "escargot", env


def build_wtb():
    directory = ROOT / "test" / "web-tooling-benchmark"
    suite = directory / "src" / "suite.js"
    text = suite.read_text()
    changed = text.replace("minSamples: 20", "minSamples: 3")
    if changed == text and "minSamples: 3" not in text:
        raise RuntimeError("WTB minSamples setting changed upstream")
    suite.write_text(changed)
    # npm 6 resolves this GitHub dependency through git://, which is blocked on
    # many runners. Fetch the same pinned commit as an HTTPS tarball instead.
    revision = "ce9ae1a99f04d87ae78d9066efcede7fe5091a9b"
    url = f"https://codeload.github.com/bmeurer/raw-loader/tar.gz/{revision}"
    package_file = directory / "package.json"
    lock_file = directory / "package-lock.json"
    package = json.loads(package_file.read_text())
    lock = json.loads(lock_file.read_text())
    if lock["dependencies"]["raw-loader"]["version"].split("#")[-1] not in (revision, url):
        raise RuntimeError("WTB raw-loader revision changed upstream")
    package["devDependencies"]["raw-loader"] = url
    lock["dependencies"]["raw-loader"].update({"version": url, "from": url, "resolved": url})
    package_file.write_text(json.dumps(package, indent=2) + "\n")
    lock_file.write_text(json.dumps(lock, indent=2) + "\n")
    env = os.environ.copy()
    env["NODE_OPTIONS"] = "--openssl-legacy-provider"
    run("npx", "--yes", "npm@6", "install", "--no-audit", "--no-fund",
        cwd=directory, env=env)
    memory_entry = directory / "src/memory.js"
    memory_entry.write_text("const getTarget = require('./cli-flags-helper').getTarget;\n" +
                            "const tests = getTarget().map(name => require('./' + name + '-benchmark'));\n" +
                            "benchmarkCheckpoint('loaded');\nconst counts = {};\n" +
                            "for (const test of tests) {\n" +
                            "  for (let i = 0; i < 3; i++) { test.fn(); counts[test.name] = (counts[test.name] || 0) + 1; }\n" +
                            "  benchmarkCheckpoint('test:' + test.name);\n}\n" +
                            "print('MEM_WORK_JSON ' + JSON.stringify(counts));\n")
    # Reuse the score bundle's exact dependency and virtual-filesystem aliases.
    config = directory / "memory-webpack.config.js"
    config.write_text("const configs = require('./webpack.config')();\n" +
                      "configs[0].entry = './memory.js'; configs[0].output.filename = 'memory.js';\n" +
                      "module.exports = [configs[0]];\n")
    run("node", "node_modules/webpack/bin/webpack.js", "--config", str(config), cwd=directory, env=env)
    return directory


def measure(name, command, directory, env, script="dist/cli.js", kind="web_tooling"):
    log = REPORT / f"{name}.log"
    rss = []
    interval = 0.005 if kind == "sunspider" else 0.05
    peak_file = REPORT / f"{name}.peak-rss-kib.txt"
    started = time.monotonic()
    next_report = started + 30
    print(f"Starting {name} {kind} on CPU {CPU}", flush=True)
    with log.open("w") as output:
        process = subprocess.Popen(["taskset", "-c", str(CPU), "/usr/bin/time", "-f", "%M", "-o", str(peak_file),
                                    *map(str, command), str(script)], cwd=directory,
                                   env=env, stdout=output, stderr=subprocess.STDOUT)
        previous_size = 0
        affinities = set()
        while process.poll() is None:
            try:
                children = Path(f"/proc/{process.pid}/task/{process.pid}/children").read_text().split()
                if not children:
                    time.sleep(interval)
                    continue
                status = Path(f"/proc/{children[0]}/status").read_text()
                match = re.search(r"^VmRSS:\s+(\d+) kB", status, re.MULTILINE)
                if match:
                    rss.append(int(match.group(1)))
                affinity = re.search(r"^Cpus_allowed_list:\s+(.*)$", status, re.MULTILINE)
                if affinity:
                    affinities.add(affinity.group(1))
            except (FileNotFoundError, ProcessLookupError):
                pass
            now = time.monotonic()
            if now >= next_report:
                current_size = log.stat().st_size
                if current_size != previous_size:
                    with log.open(errors="replace") as source:
                        source.seek(previous_size)
                        print(source.read(), end="", flush=True)
                    previous_size = current_size
                print(f"{name}: {(now - started) / 60:.1f} min elapsed", flush=True)
                next_report = now + 30
            time.sleep(interval)
        code = process.wait()
    contents = log.read_text(errors="replace")
    print(contents[-2000:], flush=True)
    if code:
        raise RuntimeError(f"{name} exited with status {code}; see {log.name}")
    rss_file = REPORT / f"{name}.rss-kib.txt"
    rss_file.write_text("".join(f"{sample}\n" for sample in rss))
    if affinities != {str(CPU)}:
        raise RuntimeError(f"Unexpected measured CPU affinity: {affinities}")
    result = {"average_rss_kib": round(statistics.mean(rss)) if rss else None,
              "peak_rss_kib": int(peak_file.read_text().strip()),
              "sampled_peak_rss_kib": max(rss) if rss else None,
              "peak_rss_method": "GNU time ru_maxrss",
              "duration_seconds": round(time.monotonic() - started, 3),
              "cpu_affinity": sorted(affinities), "log": log.name,
              "rss_sample_count": len(rss), "rss_samples_file": rss_file.name,
              "rss_sampling_interval_ms": int(interval * 1000)}
    if kind == "sunspider":
        match = re.search(r"SUNSPIDER_JSON (.*)", contents)
        if not match:
            raise RuntimeError("Missing SunSpider results")
        scores = json.loads(match.group(1))
        if len(scores) != 26 or any(value < 0 for value in scores.values()):
            raise RuntimeError("Incomplete SunSpider results")
        result.update(total_milliseconds=sum(scores.values()), subtests_milliseconds=scores)
    elif kind in ("octane", "octane_fixed_work"):
        match = re.search(r"Score \(version [^)]+\):\s*([0-9.]+)", contents)
        if not match:
            raise RuntimeError(f"Missing Octane score; see {log.name}")
        if kind == "octane":
            result["score"] = float(match.group(1))
            result["subtests_scores"] = {m.group(1): float(m.group(2)) for m in re.finditer(
                r"^([A-Za-z][A-Za-z0-9-]*): ([0-9.]+)$", contents, re.MULTILINE)}
    else:
        score = re.search(r"Geometric mean:\s*([0-9.]+)\s+runs/s", contents)
        subtests = {m.group(1): float(m.group(2)) for m in re.finditer(
            r"^\s*([a-z][a-z0-9-]*):\s*([0-9.]+)\s+runs/s$", contents, re.MULTILINE)}
        if not score or len(subtests) != 18 or not rss:
            raise RuntimeError(f"Missing WTB score, subtests, or RSS; see {log.name}")
        result.update(score_runs_per_second=float(score.group(1)),
                      subtests_runs_per_second=subtests)
    return result


def build_sunspider():
    directory = ROOT / "test/vendortest/SunSpider/tests/sunspider-1.0.2"
    tests = directory.joinpath("LIST").read_text().splitlines()
    # Use the same Date timer and timed load as the upstream driver. Each
    # repetition gets a fresh process; data-file loading and GC are untimed.
    entries = []
    for name in tests:
        data = directory / f"{name}-data.js"
        entries.append({"name": name, "script": str(directory / f"{name}.js"),
                        "data": str(data) if data.is_file() else None})
    script = REPORT / "sunspider-driver.js"
    script.write_text("(function() {\nvar entries = " + json.dumps(entries) + ";\n" +
                      "var result = {};\nfor (var i = 0; i < entries.length; i++) {\n" +
                      "var test = entries[i]; if (test.data) load(test.data);\n" +
                      "var start = new Date(); load(test.script);\n" +
                      "result[test.name] = new Date() - start; gc();\n}\n" +
                      "print('SUNSPIDER_JSON ' + JSON.stringify(result));\n})();\n")
    return script


def build_memory_drivers(wtb):
    sunspider_dir = ROOT / "test/vendortest/SunSpider/tests/sunspider-1.0.2"
    entries = []
    for name in sunspider_dir.joinpath("LIST").read_text().splitlines():
        data = sunspider_dir / f"{name}-data.js"
        entries.append({"name": name, "script": str(sunspider_dir / f"{name}.js"),
                        "data": str(data) if data.is_file() else None})
    begin = "benchmarkCheckpoint('baseline');\n"
    end = "benchmarkCheckpoint('workload_end');\ngc(); gc();\nbenchmarkCheckpoint('after_gc');\n"
    sunspider = REPORT / "sunspider-memory.js"
    sunspider.write_text(begin + "(function() {\nvar entries = " + json.dumps(entries) + ";\n" +
                        "benchmarkCheckpoint('loaded'); var counts = {};\n" +
                        "for (var i = 0; i < entries.length; i++) { var test = entries[i];\n" +
                        " if (test.data) load(test.data); load(test.script); counts[test.name] = 1;\n" +
                        " benchmarkCheckpoint('test:' + test.name);\n}\n" +
                        "print('MEM_WORK_JSON ' + JSON.stringify(counts));\n})();\n" + end)
    octane = ROOT / "test/octane"
    shutil.copyfile(ROOT / "tools/test/octane/base_mem.js", octane / "base_mem.js")
    loads = re.findall(r"load\(base_dir \+ '([^']+)'\);", (octane / "run.js").read_text())
    if len(loads) != 20 or loads[0] != "base.js":
        raise RuntimeError(f"Octane script manifest changed: {loads}")
    driver = REPORT / "octane-memory.js"
    driver.write_text(begin + "\n".join("load(" + json.dumps(
        "base_mem.js" if name == "base.js" else name) + ");" for name in loads) + "\n" +
        "benchmarkCheckpoint('loaded');\nvar memoryCounts = {};\n" +
        "var memoryRun = BenchmarkSuite.prototype.RunSingleBenchmark;\n" +
        "BenchmarkSuite.prototype.RunSingleBenchmark = function(benchmark, data) {\n" +
        " memoryCounts[benchmark.name] = (memoryCounts[benchmark.name] || 0) + 1;\n" +
        " return memoryRun.call(this, benchmark, data);\n};\n" +
        "BenchmarkSuite.RunSuites({\n" +
        " NotifyResult: function(name) { benchmarkCheckpoint('test:' + name); },\n" +
        " NotifyError: function(name, error) { throw Error(name + ': ' + error); },\n" +
        " NotifyScore: function() { print('MEM_WORK_JSON ' + JSON.stringify(memoryCounts)); }\n" +
        "});\n" + end)
    web = REPORT / "web-tooling-memory.js"
    web.write_text(begin + "load('dist/memory.js');\n" + end)
    return {"sunspider": (ROOT, sunspider, 26, 1),
            "octane": (octane, driver, 18, 1),
            "web_tooling": (wtb, web, 18, 3)}


def run_memory_comparison(data, engines, drivers):
    allowed = sorted(os.sched_getaffinity(0))
    monitor_cpu = next((cpu for cpu in allowed if cpu != CPU), None)
    if monitor_cpu is None:
        raise RuntimeError("Memory measurements require a separate monitor CPU")
    data["memory_method"] = METHOD
    data["memory_protocol"] = {
        "repetitions": 3, "monitor_cpu": monitor_cpu, "sample_interval_ms": 10,
        "average_method": "time-weighted trapezoidal integration of smaps_rollup; checkpoint waits excluded",
        "average_window": "baseline to workload_end, including code/data loading and natural GC",
        "uss_definition": "Private_Clean + Private_Dirty; no explicit huge pages",
        "gc_policy": "natural GC during work; two explicit gc calls only for the separate after_gc checkpoint",
        "peak_method": "whole-process lifetime GNU time ru_maxrss; sampled smaps peaks also retained"}
    failures = []
    signatures = {}
    # Rotate engine order between repeats to reduce systematic order effects.
    for repetition in range(3):
        rotated = engines[repetition:] + engines[:repetition]
        for name, command, env in rotated:
            engine = data["engines"][name]
            engine["command"] = list(map(str, command))
            memory = engine.setdefault("memory", {})
            for kind, (directory, script, count, iterations) in drivers.items():
                entry = memory.setdefault(kind, {"samples": []})
                try:
                    sample = measure_memory(f"{name}-{kind}-memory-{repetition + 1}", name,
                                            command, directory, env, script, REPORT, CPU, monitor_cpu)
                    work = sample["work_counts"]
                    if (count is not None and len(work) != count) or any(value != iterations for value in work.values()):
                        raise RuntimeError(f"Incorrect fixed-work counts: {work}")
                    if kind in signatures and signatures[kind] != sample["work_signature"]:
                        raise RuntimeError("Workload counts differ between engines or repetitions")
                    signatures[kind] = sample["work_signature"]
                    entry["samples"].append(sample)
                    memory[kind] = aggregate(entry["samples"])
                except Exception as error:
                    entry["error"] = str(error)
                    failures.append(f"{name}/{kind}/{repetition + 1}")
                    print(f"Memory run failed: {failures[-1]}: {error}", file=sys.stderr, flush=True)
                (REPORT / "measurements.json").write_text(json.dumps(data, indent=2) + "\n")
    data["memory_validated"] = not failures
    (REPORT / "measurements.json").write_text(json.dumps(data, indent=2) + "\n")
    rows = [f"# {ARCHITECTURE.upper()} fixed-work memory comparison", "", data["measured_date_kst"], "",
            "Median of three fresh processes; MiB. Average RSS / kernel peak RSS.", "",
            "| Engine | SunSpider | Octane | Web Tooling |", "|---|---:|---:|---:|"]
    for name, _, _ in engines:
        cells = []
        for kind in drivers:
            result = data["engines"][name]["memory"][kind]
            cells.append("failed" if "error" in result else
                         f"{result['average_rss_kib'] / 1024:.1f} / {result['peak_rss_kib'] / 1024:.1f}")
        rows.append("| " + " | ".join([name] + cells) + " |")
    rows.extend(["", "All suites use fixed work and natural GC. Loading is included; checkpoint waits are excluded.",
                 "Raw data includes timestamped RSS/PSS/USS samples, exact phase snapshots, per-run peaks and ranges.", ""])
    summary = "\n".join(rows)
    with (REPORT / "summary.md").open("a") as output:
        output.write("\n" + summary)
    print(summary, flush=True)
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(os.environ["GITHUB_STEP_SUMMARY"], "a") as output:
            output.write(summary)
    if failures:
        raise RuntimeError("Incomplete memory comparison: " + ", ".join(failures))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--scores-only", action="store_true")
    parser.add_argument("--memory-only", action="store_true")
    parser.add_argument("--scores-json", type=Path)
    args = parser.parse_args()
    if args.scores_only and args.memory_only:
        parser.error("--scores-only and --memory-only cannot be combined")
    if os.uname().machine not in ("aarch64", "arm64", "armv7l", "armv8l"):
        raise RuntimeError("This benchmark must run on native ARM hardware")
    pointer_bits = struct.calcsize("P") * 8
    if pointer_bits != (32 if ARCHITECTURE == "arm32" else 64):
        raise RuntimeError("Python userspace does not match the requested architecture")
    # A binfmt/QEMU process exposes the emulator executable here. Refuse it
    # rather than reporting emulation as native ARM32 performance.
    with Path("/proc/self/exe").open("rb") as executable:
        header = executable.read(20)
    if header[4] != (1 if pointer_bits == 32 else 2) or int.from_bytes(header[18:20], "little") != (40 if pointer_bits == 32 else 183):
        raise RuntimeError("Emulated ARM execution cannot be benchmarked")
    REPORT.mkdir(parents=True, exist_ok=True)
    loader, runtime_dir, runtime_info = install_glibc()
    d8, d8_info = install_d8(loader, runtime_dir)
    qjs, qjs_info = install_qjs()
    escargot, escargot_env = build_escargot()
    for executable in (qjs, escargot, Path(d8[-1])):
        with executable.open("rb") as source:
            header = source.read(20)
        if header[:4] != b"\x7fELF" or header[4] != (1 if pointer_bits == 32 else 2) or int.from_bytes(header[18:20], "little") != (40 if pointer_bits == 32 else 183):
            raise RuntimeError(f"Engine ELF architecture mismatch: {executable}")
    wtb = build_wtb()
    sunspider = build_sunspider()
    octane = ROOT / "test/octane"
    adapter = REPORT / "quickjs-shell.js"
    adapter.write_text("globalThis.load = std.loadScript;\n"
                       "globalThis.read = function(path) { var f = std.open(path, 'r'); if (!f) throw Error(path); try { return f.readAsString(); } finally { f.close(); } };\n"
                       "globalThis.gc = std.gc;\n")
    revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    data = {"measured_at": datetime.datetime.now(datetime.timezone.utc).isoformat(),
            "measured_date_kst": datetime.datetime.now(datetime.timezone(datetime.timedelta(hours=9))).date().isoformat(),
            "architecture": "arm" if ARCHITECTURE == "arm32" else "aarch64",
            "host_architecture": os.uname().machine, "pointer_bits": pointer_bits,
            "execution_mode": "native", "hostname": socket.gethostname(),
            "container_image": os.environ.get("BENCHMARK_CONTAINER_IMAGE"),
            "platform": platform.platform(), "cpu": CPU, "glibc_runtime": runtime_info,
            "cpu_info": Path("/proc/cpuinfo").read_text(),
            "wtb_min_samples": 3, "sunspider_repetitions": 5,
            "escargot_revision": revision,
            "escargot_source_revision": os.environ.get("ESCARGOT_SOURCE_REVISION", revision),
            "suite_revisions": {name: subprocess.check_output(
                ["git", "rev-parse", "HEAD"], cwd=ROOT / "test" / name, text=True).strip()
                for name in ("octane", "web-tooling-benchmark", "vendortest")},
            "engines": {"quickjs": qjs_info, "d8": d8_info,
                        "d8_jitless": {**d8_info, "flags": ["--jitless", "--expose-gc"]},
                        "escargot": {"version": revision}}}
    engines = [("quickjs", [loader, "--library-path", f"{runtime_dir}:{SYSTEM_LIBRARY_PATH}", qjs, "--std", "--stack-size", "8388608", "-I", adapter], os.environ.copy()),
               ("d8", [*d8, "--expose-gc"], os.environ.copy()),
               ("d8_jitless", [*d8, "--jitless", "--expose-gc"], os.environ.copy()),
               ("escargot", [loader, "--library-path",
                              f"{runtime_dir}:{escargot_env.get('LD_LIBRARY_PATH', '')}:{SYSTEM_LIBRARY_PATH}", escargot], escargot_env)]
    if args.scores_json:
        previous = json.loads(args.scores_json.read_text())
        for key in ("architecture", "hostname", "cpu", "escargot_source_revision", "suite_revisions"):
            if previous[key] != data[key]:
                raise RuntimeError(f"Imported score context differs: {key}")
        for name in ("quickjs", "d8", "d8_jitless"):
            for key in ("version", "sha256"):
                if previous["engines"][name][key] != data["engines"][name][key]:
                    raise RuntimeError(f"Imported engine version differs: {name}/{key}")
        data["score_measured_at"] = previous["measured_at"]
        data["score_source_sha256"] = sha256(args.scores_json)
        shutil.copyfile(args.scores_json, REPORT / "original-score-measurements.json")
        for pattern in ("*.log", "*.rss-kib.txt", "*.peak-rss-kib.txt"):
            for source in args.scores_json.parent.glob(pattern):
                shutil.copyfile(source, REPORT / source.name)
        for name, _, _ in engines:
            for suite in ("sunspider", "octane", "web_tooling"):
                data["engines"][name][suite] = previous["engines"][name][suite]
    if args.memory_only:
        drivers = build_memory_drivers(wtb)
        data["memory_driver_sha256"] = {kind: sha256(script) for kind, (_, script, _, _) in drivers.items()}
        data["wtb_memory_bundle_sha256"] = sha256(wtb / "dist/memory.js")
        data["monitor_sha256"] = sha256(ROOT / "tools/engine_memory_benchmark.py")
        run_memory_comparison(data, engines, drivers)
        return
    failures = []
    for name, command, env in engines:
        result = data["engines"][name]
        result["command"] = list(map(str, command))
        for kind, directory, script in (("sunspider", ROOT, sunspider),
                                        ("octane", octane, "run.js"),
                                        ("web_tooling", wtb, "dist/cli.js")):
            try:
                if kind == "sunspider":
                    measure(f"{name}-sunspider-warmup", command, directory, env, script, kind)
                    samples = [measure(f"{name}-sunspider-{i}", command, directory, env, script, kind)
                               for i in range(1, 6)]
                    result[kind] = {"total_milliseconds": statistics.mean(
                        sample["total_milliseconds"] for sample in samples),
                                    "average_rss_kib": round(sum(sample["average_rss_kib"] * sample["rss_sample_count"]
                                                                for sample in samples) / sum(
                                        sample["rss_sample_count"] for sample in samples)),
                                    "peak_rss_kib": max(sample["peak_rss_kib"] for sample in samples),
                                    "samples": samples}
                else:
                    result[kind] = measure(f"{name}-{kind}", command, directory, env, script, kind)
            except Exception as error:
                result[kind] = {"error": str(error)}
                failures.append(f"{name}/{kind}")
                print(f"{name}/{kind}: {error}", file=sys.stderr, flush=True)
            (REPORT / "measurements.json").write_text(json.dumps(data, indent=2) + "\n")
    rows = [f"# {ARCHITECTURE.upper()} JavaScript engine comparison", "", data["measured_date_kst"], "",
            f"Escargot source: `{data['escargot_source_revision']}`; CPU: {CPU}.", "",
            "| Engine | Version | SunSpider ms ↓ | Octane score ↑ | WTB runs/s ↑ |",
            "|---|---|---:|---:|---:|"]
    def value(result, key):
        return f"{result[key]:.2f}" if key in result else "failed"
    for name, _, _ in engines:
        result = data["engines"][name]
        rows.append(f"| {name} | {result['version']} | " +
                    " | ".join((value(result["sunspider"], "total_milliseconds"),
                                value(result["octane"], "score"),
                                value(result["web_tooling"], "score_runs_per_second"))) + " |")
    rows += ["", "SunSpider 1.0.2: one discarded run, five fresh-process repetitions, mean total ms.",
             "Octane 2.0: one scored run; WTB: minSamples=3.",
             "Memory is measured separately using the validated fixed-work protocol below.",
             "All engines use the same glibc runtime and CPU.", ""]
    summary = "\n".join(rows)
    (REPORT / "summary.md").write_text(summary)
    print(summary, flush=True)
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(os.environ["GITHUB_STEP_SUMMARY"], "a") as output:
            output.write(summary)
    if failures:
        raise RuntimeError("Benchmark failed for: " + ", ".join(failures))
    data["score_measured_at"] = data["measured_at"]
    if args.scores_only:
        (REPORT / "measurements.json").write_text(json.dumps(data, indent=2) + "\n")
        return
    drivers = build_memory_drivers(wtb)
    data["memory_driver_sha256"] = {kind: sha256(script) for kind, (_, script, _, _) in drivers.items()}
    data["wtb_memory_bundle_sha256"] = sha256(wtb / "dist/memory.js")
    data["monitor_sha256"] = sha256(ROOT / "tools/engine_memory_benchmark.py")
    run_memory_comparison(data, engines, drivers)


if __name__ == "__main__":
    main()
