#!/usr/bin/env bash
set -euo pipefail

# Usage: bash scripts/profile.sh [stat|record] [batches] [orders_per_batch]
mode=${1:-stat}
batches=${2:-200000}
orders=${3:-100}
if [[ $# -gt 3 || ( "$mode" != stat && "$mode" != record ) ]]; then
    echo 'Usage: bash scripts/profile.sh [stat|record] [batches=200000] [orders_per_batch=100]' >&2
    exit 2
fi
for value in "$batches" "$orders"; do
    if [[ ! "$value" =~ ^[1-9][0-9]*$ ]]; then
        echo 'Workload arguments must be positive decimal integers.' >&2
        exit 2
    fi
done
command -v perf >/dev/null || { echo 'Linux perf is required.' >&2; exit 1; }
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
build_dir="$project_dir/build-perf"
output_dir="$build_dir/profile/$mode"
cmake -S "$project_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DBUILD_PROFILING=ON -DBUILD_TESTING=OFF -DBUILD_BENCHMARKS=OFF \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build "$build_dir" --target clob_profile -j 4
mkdir -p "$output_dir"

{
    date -u '+date_utc=%Y-%m-%dT%H:%M:%SZ'
    printf 'mode=%s\nbatches=%s\norders_per_batch=%s\n' "$mode" "$batches" "$orders"
    printf 'commit='
    git -C "$project_dir" rev-parse HEAD
    git -C "$project_dir" status --short
    uname -a
    perf --version
    lscpu
} > "$output_dir/metadata.txt"
# Keep exact sources and cache with each result, even for an uncommitted tree.
tar -C "$project_dir" -czf "$output_dir/source.tar.gz" CMakeLists.txt include src profiling scripts
cp "$build_dir/CMakeCache.txt" "$output_dir/CMakeCache.txt"
cp "$build_dir/compile_commands.json" "$output_dir/compile_commands.json"
cp "$build_dir/clob_profile" "$output_dir/clob_profile"

if [[ "$mode" == stat ]]; then
    perf stat -r 5 \
        -e task-clock,cycles:u,instructions:u,branches:u,branch-misses:u,cache-references:u,cache-misses:u \
        -o "$output_dir/stat.txt" -- "$output_dir/clob_profile" "$batches" "$orders" \
        > "$output_dir/workload.txt"
    cat "$output_dir/stat.txt"
else
    perf record -e cycles:u -F 499 --call-graph dwarf \
        -o "$output_dir/perf.data" -- "$output_dir/clob_profile" "$batches" "$orders" \
        > "$output_dir/workload.txt"
    perf report --stdio --no-children --percent-limit 1 \
        -i "$output_dir/perf.data" > "$output_dir/report.txt"
    cat "$output_dir/report.txt"
fi
printf '\nResults saved to %s\n' "$output_dir"
