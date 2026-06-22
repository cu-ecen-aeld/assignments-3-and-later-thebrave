# Makefile for writer.c
# Supports cross-compilation. Usage: make CROSS_COMPILE=aarch64-none-linux-gnu-

# Use the CROSS_COMPILE prefix for the compiler if provided, otherwise default to gcc
CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Werror
LDFLAGS ?=

# The name of the final executable
TARGET = writer

# Default target
all: $(TARGET)

# Rule to link the object files into the final executable
$(TARGET): writer.o
	$(CC) $(CFLAGS) -o $(TARGET) writer.o $(LDFLAGS)

# Rule to compile the source file into an object file
writer.o: writer.c
	$(CC) $(CFLAGS) -c writer.c -o writer.o

# Clean target to remove the executable and object files
clean:
	rm -f $(TARGET) *.o

# Declare targets that aren't files
.PHONY: all clean
