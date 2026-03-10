# Makefile to compile and run run_benchmarks.c with libnuma

# Compiler
CC = gcc

# Source file
SRC = run_benchmarks.c

# Output executable
OUT = run_benchmarks

# Compiler flags
CFLAGS = -O2 -Wall -pthread

# Linker flags
LDFLAGS = -lnuma

# Default target: build and run
.PHONY: all
all: $(OUT)
	./$(OUT)

# Build the executable
$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $(OUT) $(SRC) $(LDFLAGS)

# Clean
.PHONY: clean
clean:
	rm -f $(OUT)