# Delegates to each example section.

DIRS := 01_pthreads 02_cpp11_threads 03_patterns 04_pitfalls 05_memory_model 06_openmp

.PHONY: all clean run $(DIRS)

all:  $(DIRS:%=build-%)
clean: $(DIRS:%=clean-%)
run:  $(DIRS:%=run-%)

build-%:
	$(MAKE) -C $*

clean-%:
	$(MAKE) -C $* clean

run-%:
	$(MAKE) -C $* run
