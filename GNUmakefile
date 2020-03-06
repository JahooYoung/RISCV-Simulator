CXX := g++
CXXFLAGS := -Wall -O2 -std=c++11 -MD
LFLAGS := -O2
PREFIX := build
TARGET := simulator

RISCV_CC := riscv64-unknown-elf-gcc
SAMPLE_PREFIX := samples

SRCS := $(wildcard *.cpp)
OBJS := $(SRCS:.cpp=.o)
OBJS := $(addprefix $(PREFIX)/, $(OBJS))

$(shell mkdir -p $(PREFIX))

.PHONY: all clean
all: $(PREFIX)/$(TARGET)

$(PREFIX)/$(TARGET): $(OBJS)
	$(CXX) $(LFLAGS) -o $@ $^

$(PREFIX)/%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(PREFIX)/.deps: $(wildcard $(PREFIX)/*.d)
	@perl mergedep.pl $@ $^

-include $(PREFIX)/.deps

clean:
	rm -rf $(PREFIX)

run-%: $(SAMPLE_PREFIX)/% $(PREFIX)/$(TARGET)
	$(PREFIX)/$(TARGET) $<

sample-%: $(SAMPLE_PREFIX)/%

$(SAMPLE_PREFIX)/%: $(SAMPLE_PREFIX)/%.c
	$(RISCV_CC) -Wa,-march=rv64i -o $(SAMPLE_PREFIX)/$* $<
