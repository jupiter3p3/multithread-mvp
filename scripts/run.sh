#!/usr/bin/env bash
set -euo pipefail

BIN=./bin/mvp
METRICS=./metrics/metrics.csv
PLOT_DIR=./plots
CALC=./scripts/metrics_calc.py

mkdir -p "$(dirname "$METRICS")" "$PLOT_DIR"

build() {
  make -j
}

run_scenario() {
  local name="$1"; shift
  echo "[run] $name ..."
  # each latency (ns) to tmp file
  $BIN "$@" --emit-latencies ./latencies_ns.txt
  # cal data to metrics.csv
  python3 "$CALC" --latencies ./latencies_ns.txt --append "$METRICS" --scenario "$name"
}

plots() {
  python3 "$CALC" --plot "$PLOT_DIR" --metrics "$METRICS"
}

main() {
  build
  # init metrics.csv
  [[ -f "$METRICS" ]] || echo "scenario,latency_p50_ms,latency_p95_ms,latency_p99_ms,throughput_ops_s,drop_rate" > "$METRICS"

  # 3 scenario
  # run_scenario baseline_pi_1000                --duration 10s --prod-interval-us 1000  --queue-cap 64
  # run_scenario affinity_pi_1000                --duration 10s --prod-interval-us 1000  --queue-cap 64 --pin-consumer 1
  # run_scenario queue_cap_8_pi_1000             --duration 10s --prod-interval-us 1000  --queue-cap 8

  plots
  echo "[done] metrics @ $METRICS; plots @ $PLOT_DIR"
}

main "$@"
