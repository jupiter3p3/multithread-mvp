#!/usr/bin/env bash
set -euo pipefail

BIN=./bin/mvp
RAW=./metrics/raw.csv
OUT=./metrics/metrics.csv
PLOT_DIR=./plots
CALC=./scripts/metrics_calc.py

REPS=${REPS:-3}             # repeat times（def 3）
DUR=${DUR:-10}              # per run time(s)（def 10）
WORK=${WORK:-0}             # consumer work bytes（def 0）

mkdir -p "$(dirname "$OUT")" "$PLOT_DIR"

build() {
  make -j
}

one_run() {
  local scen="$1"; shift
  local idx="$1"; shift
  local lat="latencies_ns_${scen}_${idx}.txt"
  local st="stats_${scen}_${idx}.txt"
  $BIN "$@" --emit-latencies "$lat" --stats "$st"
  python3 "$CALC" --latencies "$lat" --stats "$st" \
    --append "$RAW" --scenario "$scen" --rep "$idx"
}

run_scenario() {
  local scen="$1"; shift
  echo "[run] scenario=$scen (REPS=$REPS)"
  for ((i=1;i<=REPS;i++)); do
    one_run "$scen" "$i" "$@"
  done
}

aggregate_and_plot() {
  python3 "$CALC" --aggregate "$RAW" --out "$OUT"
  python3 "$CALC" --plot "$PLOT_DIR" --metrics "$OUT"
}

main() {
  build
  # init raw csv
  [[ -f "$RAW" ]] || echo "scenario,rep,latency_p50_ms,latency_p95_ms,latency_p99_ms,throughput_ops_s,drop_rate" > "$RAW"

    run_scenario baseline_pi_1000      --duration "$DUR" --prod-interval-us 1000 --queue-cap 64 --work-bytes "$WORK"
    run_scenario affinity_pi_1000      --duration "$DUR" --prod-interval-us 1000 --queue-cap 64 --pin-consumer 1 --work-bytes "$WORK"
    run_scenario queue_cap_8_pi_1000   --duration "$DUR" --prod-interval-us 1000 --queue-cap 8  --work-bytes "$WORK"
    run_scenario baseline_pi_0      --duration "$DUR" --prod-interval-us 0 --queue-cap 64 --work-bytes "$WORK"
    run_scenario affinity_pi_0      --duration "$DUR" --prod-interval-us 0 --queue-cap 64 --pin-consumer 1 --work-bytes "$WORK"
    run_scenario queue_cap_8_pi_0   --duration "$DUR" --prod-interval-us 0 --queue-cap 8  --work-bytes "$WORK"

  aggregate_and_plot
  echo "[done] metrics @ $OUT; plots @ $PLOT_DIR"
}

main "$@"
