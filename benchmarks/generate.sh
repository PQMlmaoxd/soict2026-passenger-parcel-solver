#!/usr/bin/env bash
# Regenerate the benchmark instances listed in mimic-configs.tsv.
#
#   benchmarks/generate.sh [OUTPUT_DIR [NAME_PATTERN]]
#
# OUTPUT_DIR defaults to build/benchmarks. NAME_PATTERN is an extended regular
# expression matched against the configuration name, for example 's1_0[1-4]'
# for the first four. The generator is deterministic, so every run yields the
# same bytes.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
out="${1:-$root/build/benchmarks}"
pattern="${2:-.}"

cargo build --release --manifest-path "$root/tools/Cargo.toml" --bin gen_instance
exe="$root/tools/target/release/gen_instance"
[ -x "$exe" ] || exe="$exe.exe"

mkdir -p "$out"
count=0
while IFS=$'\t' read -r name taxis passengers parcels seed layout horizon window asymmetry; do
  "$exe" "$taxis" "$passengers" "$parcels" "$seed" "$layout" "$horizon" "$window" "$asymmetry" \
    > "$out/$name.in"
  count=$((count + 1))
done < <(tr -d '\r' < "$root/benchmarks/mimic-configs.tsv" | awk -F'\t' -v re="$pattern" 'NR > 1 && $1 ~ re')
echo "wrote $count instances to $out"
