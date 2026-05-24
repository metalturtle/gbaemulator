CXX ?= g++
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic -O2 -g
CPPFLAGS ?= -Icore/include

CORE_SRCS := \
	core/src/bus.cpp \
	core/src/cartridge.cpp \
	core/src/cpu.cpp \
	core/src/emulator.cpp \
	core/src/gba_c_api.cpp \
	core/src/interrupts.cpp \
	core/src/keypad.cpp \
	core/src/ppu.cpp \
	core/src/scheduler.cpp \
	core/src/timers.cpp

TEST_SRCS := \
	tests/test_main.cpp

.PHONY: all test clean

all: build/gba_tests

build:
	mkdir -p build

build/gba_tests: $(CORE_SRCS) $(TEST_SRCS) | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

test: build/gba_tests
	./build/gba_tests

clean:
	rm -rf build
