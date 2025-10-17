#!/usr/bin/env python3
import argparse, sys, math, statistics
import numpy as np
import matplotlib.pyplot as plt
import csv, os

def pct(arr, p):
  return float(np.percentile(arr, p, interpolation="linear"))

def ns_to_ms(ns): return ns / 1e6

def main():
  ap = argparse.ArgumentParser()
  ap.add_argument("--latencies", help="latencies_ns.txt produced by app")
  ap.add_argument("--append", help="metrics.csv to append")
  ap.add_argument("--scenario")
  ap.add_argument("--plot")
  ap.add_argument("--metrics")
  args = ap.parse_args()

  if args.latencies and args.append and args.scenario:
    with open(args.latencies) as f:
      ns = [int(line.strip()) for line in f if line.strip().isdigit()]
    if not ns:
      print("No latencies.", file=sys.stderr); sys.exit(1)
    p50 = ns_to_ms(pct(ns, 50))
    p95 = ns_to_ms(pct(ns, 95))
    p99 = ns_to_ms(pct(ns, 99))
    duration_s = len(ns) / max(1, len(ns))  # placeholder if you also print total_s 
    # throughput: if main program prints total_seconds, read it; using len/ns as placeholder to avoid failure
    tput = len(ns) / 10.0  # assume 10 seconds run time; align with --duration
    drop_rate = 0.0        # if main program prints dropped_count/produced_count, include them here
    with open(args.append, "a", newline="") as wf:
      w = csv.writer(wf)
      w.writerow([args.scenario, f"{p50:.3f}", f"{p95:.3f}", f"{p99:.3f}", f"{tput:.0f}", f"{drop_rate:.3f}"])

  if args.plot and args.metrics:
    import pandas as pd
    df = pd.read_csv(args.metrics)
    # Latency 圖
    plt.figure()
    x = np.arange(len(df))
    plt.plot(x, df["latency_p50_ms"], marker="o", label="p50")
    plt.plot(x, df["latency_p95_ms"], marker="o", label="p95")
    plt.plot(x, df["latency_p99_ms"], marker="o", label="p99")
    plt.xticks(x, df["scenario"], rotation=20)
    plt.ylabel("Latency (ms)"); plt.title("Latency by Scenario"); plt.legend()
    plt.tight_layout(); plt.savefig(os.path.join(args.plot, "latency.png")); plt.close()

    # Throughput 圖
    plt.figure()
    plt.bar(df["scenario"], df["throughput_ops_s"])
    plt.ylabel("Throughput (ops/s)"); plt.title("Throughput by Scenario")
    plt.tight_layout(); plt.savefig(os.path.join(args.plot, "throughput.png")); plt.close()

if __name__ == "__main__":
  main()
