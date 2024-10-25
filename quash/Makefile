# Makefile for Quash

# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -g

# Output executable
OUTPUT = quash

# Source file inside the src directory
SRCS = src/quash.c

# Default target
all: $(OUTPUT)

# Rule to build the quash executable
$(OUTPUT): $(SRCS)
	$(CC) $(CFLAGS) -o $(OUTPUT) $(SRCS)

# Clean up
clean:
	rm -f $(OUTPUT)
