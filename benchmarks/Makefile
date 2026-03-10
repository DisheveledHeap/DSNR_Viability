# Makefile to compile and run run_benchmarks.c with libnuma

# Compiler
CC = gcc

# Compiler flags
CFLAGS = -O2 -Wall -pthread

# Linker flags
LDFLAGS = -lpthread -lnuma

.PHONY:
pascal_comp:
	echo "\n════════════════════════════════════════════"
	echo "Compiling Pascal Benchmark"
	echo "════════════════════════════════════════════"
	$(CC) $(CFLAGS) -o pascal_bench pascal_bench.c $(LDFLAGS)

.PHONY:
counter_comp:
	echo "\n════════════════════════════════════════════"
	echo "Compiling Counter Benchmark"
	echo "════════════════════════════════════════════"
	$(CC) $(CFLAGS) -o counter_bench counter_bench.c $(LDFLAGS)

.PHONY:
pascal: pascal_comp
	echo "\n════════════════════════════════════════════"
	echo "Running Pascal Benchmark"
	echo "════════════════════════════════════════════"
	./pascal_bench

.PHONY:
pascal_r: pascal_comp
	echo "\n════════════════════════════════════════════"
	echo "Running Pascal Benchmark"
	echo "════════════════════════════════════════════"
	./pascal_bench -r

.PHONY:
counter: counter_comp
	echo "\n════════════════════════════════════════════"
	echo "Running Counter Benchmark"
	echo "════════════════════════════════════════════"
	./counter_bench

.PHONY:
counter_r: counter_comp
	echo "\n════════════════════════════════════════════"
	echo "Running Counter Benchmark"
	echo "════════════════════════════════════════════"
	./counter_bench -r

.PHONY:
no_rep: pascal counter

.PHONY:
rep: pascal_r counter_r

# Default target: build and run
.PHONY: all
all: echo "\n════════════════════════════════════════════\nRunning All Benchmarks\n════════════════════════════════════════════"
	pascal
	counter

# Clean
.PHONY: clean
clean:
	rm -f pascal_bench counter_bench