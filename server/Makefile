# Makefile for aesdsocket.c
# Supports cross-compilation. Usage: make CROSS_COMPILE=aarch64-none-linux-gnu-

# Use the CROSS_COMPILE prefix for the compiler if provided, otherwise default to gcc
CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -g -Wall -Werror
LDFLAGS ?=

# The name of the final executable
TARGET = aesdsocket

# Default target
all: $(TARGET)

# Rule to link the object files into the final executable
$(TARGET): $(TARGET).o thr_cnx.o thr_tim.o util.o
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).o thr_cnx.o thr_tim.o util.o $(LDFLAGS)

# Rule to compile the source file into an object file
$(TARGET).o: $(TARGET).c
	$(CC) $(CFLAGS) -c $(TARGET).c -o $(TARGET).o

thr_cnx.o: thr_cnx.c
	$(CC) $(CFLAGS) -c thr_cnx.c -o thr_cnx.o

thr_tim.o: thr_tim.c
	$(CC) $(CFLAGS) -c thr_tim.c -o thr_tim.o

util.o: util.c
	$(CC) $(CFLAGS) -c util.c -o util.o

# Clean target to remove the executable and object files
clean:
	rm -f $(TARGET) *.o

# Declare targets that aren't files
.PHONY: all clean
