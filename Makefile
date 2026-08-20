# Top-level Makefile: all runnable code lives under examples/.

.PHONY: all clean run

all:
	$(MAKE) -C examples

clean:
	$(MAKE) -C examples clean

run:
	$(MAKE) -C examples run
