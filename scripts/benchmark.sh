#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Generates the four instance groups of Section 6 of arXiv:2506.02346
# (10 instances each), runs halin_td on every instance and validates the
# output. Every program run is limited to 32 GB of memory and 10 minutes.
#
#   scripts/benchmark.sh [instances-per-group] [data-dir]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RUN="$ROOT/scripts/run_limited.sh"
BIN="$ROOT/bin"
PER_GROUP="${1:-10}"
DATA="${2:-$ROOT/data}"
mkdir -p "$DATA"
RESULTS="$DATA/results.csv"
echo "group,instance,n,m,time_recognition,time_htd,time_total_wall,width,valid" > "$RESULTS"

declare -A LO=([small]=100 [medium]=1000 [large]=10000 [giant]=100000)
declare -A HI=([small]=999 [medium]=9999 [large]=99999 [giant]=999999)

seed=1
failures=0
for group in small medium large giant; do
  for ((k = 1; k <= PER_GROUP; k++)); do
    name="${group}_$(printf %02d "$k")"
    gr="$DATA/$name.gr"; td="$DATA/$name.td"
    [[ -f "$gr" ]] || "$RUN" "$BIN/gen_halin" "${LO[$group]}" "${HI[$group]}" "$seed" -o "$gr"
    seed=$((seed + 1))
    start=$(date +%s.%N)
    stats=$("$RUN" "$BIN/halin_td" --stats -o "$td" "$gr" 2>&1 >/dev/null)
    end=$(date +%s.%N)
    wall=$(awk -v a="$start" -v b="$end" 'BEGIN { print b - a }')
    n=$(sed -n 's/.*n=\([0-9]*\) m=\([0-9]*\).*/\1/p' <<<"$stats")
    m=$(sed -n 's/.*n=\([0-9]*\) m=\([0-9]*\).*/\2/p' <<<"$stats")
    trec=$(sed -n 's/.*time_recognition=\([0-9.]*\).*/\1/p' <<<"$stats")
    thtd=$(sed -n 's/.*time_htd=\([0-9.]*\).*/\1/p' <<<"$stats")
    width=$(sed -n 's/.*width=\([0-9]*\).*/\1/p' <<<"$stats")
    if vout=$("$RUN" "$BIN/td_validate" "$gr" "$td" --expect-width 3 2>&1); then
      valid=yes
      rm -f "$td"
    else
      # keep the decomposition for inspection; the run fails at the end
      valid=NO
      failures=$((failures + 1))
      echo "error: $name: validation failed (kept $td): $vout" >&2
    fi
    printf '%s,%s,%s,%s,%s,%s,%.4f,%s,%s\n' "$group" "$name" "$n" "$m" "$trec" "$thtd" "$wall" "$width" "$valid" | tee -a "$RESULTS"
  done
done

echo
echo "group averages (seconds):"
awk -F, 'NR > 1 { c[$1]++; r[$1] += $5; h[$1] += $6; w[$1] += $7; n[$1] += $3; if ($9 != "yes") bad[$1]++ }
  END { printf "%-8s %10s %14s %10s %12s %8s\n", "group", "avg_n", "recognition", "H-Td", "wall_total", "invalid";
        split("small medium large giant", gs, " ");
        for (i = 1; i <= 4; i++) { g = gs[i]; if (!c[g]) continue;
          printf "%-8s %10.0f %14.6f %10.6f %12.4f %8d\n", g, n[g]/c[g], r[g]/c[g], h[g]/c[g], w[g]/c[g], bad[g] + 0 } }' "$RESULTS"

if ((failures > 0)); then
  echo "error: $failures instance(s) failed validation" >&2
  exit 1
fi
