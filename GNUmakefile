CXX := g++
CXXFLAGS := -Wall -O2 -std=c++17 -MD
LFLAGS := -O2
PREFIX := build
TARGET := simulator

RISCV_CC := riscv64-unknown-elf-gcc
RISCV_OBJDUMP := riscv64-unknown-elf-objdump
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
	$(CXX) $(CXXFLAGS) -Iinclude -c -o $@ $<

$(PREFIX)/.deps: $(wildcard $(PREFIX)/*.d)
	@perl mergedep.pl $@ $^

-include $(PREFIX)/.deps

clean:
	rm -rf $(PREFIX)

.PRECIOUS: $(SAMPLE_PREFIX)/% $(SAMPLE_PREFIX)/%.S

run-%-info: $(SAMPLE_PREFIX)/% $(PREFIX)/$(TARGET)
	$(PREFIX)/$(TARGET) -i $<-info.txt $<

run-%: $(SAMPLE_PREFIX)/% $(PREFIX)/$(TARGET)
	$(PREFIX)/$(TARGET) -v $< > samples/output.txt

srun-%: $(SAMPLE_PREFIX)/% $(PREFIX)/$(TARGET)
	$(PREFIX)/$(TARGET) -s $<

sample-%: $(SAMPLE_PREFIX)/%
	@:

$(SAMPLE_PREFIX)/%: $(SAMPLE_PREFIX)/%.c
	$(RISCV_CC) -Wa,-march=rv64imc -o $(SAMPLE_PREFIX)/$* $<
	$(RISCV_OBJDUMP) -d $(SAMPLE_PREFIX)/$* > $(SAMPLE_PREFIX)/$*.S
