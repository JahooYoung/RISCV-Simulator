# 体系结构实习 lab2 - RISCV Simulator

by Jahoo

这是一个RISCV的五阶段流水线功能及性能模拟器。该模拟器有如下主要功能：

- 支持RV64IMC指令集
- 程序运行后可输出动态指令数，周期数及其他性能相关信息
- 可深度配置不同运算、系统调用、访存等所需的周期数
- 可任意配置缓存的层次、大小、命中时间、写策略等参数
- 自带一个精简的库tinylib
- 可选择是否启用数据前递
- 支持不同的转移预测策略
- 支持命令行参数
- 单步调试模式，支持反汇编、断点、打印寄存器、打印内存

## 使用说明

### 环境及依赖

- 建议在Ubuntu 18.04下编译

- g++（需支持c++17，推荐版本7.4.0或以上）
- make
- riscv-gnu-toolchain（包括riscv64-unknown-elf-gcc，riscv64-unknown-elf-ar，riscv64-unknown-elf-objdump）

### 编译

项目内已有`GNUmakefile`，直接运行命令`make`即可编译模拟器和库函数。编译出来的模拟器可执行文件是`build/simulator`，库文件是`build/lib/libtiny.a`。

如果`g++`或`riscv64-unknown-elf-gcc`等编译器工具链的路径需要指定，请修改`GNUmakefile`中的相应变量。

### 运行

运行`./build/simulator --help`可查看用法及命令行参数：

```
Usage: ./build/simulator [options] elf_file [args...]

Options:
  -h, --help               Print this help
  -c, --config config_file Specify the configuration file,
                           default is 'default_config.json'
  -s                       Single step mode
  -i, --info info_file     Output filename of Elf information
  -v                       Verbose mode
```

运行该模拟器**需要有配置文件**。配置文件是JSON格式，默认是项目中已提供的`default_config.json`。配置文件的内容及说明请见"配置文件说明"一节。

该模拟器的输入是RISCV格式并静态链接`libtiny`的ELF文件。因此编译前应确保源代码只包含一个头文件`tinylib.h`，并**确保源代码没有使用其他库函数**（`riscv64-unknown-elf-gcc`可能会默认链接glibc/newlib的标准库函数，tinylib的库函数列表见“库函数”一节）。编译命令请参考

```
riscv64-unknown-elf-gcc -Iinclude -O2 -Wa,-march=rv64imc -static -o [output_file] [your_source_file] -Lbuild/lib -ltiny
```

`-i`选项会输出ELF文件的相关信息到指定的文件，输出内容包括ELF头、节头、程序头和符号表。

`-v`选项会打印每一步的流水线指令（需要在配置文件中开启反汇编，默认开启）和寄存器内容。**开启后输出内容非常多，只能在运行动态指令数较少的程序时开启。**

#### 便捷指令

为了调试及运行方便，`GNUmakefile`中还提供了一些便捷指令。如`make run-add`，该指令会寻找`samples`目录下的`add.c`文件，编译输出到`samples/add`，然后作为输入调用模拟器。`make srun-add`则是单步模式，其他类似。如果elf文件需要参数，则修改`ELF_ARGS`变量，比如`make run-add ELF_ARGS='1 2'`。

#### 单步模式

单步模式需要加`-s`选项，用法和gdb相似，支持以下指令（部分指令可缩写为前缀）：

- 空指令（直接按回车）：执行上一条指令
- `quit`：退出模拟器
- `set args`：设置程序参数
- `run`：装载并运行ELF文件，若带参数，则用此参数运行，否则以上一次`run`或`set args`设置的参数运行。若要以空参数运行，请用`set args`清空参数
- `kill`：结束程序
- `continue`：继续执行直到遇到下一个断点或结束
- `step`：单步（**直到流水线发生变化，可包括多个周期**）
- `next`：与`step`功能**相同**
- `breakpoint expr`：设置断点，地址为表达式的值
- `info`：打印信息，子命令可以是
  - `registers`：打印所有（32个整数）寄存器的值
  - `breakpoints`：打印已添加的断点
- `print expr`：打印表达式的值
- `x/[n][xdufcs][bhwg] expr`：打印以表达式的值为地址开始的`n`个单位的内存，格式可以是`xdufcs`中的一个（跟printf类似），单位可以是`bhwg`中的一个（分别代表1、2、4、8个字节）

**注1**：目前表达式仅支持非负整数（16进制地址请加`0x`前缀）、寄存器（例如`$sp, $a0`）和符号（函数、全局变量）名

**注2**：单步模式下，**程序运行时**发送SIGINT只会停止正在运行的程序，不会退出模拟器

## 库函数

tinylib有以下库函数：

- `int readint()`：从`stdin`读一个整型
- `printf`：与标准IO库相同
- `malloc, free, calloc, realloc, srand, rand, atoi, isdigit`：与标准库相同
- `long time()`：返回从Epoch以来的秒数

## 配置文件说明

配置文件采用JSON格式，可配置的内容有

- `disassemble`：bool类型，表示是否反汇编
- `objdump`：string类型，表示objdump的路径（若不反汇编则可以忽略该参数）
- `data_forwarding`：bool类型，表示是否进行数据前递
- `branch_predictor`：string类型，表示转移预测策略。可选项有`never_taken`，`always_taken`和`btfnt`（Backward Taken Forward Not Taken）
- `stack_size`：int类型，表示栈大小，单位是KB
- `alu_cycles`：配置ALU不同运算所需周期数，见`default_config.json`。
- `ecall_cycles`：配置不同系统调用所需周期数，见`default_config.json`。
- `memory_cycles`：int类型，表示访问主存所需周期数
- `cache`：数组类型，每个元素代表一个cache，每个cache的配置有
  - `name`：string类型，**必须**，表示cache名称
  - `instruction_entry`：bool类型，标注取指入口。最多只能有1个取指入口，若无，则直接访问主存
  - `data_entry`：bool类型，标注数据读写入口。最多只能有1个数据读写入口，若无，则直接访问主存
  - `size`：int类型，**必须**，表示cache大小，单位为KB
  - `associativity`：int类型，**必须**，表示关联度
  - `cache_line_bytes`：int类型，表示每个cache line的大小，单位为Byte。**必须为2的幂且不小于8**，且组数（`size * 1024 / associativity / cache_line_bytes`）也必须是2的幂。下一级cache的cache line大小必须**大于等于**这一级的大小。默认是64
  - `write_back`：bool类型，表示写命中时是否采用写回策略，默认采用
  - `write_allocate`：bool类型，表示写不命中时是否采用写分配策略，默认采用
  - `hit_cycles`：int类型，**必须**，表示缓存命中时所需周期数
  - `cache_for`：string类型，**必须**，表示下一级缓存/主存

## 实现细节

### 可执行文件的装载、初始化

这部分功能对应`ElfReader`类（见`elf_reader.hpp/cpp`）。

在打开ELF文件后，借助系统库中`elf.h`（已将用到的部分放到`elf.hpp`）的ELF头、节头（section header）、程序头（program header）、符号表（symtab）以及一些辅助宏定义，可以直接用`fread`函数方便地读取ELF文件中的信息。

装载过程也比较简单，按照ELF文件中的每个程序头的信息依次将ELF文件中的内容写入内存即可。要注意的是程序头中有`filesz`和`memsz`之分，`filesz`是ELF文件中该段的内容大小，但内存中该段需要分配`memsz`的大小，多出来的部分需全设成零。

装载后的初始化工作主要包括设置PC（程序头中的`e_entry`，对应`_start`函数），初始化栈和栈指针，通用寄存器清零，流水线寄存器初始化为气泡（bubble）状态和性能计数器清零。

这里重点说一下初始化栈的过程。经过对`riscv64-unknown-elf-gcc`编译出来程序的分析，可以得到一个比较通用的栈布局：

```
/**
  * stack layout
  *
  * +-------------+  <--- STACK_TOP
  * | arg strings |
  * |     ...     |
  * +-------------+
  * |    NULL     |
  * +-------------+
  * | argv[argc-1]|
  * +-------------+
  * |     ...     |
  * +-------------+
  * |   argv[0]   |
  * +-------------+
  * |    argc     |
  * +-------------+  <--- sp (16 bytes aligned)
  */
```

首先按照上面的布局计算sp以及每个部分的起始地址，然后将参数字符串复制到`arg strings`中，并把起始地址放入相应的`argv[]`中，最后将参数数量放到`argc`处（注意是int类型）。要注意的一点是预留`arg strings`的空间时要考虑到每个参数字符串的结束符`\0`。

另外，为了调试方便，我借助`riscv64-unknown-elf-objdump`的输出进行反汇编，并将指令地址和汇编语句存入一个`map`中方便读取和查看。

### 存储系统及其接口

这部分的功能对应`MemorySystem`类（见`memory_system.hpp/cpp`）。

主存我采用虚拟内存抽象，并采用断页式管理，每一页的大小为4KB。页表用C++标准库中的`unordered_map`维护，可以在保证性能的同时减少模拟器所需内存。

`MemorySystem`类提供的接口主要有

- `page_alloc`：按照给定的虚拟地址分配一页，并更新页表。该接口用于分配装载时所需的页以及栈和堆所需的页。
- `read_inst`：从给定的虚拟地址读取指令（4字节），返回指令内容及所需周期数
- `read_data`：从给定的虚拟地址读取数据（可指定字节数），返回数据及所需周期数
- `write_data`：在给定的虚拟地址写入数据（可指定字节数），返回所需周期数

- `sbrk`：按照给定的字节数扩展堆，该接口负责处理`sbrk`系统调用

值得留意的是读写存储系统的接口都会返回该操作所需周期数。这样的接口设计可以让模拟器核心代码无需关注存储系统的层次结构和具体构成，即缓存对于模拟器核心代码是透明的，因此缓存的配置十分灵活。

#### 缓存

现代存储系统中缓存是不可或缺的部分，而现在CPU的缓存也有非常多的配置可供选择。综合考虑下，我实现了一套踪迹驱动的可插拔、可配置的缓存系统。这部分的功能对应`Storage`类、`Cache`类和`Memory`类（见`cache.hpp/cpp`）。

`Storage`类是`Cache`类和`Memory`类的抽象父类，约定了这几个踪迹驱动模拟类的接口。由于模拟器没有必要从模拟的缓存中读取数据（因为这并不比从主存中读取更快），上述这几个类实际上接受要读或写的物理地址然后返回所需的周期数。

具体到`Cache`类，给定要读的物理地址后，它首先查找该地址对应的缓存行是否已在缓存中，如果是则返回命中所需的周期数，否则返回下一级缓存（或主存）读该地址所需的周期数，并做出相应替换（目前的替换算法是LRU）。写的情况则稍微复杂一点。如果写命中，则设置脏标志（dirty bit）并根据是否采用写回策略相应更新下一级缓存。若写不命中，则根据是否采用写分配策略做出替换或直接写下一级缓存。采用写回策略替换时也要注意检查脏标记，所需周期数也要加上写入下一级缓存的时间。`Memory`类则简单得多，无论读写都只需返回配置中访问主存所需的周期数即可。

回到存储系统的角度，读写数据和获取操作所需的周期数在模拟器代码的实现中实际上是独立的。存储系统一方面维护用于读写数据的主存页面及其页表，一方面维护用于获取周期数的踪迹驱动缓存子系统。踪迹驱动缓存子系统则维护已创建的`Cache`、指令读取入口和数据读写入口。

#### 非对齐访问

非对齐访问是一个不容易注意到的边界情况。在读写数据以及获取所需周期数的过程中，要判断读写的区间是否跨页、跨缓存行。如果跨了，则要分两次读取然后合并。

### 指令语义的解析

这部分功能的代码位于`decode_helpers.hpp/cpp`。

实际上解析指令语义的工作并不复杂，就是按照RISCV的SPEC一种一种格式，一条一条指令地去解析，在代码中的体现就是一条又一条的switch语句。因为该模拟器支持RV64C（2字节的压缩指令），所以解析指令的第一步是判断指令的最低2位是否为11，是则为4字节的正常指令，否则为2字节的压缩指令。确定指令长度后根据指令的细分类型获取指令的语义，包括

- opcode：指令功能
- funct3：指令的细分功能，在访存指令用以确定字节数和扩展方式
- rs1，rs2，rd：源寄存器1，源寄存器2，目的寄存器（无则置0）
- imm：指令中的立即数
- alu_op：ALU要执行的运算
- compressed_inst：是否为压缩指令

值得一提的是解析指令尤其是压缩指令的过程中会经常需要将指令中的某几位提取出来再跟其他几位拼在一起，我将这个过程写成了如下的函数方便使用：

```C++
/**
 *  extract `count` bits from `start` in `inst` and left shift `shamt` bits
 */
inline uint32_t getbits(inst_t inst, int start, int count, int shamt = 0)
{
    return ((inst >> start) & ((1 << count) - 1)) << shamt;
}
```

这样拼接立即数就可以方便一点，比如

```c++
e.imm = getbits(inst, 5, 1, 3) | getbits(inst, 6, 1, 2) |
        getbits(inst, 7, 4, 6) | getbits(inst, 11, 2, 4);
```

### 控制信号的处理

这部分功能的代码位于`Simulator::process_control_signal()`（见`simulator.hpp/cpp`）。

得益于RISCV指令集的简单设计，RISCV五阶段流水线并没有复杂的冒险组合情况，因此控制信号的处理并不难。为了方便说明，下面用IF、ID、EX、MEM、WB分别代指流水线的取指、译码、执行、访存、写回阶段。大写字母F、D、E、M、W代表对应阶段（与前一阶段之间）的流水线寄存器，小写字母f、d、e、m、w代表（由上一阶段产生的）准备写入对应流水线寄存器的信号。

下面对每种冒险逐个分类讨论。

#### 数据冒险

数据冒险根据是否使用数据前递需要分开讨论。不使用数据前递时，由于最后一个阶段写回才会更新寄存器，因此只要EX、MEM和WB的目的寄存器等于ID的任一源寄存器，就产生了数据冒险。这种情况的代码如下：

```c++
data_dependent =
    (e.rs1 != 0 && (E.rd == e.rs1 || M.rd == e.rs1 || W.rd == e.rs1)) ||
    (e.rs2 != 0 && (E.rd == e.rs2 || M.rd == e.rs2 || W.rd == e.rs2));
```

使用数据前递则只需处理LOAD指令的情况，代码如下：

```c++
data_dependent = (e.rs1 != 0 && e.rs1 == E.rd && E.opcode == OP_LOAD) ||
            (e.rs2 != 0 && e.rs2 == E.rd && E.opcode == OP_LOAD);
```

数据冒险的一种特殊情况是系统调用，也就是ecall指令。一方面，系统调用可能访问所有寄存器，因此必须等ecall指令到达WB再处理；另一方面，系统调用可能会修改所有寄存器，因此必须暂停解码直到ecall指令处理完毕。所以判断条件为：

```c++
meet_ecall = (E.opcode == OP_ECALL || M.opcode == OP_ECALL || W.opcode == OP_ECALL);
```

数据冒险的处理方式是暂停解码直到寄存器的值更新，因此控制信号是IF、ID暂停（stall），EX气泡（bubble）。

#### 控制冒险

控制冒险来源于错误的分支预测。分支和跳转指令需要到执行阶段才能知道是否要跳转以及跳转的地址，判断条件如下：

```c++
mispredicted =
    (((E.opcode == OP_BRANCH && m.cond) || E.opcode == OP_JALR || E.opcode == OP_JAL) && E.predPC != m.valE)
        || (E.opcode == OP_BRANCH && !m.cond && E.predPC != m.val2);
```

其中`E.predPC`是之前预测的地址，`m.valE`是条件为真时条件跳转、JAL和JALR的跳转地址，`m.val2`则是条件为假时条件跳转的跳转地址。判断逻辑就是看预测的地址是否与实际的地址相同。

预测错误的处理方式是取消流水线中错误的指令，因此控制信号是ID、EX气泡（因为这是下一个阶段的控制信号，实际上取消了现阶段在IF和ID的指令）。

#### 组合冒险

仔细分析上面的判断条件，可以发现在不使用数据前递时且`E.opcode == OP_JALR`时可能会导致`mispredicted`和`data_dependent`同时为真，这时候控制信号会有冲突。实际上转移预测错了，ID的指令会被取消掉，这时也就无所谓数据冒险了。因此这种情况按照预测错误的处理方式处理即可。

### 系统调用和库函数接口的处理

我自己实现了一个迷你的库，取名为tinylib。库函数的头文件位于`include`目录下，库函数的实现位于`lib`目录下。模拟器处理系统调用的代码位于`Simulator::process_syscall()`（见`simulator.hpp/cpp`）。

我按照一般习惯约定系统调用号放在a7寄存器，参数放在a1-a5寄存器（所以目前最多五个参数），返回值放在a0寄存器。目前实现了如下4个系统调用：

- 程序退出
- `cputchar`：往模拟器标准输出打印a1寄存器中的字符
- `sbrk`：按照给定字节数扩展堆空间，返回新增部分的起始地址
- `readint`：在模拟器标准输入获取一个整数并返回
- `time`：返回自从Epoch以来经过的秒数

基于这些系统调用，tinylib封装了一些常用的库函数（见”库函数“一节）。其中堆空间借助Splay维护，可在空间利用率和速度都有比较好的表现。在库函数的实现中，系统调用可通过一个封装的通用系统调用接口来减少内联汇编的书写：

```c
static inline int64_t
syscall(int num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5)
{
	int64_t ret;

	// Generic system call: pass system call number in a7,
	// up to five parameters in a1, a2, a3, a4, a5.
	// Interrupt kernel with `ecall`.
	//
	// The "volatile" tells the assembler not to optimize
	// this instruction away just because we don't use the
	// return value.
	//
	// The last clause tells the assembler that this can
	// potentially change arbitrary memory locations.

	asm volatile(
        "mv a1, %1\n"
        "mv a2, %2\n"
        "mv a3, %3\n"
        "mv a4, %4\n"
        "mv a5, %5\n"
        "li a7, %6\n"
        "ecall\n"
        "mv %0, a0"
        :   "=r" (ret)
        :   "r" (a1),
            "r" (a2),
            "r" (a3),
            "r" (a4),
            "r" (a5),
            "i" (num)
        :   "memory"
    );

	return ret;
}
```

### 性能计数相关模块的处理

目前模拟器的性能计数包括动态指令数，周期数和缓存命中率。动态指令数和周期数的统计主要在`simulator.cpp`中，缓存命中率则位于`cache.cpp`中。

动态指令数的统计需要寻找一个合适的“窗口”并注意排除流水线控制信号的影响。我选择以WB阶段为“窗口”，因为确实被执行了的指令一定会经过WB阶段，而预测错误的指令则会在WB阶段之前被取消，如此可以保证统计出来指令数的准确性。如果WB阶段是气泡或被暂停，该指令也不能被计入动态指令数。因此统计动态指令数的代码如下：

```c++
if (!W.stall && !W.bubble)
    instruction_count++;
```

在模拟器中统计周期数则与真实硬件有些不同。按照真实硬件的执行过程，模拟器应该一个周期一个周期地执行，如果遇到多周期指令则暂停直到执行完毕。这样做能十分自然地统计周期数，但实际上没有必要，因为模拟器不是真实的硬件，它并不需要真的花费这么多的“周期”来完成一个执行或访存操作。所以我在设计时，五个流水线阶段和存储系统的访存接口的返回值都是所需周期数。计算周期数时，串行的操作（比如缓存写回再读取）则将周期数相加，并行的操作（比如五个流水线阶段）则取最大值。这样的设计既可以准确地统计周期数，又可以不牺牲模拟器的效率，应该算是比较好的实践。

缓存命中率的统计则十分简单，只需在命中时和未命中时添加一个计数器，查看代码便一目了然。

### 调试接口

该模拟器的调试功能比较简单，我将它集成在了`Simulator`类中（见`simulator.hpp`和`simulator_debugger.cpp`）。

模拟器的调试部分主要参考了gdb的设计，从中抽取了一部分常用命令。调试接口包括命令处理、断点检查和打印通用寄存器和流水线寄存器信息。断点的维护使用标准库的`set`，因此添加断点和检查断点都非常简单。命令中的表达式可以含有符号，因此需要读取ELF文件时要把符号表保存下来，计算时再查找并取出相应符号的值。

## 其他说明

暂无