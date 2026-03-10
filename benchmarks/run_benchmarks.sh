#!/bin/bash
# run_benchmarks.sh
# Compiles and runs both benchmarks

set -e

mkdir -p results

echo ""
echo "════════════════════════════════════════════"
echo "  Compiling..."
echo "════════════════════════════════════════════"

gcc -O2 -o counter_bench counter_bench.c -lpthread -lnuma
echo "  counter_bench ✓"

gcc -O2 -o pascal_bench pascal_bench.c -lpthread -lnuma
echo "  pascal_bench  ✓"

echo ""
echo "════════════════════════════════════════════"
echo "  Running Counter Benchmark..."
echo "════════════════════════════════════════════"
./counter_bench

echo ""
echo "════════════════════════════════════════════"
echo "  Running Pascal Benchmark..."
echo "════════════════════════════════════════════"
./pascal_bench

echo ""
echo "════════════════════════════════════════════"
echo "  Done! Results in results/"
ls -lh results/
echo "════════════════════════════════════════════"
