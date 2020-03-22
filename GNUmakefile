CXX := g++
CXXFLAGS := -Wall -O3 -std=c++17 -MD
LFLAGS := -O3
PREFIX := build
TARGET := $(PREFIX)/simulator

RISCV_CC := riscv64-unknown-elf-gcc
RISCV_AR := riscv64-unknown-elf-ar
RISCV_OBJDUMP := riscv64-unknown-elf-objdump
SAMPLE_PREFIX := samples

LIB_PREFIX := $(PREFIX)/lib
LIB := $(LIB_PREFIX)/libtiny.a

SRCS := $(wildcard *.cpp)
OBJS := $(SRCS:.cpp=.o)
OBJS := $(addprefix $(PREFIX)/, $(OBJS))

$(shell mkdir -p $(LIB_PREFIX))

.PHONY: all clean
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(LFLAGS) -o $@ $^

$(PREFIX)/%.o: %.cpp
	$(CXX) $(CXXFLAGS) -Iinclude -c -o $@ $<

$(PREFIX)/.deps: $(wildcard $(PREFIX)/*.d)
	@perl mergedep.pl $@ $^

-include $(PREFIX)/.deps

clean:
	rm -rf $(PREFIX)

.PRECIOUS: $(SAMPLE_PREFIX)/% $(SAMPLE_PREFIX)/%.S

run-%-info: $(SAMPLE_PREFIX)/% $(TARGET)
	$(TARGET) -i $<-info.txt $<

run-%: $(SAMPLE_PREFIX)/% $(TARGET)
	$(TARGET) $<

run-%-v: $(SAMPLE_PREFIX)/% $(TARGET)
	$(TARGET) -v $< > samples/output.txt

srun-%: $(SAMPLE_PREFIX)/% $(TARGET)
	$(TARGET) -s $<

sample-%: $(SAMPLE_PREFIX)/%
	@:

$(SAMPLE_PREFIX)/%: $(SAMPLE_PREFIX)/%.c $(LIB)
	$(RISCV_CC) -Iinclude -O2 -Wa,-march=rv64imc -static -o $(SAMPLE_PREFIX)/$* $< -L $(LIB_PREFIX) -ltiny
	$(RISCV_OBJDUMP) -S $(SAMPLE_PREFIX)/$* > $(SAMPLE_PREFIX)/$*.S


# library

LIB_SRCS := $(wildcard lib/*.c)
LIB_OBJS := $(LIB_SRCS:.c=.o)
LIB_OBJS := $(addprefix $(PREFIX)/, $(LIB_OBJS))

$(LIB): $(LIB_OBJS)
	$(RISCV_AR) -crv $@ $^

$(LIB_PREFIX)/%.o: lib/%.c
	$(RISCV_CC) -O3 -MD -Iinclude -c -o $@ $<

$(LIB_PREFIX)/.deps: $(wildcard $(LIB_PREFIX)/*.d)
	@perl mergedep.pl $@ $^

-include $(LIB_PREFIX)/.deps
