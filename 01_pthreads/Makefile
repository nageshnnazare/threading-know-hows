# Makefile for pthread examples.
#
# Usage:
#   make            # build everything
#   make run        # run them in order
#   make clean

CC      ?= cc
CFLAGS  ?= -O2 -g -Wall -Wextra -pthread -std=c11

# Find every .c under any subdir.
SRCS := $(wildcard 01_basics/*.c) \
        $(wildcard 02_mutex/*.c) \
        $(wildcard 03_condition_variables/*.c) \
        $(wildcard 04_rwlock/*.c) \
        $(wildcard 05_barriers/*.c) \
        $(wildcard 06_semaphores/*.c) \
        $(wildcard 07_spinlock/*.c) \
        $(wildcard 08_thread_specific/*.c) \
        $(wildcard 09_cancellation/*.c)

BINS := $(SRCS:.c=)

.PHONY: all clean run

all: $(BINS)

# Generic compile rule (in-place: leave the binary next to the source).
%: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(BINS)

# Run every example sequentially, with a banner.
run: all
	@for b in $(BINS); do \
	    echo ""; echo "================================================="; \
	    echo "==  $$b"; echo "================================================="; \
	    $$b || true; \
	done
