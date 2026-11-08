# SPDX-License-Identifier: MIT

CXX      ?= g++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -pedantic
BIN      := bin

all: $(BIN)/halin_td $(BIN)/gen_halin $(BIN)/td_validate $(BIN)/test_recognition

$(BIN):
	mkdir -p $(BIN)

$(BIN)/halin_td: src/halin_td.cpp src/eppstein.cpp src/htd.cpp src/*.hpp | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ src/halin_td.cpp src/eppstein.cpp src/htd.cpp

$(BIN)/gen_halin: tools/gen_halin.cpp src/graph_io.hpp | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ tools/gen_halin.cpp

$(BIN)/td_validate: tools/td_validate.cpp src/graph_io.hpp | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ tools/td_validate.cpp

$(BIN)/test_recognition: tools/test_recognition.cpp src/eppstein.cpp src/htd.cpp src/*.hpp | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ tools/test_recognition.cpp src/eppstein.cpp src/htd.cpp

test: all
	scripts/run_limited.sh $(BIN)/test_recognition 20000 1

clean:
	rm -rf $(BIN)

.PHONY: all test clean
