#!/usr/bin/env python3
import argparse, sys, math, statistics
import numpy as np
import matplotlib.pyplot as plt
import pandas as pd
import csv, os

def pct(arr, p):
    """Calculates the given percentile of an array."""
    return float(np.percentile(arr, p, interpolation="linear"))

def ns_to_ms(ns):
    """Converts nanoseconds to milliseconds."""
    return ns / 1e6

def parse_stats(stats_file):
    """Parses the key-value CSV stats file produced by the C program."""
    stats = {}
    try:
        with open(stats_file, 'r', encoding='utf-8', errors='ignore') as f:
            reader = csv.reader(f)
            for row in reader:
                if len(row) == 2:
                    stats[row[0]] = float(row[1])
    except FileNotFoundError:
        print(f"Error: Stats file not found at {stats_file}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"Error reading stats file {stats_file}: {e}", file=sys.stderr)
        sys.exit(1)
    return stats

def main():
    ap = argparse.ArgumentParser(description="Calculate and plot performance metrics.")
    
    # --- Args for MODE 1: APPEND (called by one_run) ---
    ap.add_argument("--latencies", help="Path to latencies_ns.txt file")
    ap.add_argument("--stats", help="Path to stats.txt file")
    ap.add_argument("--append", help="Path to raw metrics.csv to append to")
    ap.add_argument("--scenario", help="Name of the scenario")
    ap.add_argument("--rep", type=int, help="Repetition index (e.g., 1, 2, 3)")
    
    # --- Args for MODE 2: AGGREGATE (called by aggregate_and_plot) ---
    ap.add_argument("--aggregate", help="Path to raw metrics.csv to aggregate from")
    ap.add_argument("--out", help="Path for final aggregated metrics.csv")
    
    # --- Args for MODE 3: PLOT (called by aggregate_and_plot) ---
    ap.add_argument("--plot", help="Directory to save plots")
    ap.add_argument("--metrics", help="Path to final aggregated metrics.csv for plotting")
    
    args = ap.parse_args()

    # --- MODE 1: APPEND (from one_run) ---
    # Calculates metrics for a single run and appends to the raw CSV
    if args.latencies and args.stats and args.append and args.scenario and args.rep is not None:
        try:
            with open(args.latencies) as f:
                ns = [int(line.strip()) for line in f if line.strip().isdigit()]
            if not ns:
                print(f"Warning: No latencies found in {args.latencies}", file=sys.stderr)
                p50, p95, p99 = 0.0, 0.0, 0.0
            else:
                p50 = ns_to_ms(pct(ns, 50))
                p95 = ns_to_ms(pct(ns, 95))
                p99 = ns_to_ms(pct(ns, 99))

            # Read the actual stats from the C program's output
            stats = parse_stats(args.stats)
            duration_s = stats.get("duration_s", 0.0)
            consumed = stats.get("consumed", 0.0)
            produced = stats.get("produced", 0.0)
            dropped = stats.get("dropped", 0.0)

            # Calculate precise throughput and drop rate
            tput = consumed / duration_s if duration_s > 0 else 0
            total_attempts = produced + dropped
            drop_rate = dropped / total_attempts if total_attempts > 0 else 0

            # Append to the raw CSV, matching the header in run.sh
            with open(args.append, "a", newline="") as wf:
                w = csv.writer(wf)
                w.writerow([
                    args.scenario, 
                    args.rep, 
                    f"{p50:.3f}", 
                    f"{p95:.3f}", 
                    f"{p99:.3f}", 
                    f"{tput:.0f}", 
                    f"{drop_rate:.3f}"
                ])
        except FileNotFoundError:
            print(f"Error: Latency file not found at {args.latencies}", file=sys.stderr)
            sys.exit(1)
        except Exception as e:
            print(f"Error during append mode: {e}", file=sys.stderr)
            sys.exit(1)

    # --- MODE 2: AGGREGATE (from aggregate_and_plot, call 1) ---
    # Reads the raw CSV (with all reps) and writes the final aggregated CSV
    elif args.aggregate and args.out:
        try:
            df = pd.read_csv(args.aggregate)
            # Group by scenario and calculate the mean for all numeric columns
            agg_df = df.groupby('scenario', sort=False).mean(numeric_only=True)
            
            # Drop the 'rep' column as 'mean of rep' is meaningless
            if 'rep' in agg_df.columns:
                agg_df = agg_df.drop(columns=['rep'])

            rounding_rules = {
                'latency_p50_ms': 3,
                'latency_p95_ms': 3,
                'latency_p99_ms': 3,
                'throughput_ops_s': 0,
                'drop_rate': 3
            }
            
            agg_df = agg_df.round(rounding_rules)
            
            if 'throughput_ops_s' in agg_df.columns:
                agg_df['throughput_ops_s'] = agg_df['throughput_ops_s'].astype(int)

            # Reset index to make 'scenario' a column
            agg_df = agg_df.reset_index()
            
            # Save the final aggregated CSV
            agg_df.to_csv(args.out, index=False)
            print(f"Aggregated metrics written to {args.out}")

        except FileNotFoundError:
            print(f"Error: Raw metrics file not found at {args.aggregate}", file=sys.stderr)
            sys.exit(1)
        except Exception as e:
            print(f"Error during aggregation: {e}", file=sys.stderr)
            sys.exit(1)

    # --- MODE 3: PLOT (from aggregate_and_plot, call 2) ---
    # Reads the final aggregated CSV and generates plots
    elif args.plot and args.metrics:
        try:
            df = pd.read_csv(args.metrics)
            
            # Latency Plot
            plt.figure(figsize=(10, 6))
            x = np.arange(len(df))
            
            y_p50 = df["latency_p50_ms"]
            y_p95 = df["latency_p95_ms"]
            y_p99 = df["latency_p99_ms"]
            
            plt.plot(x, y_p50, marker="o", linestyle="--", label="p50 (median)")
            plt.plot(x, y_p95, marker="s", linestyle="-", label="p95")
            plt.plot(x, y_p99, marker="^", linestyle="-", label="p99")
            
            # label p50
            for xi, yi in zip(x, y_p50):
                plt.annotate(f'{yi:.3f}',
                             (xi, yi),
                             textcoords="offset points",
                             xytext=(0, 5),
                             ha='center',
                             fontsize=8)

            # label p95
            for xi, yi in zip(x, y_p95):
                plt.annotate(f'{yi:.3f}', (xi, yi), textcoords="offset points", xytext=(0, 5), ha='center', fontsize=8)

            # label p99
            for xi, yi in zip(x, y_p99):
                plt.annotate(f'{yi:.3f}', (xi, yi), textcoords="offset points", xytext=(0, 5), ha='center', fontsize=8)
            # plt.plot(x, df["latency_p50_ms"], marker="o", linestyle="--", label="p50 (median)")
            # plt.plot(x, df["latency_p95_ms"], marker="s", linestyle="-", label="p95")
            # plt.plot(x, df["latency_p99_ms"], marker="^", linestyle="-", label="p99")
            plt.xticks(x, df["scenario"], rotation=20, ha="right")
            plt.ylabel("Latency (ms) - Log Scale"); plt.title("Latency by Scenario (Lower is Better)")
            plt.legend(); plt.grid(axis='y', linestyle='--', alpha=0.7)
            plt.yscale('log') # Use log scale for latency
            plt.tight_layout(); plt.savefig(os.path.join(args.plot, "latency.png")); plt.close()

            # Throughput Plot
            plt.figure(figsize=(10, 6))
            bars = plt.bar(df["scenario"], df["throughput_ops_s"], color="C0")
            plt.xticks(rotation=20, ha="right")
            plt.ylabel("Throughput (ops/s)"); plt.title("Throughput by Scenario (Higher is Better)")
            plt.grid(axis='y', linestyle='--', alpha=0.7)
            for bar in bars:
                yval = bar.get_height()
                plt.text(bar.get_x() + bar.get_width()/2.0, yval,
                         f'{yval:.0f}',
                         va='bottom',
                         ha='center')
            plt.tight_layout(); plt.savefig(os.path.join(args.plot, "throughput.png")); plt.close()

            # Drop Rate Plot
            plt.figure(figsize=(10, 6))
            bars = plt.bar(df["scenario"], df["drop_rate"] * 100, color="C3") # Show as percentage
            plt.xticks(rotation=20, ha="right")
            plt.ylabel("Drop Rate (%)"); plt.title("Drop Rate by Scenario (Lower is Better)")
            plt.grid(axis='y', linestyle='--', alpha=0.7)
            
            for bar in bars:
                yval = bar.get_height()
                plt.text(bar.get_x() + bar.get_width()/2.0, yval,
                         f'{yval:.3f}%',
                         va='bottom',
                         ha='center')
            
            plt.tight_layout(); plt.savefig(os.path.join(args.plot, "drop_rate.png")); plt.close()
            
            print(f"Plots saved to {args.plot}")

        except FileNotFoundError:
            print(f"Error: Aggregated metrics file not found at {args.metrics}", file=sys.stderr)
            sys.exit(1)
        except Exception as e:
            print(f"Error during plotting: {e}", file=sys.stderr)
            sys.exit(1)

    # --- Error: Invalid combination ---
    else:
        print("Error: Invalid combination of arguments.", file=sys.stderr)
        ap.print_help()
        sys.exit(1)

if __name__ == "__main__":
    main()