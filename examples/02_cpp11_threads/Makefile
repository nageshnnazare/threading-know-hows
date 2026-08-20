# Makefile for std::thread examples.
#
# Most files build with -std=c++17. The examples in 08_jthread_cpp20/
# need -std=c++20.

CXX        ?= c++
CXXFLAGS17 ?= -std=c++17 -O2 -g -Wall -Wextra -pthread
CXXFLAGS20 ?= -std=c++20 -O2 -g -Wall -Wextra -pthread

C17_DIRS := 01_basics 02_mutex 03_condition_variables 04_atomics 05_futures 06_call_once 07_thread_local
C20_DIRS := 08_jthread_cpp20

C17_SRCS := $(foreach d,$(C17_DIRS),$(wildcard $(d)/*.cpp))
C20_SRCS := $(foreach d,$(C20_DIRS),$(wildcard $(d)/*.cpp))

C17_BINS := $(C17_SRCS:.cpp=)
C20_BINS := $(C20_SRCS:.cpp=)

ALL_BINS := $(C17_BINS) $(C20_BINS)

.PHONY: all clean run
all: $(ALL_BINS)

$(C17_BINS): %: %.cpp
	$(CXX) $(CXXFLAGS17) -o $@ $<

$(C20_BINS): %: %.cpp
	$(CXX) $(CXXFLAGS20) -o $@ $<

clean:
	rm -f $(ALL_BINS)

run: all
	@for b in $(ALL_BINS); do \
	    echo ""; echo "================================================="; \
	    echo "==  $$b"; echo "================================================="; \
	    $$b || true; \
	done
