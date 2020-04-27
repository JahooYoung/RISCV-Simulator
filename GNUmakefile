CXX := g++
CXXFLAGS := -Wall -O3 -std=c++17 -Ithird_party -Iinclude
LFLAGS := -lyaml-cpp
PREFIX := build
TARGET := $(PREFIX)/simulator
SRC_DIR := src
LIB_DIR := lib
SAMPLE_PREFIX := samples

RISCV_CC := riscv64-unknown-elf-gcc
RISCV_CFLAGS := -Iinclude -O2 -Wa,-march=rv64imc
RISCV_AR := riscv64-unknown-elf-ar
RISCV_OBJDUMP := riscv64-unknown-elf-objdump

LIB_PREFIX := $(PREFIX)/lib
LIB := $(LIB_PREFIX)/libtiny.a

SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(SRCS:.cpp=.o)
OBJS := $(notdir $(OBJS))
OBJS := $(addprefix $(PREFIX)/, $(OBJS))

$(shell mkdir -p $(LIB_PREFIX))

.PHONY: all clean
all: $(TARGET) $(LIB)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $^ $(LFLAGS)

$(PREFIX)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -MD -c -o $@ $<

$(PREFIX)/.deps: $(wildcard $(PREFIX)/*.d)
	@perl mergedep.pl $@ $^

-include $(PREFIX)/.deps

clean:
	rm -rf $(PREFIX)

.PRECIOUS: $(SAMPLE_PREFIX)/% $(SAMPLE_PREFIX)/%.S

run-%-info: $(SAMPLE_PREFIX)/% $(TARGET)
	$(TARGET) -i $<-info.txt $<

run-%: $(SAMPLE_PREFIX)/% $(TARGET)
	$(TARGET) $< $(ELF_ARGS)

run-%-v: $(SAMPLE_PREFIX)/% $(TARGET)
	$(TARGET) -v $< > samples/output.txt

srun-%: $(SAMPLE_PREFIX)/% $(TARGET)
	$(TARGET) -s $< $(ELF_ARGS)

sample-%: $(SAMPLE_PREFIX)/%
	@:

$(SAMPLE_PREFIX)/%: $(SAMPLE_PREFIX)/%.c $(LIB)
	$(RISCV_CC) $(RISCV_CFLAGS) -o $(SAMPLE_PREFIX)/$* $< -static -L $(LIB_PREFIX) -ltiny
	$(RISCV_OBJDUMP) -S $(SAMPLE_PREFIX)/$* > $(SAMPLE_PREFIX)/$*.S


# library

LIB_SRCS := $(wildcard $(LIB_DIR)/*.c)
LIB_OBJS := $(LIB_SRCS:.c=.o)
LIB_OBJS := $(addprefix $(PREFIX)/, $(LIB_OBJS))

$(LIB): $(LIB_OBJS)
	$(RISCV_AR) -crv $@ $^

$(LIB_PREFIX)/%.o: $(LIB_DIR)/%.c
	$(RISCV_CC) $(RISCV_CFLAGS) -MD -c -o $@ $<

$(LIB_PREFIX)/.deps: $(wildcard $(LIB_PREFIX)/*.d)
	@perl mergedep.pl $@ $^

-include $(LIB_PREFIX)/.deps
