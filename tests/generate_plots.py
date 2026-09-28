#!/usr/bin/env python3
import os
import json
import sys

BASE_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
RESULTS_DIR = os.path.join(BASE_DIR, "experiment_results")
SUMMARY_JSON = os.path.join(RESULTS_DIR, "summary.json")
PLOTS_DIR = os.path.join(BASE_DIR, "plots")

def main():
    if not os.path.exists(SUMMARY_JSON):
        print(f"Summary file not found: {SUMMARY_JSON}. Run tests/run_experiments.py first.")
        return 1

    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib not installed. To generate PNG plots, install it with:")
        print("  pip3 install matplotlib   OR   sudo apt install python3-matplotlib")
        return 0

    with open(SUMMARY_JSON) as f:
        data = json.load(f)

    os.makedirs(PLOTS_DIR, exist_ok=True)

    # 1. Waiting Time Comparison (p50 and p99)
    cells = ["cell_1", "cell_2", "cell_3", "cell_4", "cell_5", "cell_6"]
    labels = [
        "FCFS\n(4 thr)",
        "SJF\n(4 thr)",
        "RR\n(4 thr)",
        "DRR\n(4 thr)",
        "FCFS\n(1 thr)",
        "RR\n(1 thr)",
    ]
    p50_vals = [data[c]["metrics"]["wait_p50_ns"] / 1e6 for c in cells]
    p99_vals = [data[c]["metrics"]["wait_p99_ns"] / 1e6 for c in cells]

    x = range(len(cells))
    width = 0.35

    fig, ax = plt.subplots(figsize=(10, 5))
    rects1 = ax.bar([i - width/2 for i in x], p50_vals, width, label="p50 (Median) Wait Time (ms)", color="#3498db")
    rects2 = ax.bar([i + width/2 for i in x], p99_vals, width, label="p99 Wait Time (ms)", color="#e74c3c")

    ax.set_ylabel("Waiting Time (ms)")
    ax.set_title("Waiting Time Comparison across Scheduling Policies (Rule A28)")
    ax.set_xticks(x)
    ax.set_xticklabels(labels)
    ax.legend()
    ax.grid(axis="y", linestyle="--", alpha=0.7)

    plt.tight_layout()
    p1 = os.path.join(PLOTS_DIR, "waiting_time_comparison.png")
    plt.savefig(p1, dpi=300)
    plt.close()
    print(f"Saved {p1}")

    # 2. Throughput Comparison
    tps = [data[c]["metrics"]["throughput_rps"] for c in cells]
    fig, ax = plt.subplots(figsize=(10, 5))
    ax.bar(labels, tps, color="#2ecc71", width=0.5)
    ax.set_ylabel("Throughput (requests/sec)")
    ax.set_title("Throughput Comparison across Configurations (Rule A28)")
    ax.grid(axis="y", linestyle="--", alpha=0.7)

    for i, v in enumerate(tps):
        ax.text(i, v + 50, f"{v:.1f}", ha="center", fontweight="bold")

    plt.tight_layout()
    p2 = os.path.join(PLOTS_DIR, "throughput_comparison.png")
    plt.savefig(p2, dpi=300)
    plt.close()
    print(f"Saved {p2}")

    # 3. Normalized Slowdown across Size Classes (4 threads)
    ref_cells = ["cell_1", "cell_2", "cell_3", "cell_4"]
    ref_labels = ["FCFS", "SJF", "RR", "DRR"]
    classes = ["small", "medium", "large"]

    fig, ax = plt.subplots(figsize=(10, 5))
    x_sd = range(len(ref_labels))
    w_sd = 0.25

    for idx, cls in enumerate(classes):
        medians = [data[c]["metrics"]["slowdown_summary"][cls]["median"] for c in ref_cells]
        ax.bar([i + (idx - 1)*w_sd for i in x_sd], medians, w_sd, label=f"{cls.capitalize()} Files")

    ax.set_ylabel("Median Normalized Slowdown (ns/byte)")
    ax.set_title("Normalized Slowdown by File Size Class (Rule A21)")
    ax.set_xticks(x_sd)
    ax.set_xticklabels(ref_labels)
    ax.legend()
    ax.grid(axis="y", linestyle="--", alpha=0.7)

    plt.tight_layout()
    p3 = os.path.join(PLOTS_DIR, "slowdown_comparison.png")
    plt.savefig(p3, dpi=300)
    plt.close()
    print(f"Saved {p3}")

    # 4. RR vs DRR Forfeited Bytes
    fig, ax = plt.subplots(figsize=(6, 5))
    rr_forfeited = data["cell_3"]["metrics"]["forfeited_by_class"]
    drr_forfeited = data["cell_4"]["metrics"]["forfeited_by_class"]

    rr_tot = sum(rr_forfeited.values())
    drr_tot = sum(drr_forfeited.values())

    ax.bar(["RR (Quantum=4096)", "DRR (Quantum=4096)"], [rr_tot, drr_tot], color=["#e67e22", "#9b59b6"], width=0.4)
    ax.set_ylabel("Total Forfeited Bytes")
    ax.set_title("RR vs DRR Forfeited Bytes Comparison (Rule A29)")
    ax.grid(axis="y", linestyle="--", alpha=0.7)
    ax.text(0, rr_tot + 2000, f"{rr_tot:,} bytes", ha="center", fontweight="bold")
    ax.text(1, drr_tot + 2000, f"{drr_tot} bytes", ha="center", fontweight="bold")

    plt.tight_layout()
    p4 = os.path.join(PLOTS_DIR, "forfeited_bytes_comparison.png")
    plt.savefig(p4, dpi=300)
    plt.close()
    print(f"Saved {p4}")

    # 5. Long-line File Slowdown Comparison (RR vs DRR)
    fig, ax = plt.subplots(figsize=(6, 5))
    rr_ll_med = data["cell_3"]["metrics"].get("long_line_summary", {}).get("median", 0)
    drr_ll_med = data["cell_4"]["metrics"].get("long_line_summary", {}).get("median", 0)
    rr_ll_p99 = data["cell_3"]["metrics"].get("long_line_summary", {}).get("p99", 0)
    drr_ll_p99 = data["cell_4"]["metrics"].get("long_line_summary", {}).get("p99", 0)

    # Fallback to direct CSV calculation if 0
    if rr_ll_med == 0 or drr_ll_med == 0:
        import csv
        def get_ll(csv_path):
            if not os.path.exists(csv_path):
                return 0, 0
            sds = []
            with open(csv_path, newline="") as f:
                for r in csv.DictReader(f):
                    if "longline" in r.get("filename", ""):
                        b = int(r["bytes"])
                        if b > 0:
                            resp = int(r["finish_ns"]) - int(r["arrival_ns"])
                            sds.append(float(resp) / float(b))
            if not sds:
                return 0, 0
            sds.sort()
            import math
            med = sds[max(0, math.ceil(0.50 * len(sds)) - 1)]
            p99 = sds[max(0, math.ceil(0.99 * len(sds)) - 1)]
            return med, p99

        rr_med_csv, rr_p99_csv = get_ll(os.path.join(RESULTS_DIR, "metrics_cell_3.csv"))
        drr_med_csv, drr_p99_csv = get_ll(os.path.join(RESULTS_DIR, "metrics_cell_4.csv"))
        if rr_ll_med == 0: rr_ll_med, rr_ll_p99 = rr_med_csv, rr_p99_csv
        if drr_ll_med == 0: drr_ll_med, drr_ll_p99 = drr_med_csv, drr_p99_csv

    x_ll = range(2)
    w_ll = 0.35
    ax.bar([i - w_ll/2 for i in x_ll], [rr_ll_med, drr_ll_med], w_ll, label="Median Slowdown", color="#2980b9")
    ax.bar([i + w_ll/2 for i in x_ll], [rr_ll_p99, drr_ll_p99], w_ll, label="p99 Slowdown", color="#c0392b")

    ax.set_ylabel("Normalized Slowdown (ns/byte)")
    ax.set_title("Long-Line File Slowdown: RR vs DRR (Rule A29)")
    ax.set_xticks(x_ll)
    ax.set_xticklabels(["RR (Q=4096)", "DRR (Q=4096)"])
    ax.legend()
    ax.grid(axis="y", linestyle="--", alpha=0.7)

    plt.tight_layout()
    p5 = os.path.join(PLOTS_DIR, "longline_slowdown_comparison.png")
    plt.savefig(p5, dpi=300)
    plt.close()
    print(f"Saved {p5}")

    print("All plots successfully generated in ./plots/")

if __name__ == "__main__":
    main()
