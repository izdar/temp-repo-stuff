# Makefile for FTP Trace Replayer
# Build: make
# Clean: make clean

CC = gcc
CFLAGS = -Wall -Wextra -O2

TARGET = ftp_trace_replay
SRCS = ftp_trace_replay.c ftp_predicate_adapter.c
HDRS = ftp_predicate_adapter.h

all: $(TARGET)

$(TARGET): $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
