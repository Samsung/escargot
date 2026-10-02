"""Fixed-work Linux memory measurements for native JavaScript engine shells."""

import errno
import hashlib
import json
import os
from pathlib import Path
import pty
import re
import selectors
import signal
import statistics
import subprocess
import time
import tty

METHOD = "fixed-work-smaps-rollup-v2"
INTERVAL_SECONDS = 0.01


def memory_snapshot(pid, cpu):
    status = Path(f"/proc/{pid}/status").read_text()
    affinity = re.search(r"^Cpus_allowed_list:\s+(.*)$", status, re.M)
    if not affinity or affinity.group(1) != str(cpu):
        raise RuntimeError(f"Unexpected engine affinity: {affinity}")
    raw = Path(f"/proc/{pid}/smaps_rollup").read_text()
    fields = {key: int(value) for key, value in re.findall(r"^(\w+):\s+(\d+) kB$", raw, re.M)}
    required = ("Rss", "Pss", "Private_Clean", "Private_Dirty", "Swap")
    if any(key not in fields for key in required) or fields["Rss"] <= 0:
        raise RuntimeError(f"Missing or invalid smaps_rollup memory fields: {fields}")
    if fields["Swap"] or fields.get("Private_Hugetlb", 0) or fields.get("Shared_Hugetlb", 0):
        raise RuntimeError("Swapping or explicit huge pages would change the memory comparison")
    values = {"rss_kib": fields["Rss"], "pss_kib": fields["Pss"],
              "uss_kib": fields["Private_Clean"] + fields["Private_Dirty"],
              "swap_kib": fields["Swap"]}
    return values, raw, status


def frozen_snapshot(pid, cpu):
    os.kill(pid, signal.SIGSTOP)
    deadline = time.monotonic() + 5
    while True:
        statuses = [path.read_text() for path in Path(f"/proc/{pid}/task").glob("*/status")]
        if not statuses:
            raise RuntimeError("Engine exited before memory checkpoint")
        if all(re.search(r"^State:\s+[Tt]", status, re.M) for status in statuses):
            for status in statuses:
                if not re.search(rf"^Cpus_allowed_list:\s+{cpu}$", status, re.M):
                    raise RuntimeError("An engine thread escaped the selected CPU")
            return memory_snapshot(pid, cpu)
        if time.monotonic() > deadline:
            raise RuntimeError(f"Could not stop engine PID {pid}: {[re.findall(r'^State:.*$', item, re.M) for item in statuses]}")
        time.sleep(0.001)


def aggregate(samples):
    metrics = ("average_rss_kib", "peak_rss_kib", "average_pss_kib", "average_uss_kib")
    result = {"method": METHOD, "repetitions": len(samples), "samples": samples}
    for key in metrics:
        values = [sample[key] for sample in samples]
        result[key] = statistics.median(values)
        result[key + "_range"] = [min(values), max(values)]
    return result


def measure_memory(name, engine, command, directory, env, script, report, cpu, monitor_cpu):
    """Measure a fresh process; checkpoint barriers are excluded from averages."""
    report = Path(report)
    fifo = report / f"{name}.ack.fifo"
    os.mkfifo(fifo)
    env = {**env, "ESCARGOT_MEMORY_ACK_FIFO": str(fifo)}
    bridge = report / f"{name}.bridge.js"
    if engine == "quickjs":
        wait = "std.out.flush(); var ack = std.in.getline();"
    elif engine in ("d8", "d8_jitless"):
        wait = "var ack = readline();"
    elif engine == "escargot":
        wait = "var ack = read(" + json.dumps(str(fifo)) + ");"
    else:
        raise ValueError(f"Unknown engine bridge: {engine}")
    bridge.write_text("globalThis.benchmarkCheckpoint = function(label) {\n" +
                      "print('MEM_CHECKPOINT ' + JSON.stringify(label));\n" + wait + "\n" +
                      "if (typeof ack !== 'string' || ack.trim() !== 'continue') throw Error('Memory barrier failed');\n};\n")
    launch_command = [*command, "-I", bridge] if engine == "quickjs" else [*command, bridge]
    master, slave = pty.openpty()
    tty.setraw(slave)
    peak_file = report / f"{name}.peak-rss-kib.txt"
    log_file = report / f"{name}.log"
    raw_file = report / f"{name}.memory.jsonl"
    selector = selectors.DefaultSelector()
    selector.register(master, selectors.EVENT_READ)
    original_affinity = os.sched_getaffinity(0)
    process = None
    checkpoints = []
    samples = []
    segments = []
    segment = None
    engine_pid = None
    work = None
    buffer = b""
    started = time.monotonic()
    next_sample = started
    next_progress = started + 30
    print(f"Starting {name}: {METHOD}, engine CPU {cpu}, monitor CPU {monitor_cpu}", flush=True)
    try:
        os.sched_setaffinity(0, {monitor_cpu})
        process = subprocess.Popen(["taskset", "-c", str(cpu), "/usr/bin/time", "-f", "%M",
                                    "-o", str(peak_file), *map(str, launch_command), str(script)],
                                   cwd=directory, env=env, stdin=subprocess.PIPE,
                                   stdout=slave, stderr=slave)
        os.close(slave)
        slave = None
        with log_file.open("wb") as log, raw_file.open("w") as raw:
            while process.poll() is None or selector.get_map():
                now = time.monotonic()
                if engine_pid is None and process.poll() is None:
                    children = Path(f"/proc/{process.pid}/task/{process.pid}/children").read_text().split()
                    if children:
                        engine_pid = int(children[0])
                for key, _ in selector.select(max(0, min(0.01, next_sample - now))):
                    try:
                        chunk = os.read(master, 65536)
                    except OSError as error:
                        if error.errno != errno.EIO:
                            raise
                        chunk = b""
                    if not chunk:
                        selector.unregister(master)
                        continue
                    log.write(chunk)
                    log.flush()
                    buffer += chunk
                    while b"\n" in buffer:
                        line, buffer = buffer.split(b"\n", 1)
                        line = line.decode("utf-8", "replace").strip()
                        if line.startswith("MEM_WORK_JSON "):
                            work = json.loads(line[len("MEM_WORK_JSON "):])
                        if not line.startswith("MEM_CHECKPOINT "):
                            continue
                        label = json.loads(line[len("MEM_CHECKPOINT "):])
                        event_time = time.monotonic()
                        values, smaps, status = frozen_snapshot(engine_pid, cpu)
                        checkpoint = {"label": label, "elapsed_seconds": event_time - started, **values}
                        checkpoints.append(checkpoint)
                        index = len(checkpoints)
                        (report / f"{name}.checkpoint-{index:02}.smaps.txt").write_text(smaps)
                        (report / f"{name}.checkpoint-{index:02}.status.txt").write_text(status)
                        raw.write(json.dumps({"type": "checkpoint", **checkpoint}) + "\n")
                        raw.flush()
                        if segment is not None:
                            segment.append((event_time, values))
                            segments.append(segment)
                            segment = None
                        os.kill(engine_pid, signal.SIGCONT)
                        if engine == "escargot":
                            deadline = time.monotonic() + 5
                            while True:
                                try:
                                    fd = os.open(fifo, os.O_WRONLY | os.O_NONBLOCK)
                                    break
                                except OSError as error:
                                    if error.errno != errno.ENXIO or time.monotonic() > deadline:
                                        raise
                                    time.sleep(0.001)
                            os.write(fd, b"continue\n")
                            os.close(fd)
                        else:
                            process.stdin.write(b"continue\n")
                            process.stdin.flush()
                        if label not in ("workload_end", "after_gc"):
                            resumed_at = time.monotonic()
                            segment = [(resumed_at, values)]
                            raw.write(json.dumps({"type": "resume", "elapsed_seconds": resumed_at - started, **values}) + "\n")
                            raw.flush()
                        next_sample = time.monotonic() + INTERVAL_SECONDS
                        print(f"{name}: {label}: RSS {values['rss_kib']} / PSS {values['pss_kib']} / USS {values['uss_kib']} KiB", flush=True)
                now = time.monotonic()
                if segment is not None and now >= next_sample:
                    try:
                        values, _, _ = memory_snapshot(engine_pid, cpu)
                    except (FileNotFoundError, ProcessLookupError):
                        if process.poll() is None:
                            raise RuntimeError("Engine exited without workload_end checkpoint")
                    else:
                        sampled_at = time.monotonic()
                        segment.append((sampled_at, values))
                        sample = {"elapsed_seconds": sampled_at - started, **values}
                        samples.append(sample)
                        raw.write(json.dumps({"type": "sample", **sample}) + "\n")
                    next_sample = time.monotonic() + INTERVAL_SECONDS
                if now >= next_progress:
                    print(f"{name}: {(now - started):.0f}s elapsed; {len(samples)} memory samples", flush=True)
                    next_progress = now + 30
                if now - started > 1800:
                    raise RuntimeError("Memory run exceeded 30 minutes")
        code = process.wait()
        if code:
            raise RuntimeError(f"{name} exited {code}; see {log_file.name}")
        labels = [point["label"] for point in checkpoints]
        if labels[:2] != ["baseline", "loaded"] or labels[-2:] != ["workload_end", "after_gc"]:
            raise RuntimeError(f"Incomplete memory checkpoints: {labels}")
        if not work or not samples or any(value <= 0 for value in work.values()):
            raise RuntimeError("Missing fixed-work counts or memory samples")
        totals = {"rss_kib": 0, "pss_kib": 0, "uss_kib": 0}
        duration = 0
        for points in segments:
            for (left_time, left), (right_time, right) in zip(points, points[1:]):
                elapsed = right_time - left_time
                duration += elapsed
                for metric in totals:
                    totals[metric] += elapsed * (left[metric] + right[metric]) / 2
        if duration <= 0:
            raise RuntimeError("No active measurement interval")
        return {"method": METHOD, "average_rss_kib": totals["rss_kib"] / duration,
                "average_pss_kib": totals["pss_kib"] / duration,
                "average_uss_kib": totals["uss_kib"] / duration,
                "peak_rss_kib": int(peak_file.read_text().strip()),
                "peak_rss_method": "GNU time / wait4 ru_maxrss (whole process lifetime)",
                "sampled_peak_rss_kib": max(point["rss_kib"] for point in samples + checkpoints),
                "sampled_peak_pss_kib": max(point["pss_kib"] for point in samples + checkpoints),
                "sampled_peak_uss_kib": max(point["uss_kib"] for point in samples + checkpoints),
                "active_duration_seconds": duration, "duration_seconds": time.monotonic() - started,
                "checkpoints": checkpoints, "work_counts": work,
                "work_signature": hashlib.sha256(json.dumps(work, sort_keys=True).encode()).hexdigest(),
                "cpu_affinity": [str(cpu)], "monitor_cpu_affinity": [str(monitor_cpu)],
                "rss_sample_count": len(samples), "sampling_interval_ms": 10,
                "raw_samples_file": raw_file.name, "log": log_file.name}
    finally:
        if process is not None and process.poll() is None:
            if engine_pid:
                try:
                    os.kill(engine_pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
            process.kill()
            process.wait()
        os.sched_setaffinity(0, original_affinity)
        selector.close()
        os.close(master)
        if slave is not None:
            os.close(slave)
        fifo.unlink(missing_ok=True)
