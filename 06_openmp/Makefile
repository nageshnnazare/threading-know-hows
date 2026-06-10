# OpenMP examples Makefile.
#
# Apple Clang doesn't ship libomp; install via 'brew install libomp' and
# the OPENMP_FLAGS auto-detection below should kick in. If your compiler
# truly doesn't support OpenMP we fall back to plain compilation; each
# program then prints a friendly message.

CC      ?= cc
CFLAGS  ?= -O2 -g -Wall -Wextra -std=c11

# Detect OpenMP. Prefer -fopenmp; on Apple Clang try -Xpreprocessor.
HAVE_OMP := $(shell echo 'int main(){}' | $(CC) -fopenmp -xc - -o /dev/null 2>/dev/null && echo 1)
ifeq ($(HAVE_OMP),1)
  OMPFLAGS := -fopenmp
else
  OMPFLAGS :=
endif

SRCS := $(wildcard *.c)
BINS := $(SRCS:.c=)

.PHONY: all clean run
all: $(BINS)

%: %.c
	$(CC) $(CFLAGS) $(OMPFLAGS) -o $@ $<

clean:
	rm -f $(BINS)

run: all
	@for b in $(BINS); do echo ""; echo "== $$b =="; ./$$b || true; done
