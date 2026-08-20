CXX      ?= c++
CXXFLAGS ?= -std=c++17 -O2 -g -Wall -Wextra -pthread $(CXXFLAGS_EXTRA)

SRCS := $(wildcard *.cpp)
BINS := $(SRCS:.cpp=)

.PHONY: all clean run
all: $(BINS)

%: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(BINS)

run: all
	@for b in $(BINS); do echo ""; echo "== $$b =="; ./$$b || true; done
