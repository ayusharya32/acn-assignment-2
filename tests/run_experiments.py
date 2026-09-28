#!/usr/bin/env python3
import os
import sys
import json
import time
import signal
import subprocess
import math
import csv

BASE_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
BUILD_SERVER = os.path.join(BASE_DIR, "build", "server")
BUILD_CLIENT = os.path.join(BASE_DIR, "build", "client")
CONFIG_PATH = os.path.join(BASE_DIR, "config.json")
WORKLOAD_DIR = os.path.join(BASE_DIR, "workload")
FILES_DIR = os.path.join(BASE_DIR, "files")
RESULTS_DIR = os.path.join(BASE_DIR, "experiment_results")

CELLS = [
    {
        "id": "cell_1",
        "name": "Cell 1: FCFS (4 server threads)",
        "sched": "fcfs",
        "quantum": None,
        "server_threads": 4,
        "client_threads": 8,
    },
    {
        "id": "cell_2",
        "name": "Cell 2: SJF (4 server threads)",
        "sched": "sjf",
        "quantum": None,
        "server_threads": 4,
        "client_threads": 8,
    },
    {
        "id": "cell_3",
        "name": "Cell 3: RR (4 server threads, Q=4096)",
        "sched": "rr",
        "quantum": 4096,
        "server_threads": 4,
        "client_threads": 8,
    },
    {
        "id": "cell_4",
        "name": "Cell 4: DRR (4 server threads, Q=4096)",
        "sched": "drr",
        "quantum": 4096,
        "server_threads": 4,
        "client_threads": 8,
    },
    {
        "id": "cell_5",
        "name": "Cell 5: FCFS (1 server thread)",
        "sched": "fcfs",
        "quantum": None,
        "server_threads": 1,
        "client_threads": 8,
    },
    {
        "id": "cell_6",
        "name": "Cell 6: RR (1 server thread, Q=4096)",
        "sched": "rr",
        "quantum": 4096,
        "server_threads": 1,
        "client_threads": 8,
    },
]


def update_config(server_threads, client_threads, port=9000):
    config = {
        "server": {
            "ip": "127.0.0.1",
            "port": port,
            "server_threads": server_threads,
            "client_threads": client_threads
        },
        "load_balancer": {
            "ip": "127.0.0.1",
            "port": 8000,
            "health_interval_ms": 1000,
            "backends": [
                {"ip": "127.0.0.1", "port": 9001},
                {"ip": "127.0.0.1", "port": 9002},
                {"ip": "127.0.0.1", "port": 9003},
                {"ip": "127.0.0.1", "port": 9004}
            ]
        }
    }
    with open(CONFIG_PATH, "w") as f:
        json.dump(config, f, indent=2)


def percentile(values, p):
    if not values:
        return 0.0
    sorted_v = sorted(values)
    idx = math.ceil((p / 100.0) * len(sorted_v)) - 1
    idx = max(0, min(len(sorted_v) - 1, idx))
    return sorted_v[idx]


def classify_size(num_bytes):
    if num_bytes <= 4096:
        return "small"
    elif num_bytes <= 64 * 1024:
        return "medium"
    else:
        return "large"


def parse_cell_metrics(csv_path, sched=""):
    if not os.path.exists(csv_path):
        return None

    with open(csv_path, newline="") as f:
        rows = list(csv.DictReader(f))

    if not rows:
        return None

    # Filter out initial seeding requests (which have filename starting with workload_ but rounds=1 before load)
    # Actually all requests recorded in CSV are evaluated.
    waiting_times = []
    response_times = []
    arrivals = []
    finishes = []
    slowdown_by_class = {"small": [], "medium": [], "large": []}
    forfeited_by_class = {"small": 0, "medium": 0, "large": 0}
    counts_by_class = {"small": 0, "medium": 0, "large": 0}
    
    a14_firings = 0
    long_line_slowdowns = []

    for r in rows:
        arr = int(r["arrival_ns"])
        start = int(r["start_ns"])
        fin = int(r["finish_ns"])
        b = int(r["bytes"])
        forfeited = int(r["forfeited_bytes"])
        rounds = int(r["rounds"])
        fname = r.get("filename", "")

        wait = start - arr
        resp = fin - arr
        waiting_times.append(wait)
        response_times.append(resp)
        arrivals.append(arr)
        finishes.append(fin)

        s_class = classify_size(b)
        counts_by_class[s_class] += 1
        forfeited_by_class[s_class] += forfeited

        if b > 0:
            sd = float(resp) / float(b)
            slowdown_by_class[s_class].append(sd)
            if "longline" in fname:
                long_line_slowdowns.append(sd)

        # Detect A14 overrun: Under RR, any GET of longline file (line > 4096) fires A14 overrun on round 1
        if sched == "rr" and "longline" in fname and r.get("op") == "GET":
            a14_firings += 1

    total_window = (max(finishes) - min(arrivals)) / 1e9 if len(finishes) > 1 else 1.0
    throughput = len(rows) / total_window if total_window > 0 else 0.0

    return {
        "total_requests": len(rows),
        "wait_p50_ns": percentile(waiting_times, 50),
        "wait_p99_ns": percentile(waiting_times, 99),
        "resp_p50_ns": percentile(response_times, 50),
        "resp_p99_ns": percentile(response_times, 99),
        "throughput_rps": throughput,
        "window_sec": total_window,
        "counts_by_class": counts_by_class,
        "forfeited_by_class": forfeited_by_class,
        "slowdown_by_class": slowdown_by_class,
        "a14_firings": a14_firings,
        "long_line_slowdowns": long_line_slowdowns,
    }


def run_experiment(cell):
    print(f"\n=======================================================")
    print(f"  Running {cell['name']}...")
    print(f"=======================================================")

    os.makedirs(RESULTS_DIR, exist_ok=True)
    os.makedirs(FILES_DIR, exist_ok=True)
    os.makedirs(WORKLOAD_DIR, exist_ok=True)

    metrics_csv = os.path.join(RESULTS_DIR, f"metrics_{cell['id']}.csv")
    if os.path.exists(metrics_csv):
        os.remove(metrics_csv)

    # 1. Update config.json
    update_config(cell["server_threads"], cell["client_threads"], port=9000)

    # 2. Build server command
    cmd = [
        BUILD_SERVER,
        "--config", CONFIG_PATH,
        "--sched", cell["sched"],
        "--file", FILES_DIR,
        "--metrics-out", metrics_csv
    ]
    if cell["quantum"]:
        cmd.extend(["--quantum", str(cell["quantum"])])

    print(f"Starting server: {' '.join(cmd)}")
    server_proc = subprocess.Popen(
        cmd,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL
    )

    # Allow server to start listening
    time.sleep(0.5)

    # 3. Run client load
    client_cmd = [
        BUILD_CLIENT,
        "load",
        WORKLOAD_DIR,
        "--requests", "1000",
        "--config", CONFIG_PATH
    ]
    print(f"Running client: {' '.join(client_cmd)}")
    start_time = time.time()
    client_res = subprocess.run(client_cmd, capture_output=True, text=True)
    elapsed = time.time() - start_time
    print(f"Client finished in {elapsed:.2f}s with returncode {client_res.returncode}")
    if client_res.stderr:
        print(f"Client stderr: {client_res.stderr.strip()}")

    # 4. Gracefully terminate server (SIGINT)
    time.sleep(0.3)
    print("Sending SIGINT to server for graceful shutdown and CSV export...")
    server_proc.send_signal(signal.SIGINT)
    try:
        server_proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        print("Server did not exit within timeout, killing...")
        server_proc.kill()
        server_proc.wait()

    # 5. Parse results
    metrics = parse_cell_metrics(metrics_csv, cell["sched"])
    return metrics


def main():
    # Make sure binaries exist
    subprocess.run(["make"], cwd=BASE_DIR, check=True)

    # Ensure workload exists
    subprocess.run(["python3", os.path.join(BASE_DIR, "tests", "generate_workload.py")], check=True)

    results = {}
    for cell in CELLS:
        metrics = run_experiment(cell)
        results[cell["id"]] = (cell, metrics)

    # Print Summary Tables
    print("\n\n" + "=" * 80)
    print("                      PART A EXPERIMENTAL RESULTS")
    print("=" * 80)

    print("\n--- TABLE 1: Waiting Time (p50/p99) and Throughput (Rule A28) ---")
    print(f"{'Cell / Configuration':<38} | {'Wait p50 (ms)':<14} | {'Wait p99 (ms)':<14} | {'Throughput (req/s)':<18}")
    print("-" * 92)
    for cell_id, (cell, m) in results.items():
        if not m:
            print(f"{cell['name']:<38} | {'ERROR':<14} | {'ERROR':<14} | {'ERROR':<18}")
            continue
        p50_ms = m["wait_p50_ns"] / 1e6
        p99_ms = m["wait_p99_ns"] / 1e6
        tp = m["throughput_rps"]
        print(f"{cell['name']:<38} | {p50_ms:>11.3f} ms | {p99_ms:>11.3f} ms | {tp:>14.2f} req/s")

    print("\n\n--- TABLE 2: Normalized Slowdown (ns/byte) by Size Class (Rule A21) ---")
    print(f"{'Policy (4 threads)':<22} | {'Small Median':<14} | {'Small p99':<14} | {'Med Median':<14} | {'Med p99':<14} | {'Large Median':<14} | {'Large p99':<14}")
    print("-" * 115)
    for cell_id in ["cell_1", "cell_2", "cell_3", "cell_4"]:
        cell, m = results[cell_id]
        if not m:
            continue
        sd = m["slowdown_by_class"]
        s_med = percentile(sd["small"], 50) or 0
        s_p99 = percentile(sd["small"], 99) or 0
        m_med = percentile(sd["medium"], 50) or 0
        m_p99 = percentile(sd["medium"], 99) or 0
        l_med = percentile(sd["large"], 50) or 0
        l_p99 = percentile(sd["large"], 99) or 0
        print(f"{cell['sched'].upper():<22} | {s_med:>14.2f} | {s_p99:>14.2f} | {m_med:>14.2f} | {m_p99:>14.2f} | {l_med:>14.2f} | {l_p99:>14.2f}")

    print("\n\n--- TABLE 3: RR vs DRR Comparison (Rule A29) ---")
    print(f"{'Policy':<10} | {'Small Forfeited':<16} | {'Med Forfeited':<16} | {'Large Forfeited':<16} | {'Total Forfeited':<16} | {'A14 Overruns':<14} | {'Long-Line Med (ns/B)':<22} | {'Long-Line p99 (ns/B)':<22}")
    print("-" * 148)
    for cell_id in ["cell_3", "cell_4"]:
        cell, m = results[cell_id]
        if not m:
            continue
        f_s = m["forfeited_by_class"]["small"]
        f_m = m["forfeited_by_class"]["medium"]
        f_l = m["forfeited_by_class"]["large"]
        tot = f_s + f_m + f_l
        a14 = m["a14_firings"]
        ll_med = percentile(m["long_line_slowdowns"], 50) or 0.0
        ll_p99 = percentile(m["long_line_slowdowns"], 99) or 0.0
        print(f"{cell['sched'].upper():<10} | {f_s:>16} | {f_m:>16} | {f_l:>16} | {tot:>16} | {a14:>14} | {ll_med:>22.2f} | {ll_p99:>22.2f}")

    # Generate JSON summary
    summary_file = os.path.join(RESULTS_DIR, "summary.json")
    json_results = {}
    for cid, (c, m) in results.items():
        if m:
            # remove raw lists for clean json
            clean_m = {k: v for k, v in m.items() if k not in ["slowdown_by_class", "long_line_slowdowns"]}
            clean_m["slowdown_summary"] = {
                cls: {
                    "median": percentile(m["slowdown_by_class"][cls], 50),
                    "p99": percentile(m["slowdown_by_class"][cls], 99),
                }
                for cls in ["small", "medium", "large"]
            }
            json_results[cid] = {"config": c, "metrics": clean_m}

    with open(summary_file, "w") as f:
        json.dump(json_results, f, indent=2)
    print(f"\nSaved raw summary to {summary_file}")


if __name__ == "__main__":
    main()
