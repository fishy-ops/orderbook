CXX ?= clang++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pthread

all: test bench
test: test.cpp engine.h reference.h common.h
	$(CXX) $(CXXFLAGS) $< -o $@
bench: bench.cpp engine.h reference.h common.h spsc_queue.h
	$(CXX) $(CXXFLAGS) $< -o $@
check: test
	./test
run: bench
	./bench 1000000
clean:
	rm -f test bench
