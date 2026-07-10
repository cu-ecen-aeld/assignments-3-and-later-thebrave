# Makefile for aesdsocket.c
# Supports cross-compilation. Usage: make CROSS_COMPILE=aarch64-none-linux-gnu-

# Use the CROSS_COMPILE prefix for the compiler if provided, otherwise default to gcc
CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Werror
LDFLAGS ?=

# The name of the final executable
TARGET = aesdsocket

# Default target
all: $(TARGET)

# Rule to link the object files into the final executable
$(TARGET): $(TARGET).o
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).o $(LDFLAGS)

# Rule to compile the source file into an object file
$(TARGET).o: $(TARGET).c
	$(CC) $(CFLAGS) -c $(TARGET).c -o $(TARGET).o

# Clean target to remove the executable and object files
clean:
	rm -f $(TARGET) *.o

# Declare targets that aren't files
.PHONY: all clean
