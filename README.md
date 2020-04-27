# RISCV Simulator

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

实验报告及实现细节见[这里](./doc/lab_report.md)。

## 使用说明

### 环境及依赖

- 建议在Ubuntu 18.04下编译
- g++（需支持c++17，推荐版本7.4.0或以上）
- make
- riscv-gnu-toolchain（包括riscv64-unknown-elf-gcc，riscv64-unknown-elf-ar，riscv64-unknown-elf-objdump）
- libyaml-cpp（`sudo apt install libyaml-cpp-dev`）

### 编译

项目内已有`GNUmakefile`，直接运行命令`make`即可编译模拟器和库函数。编译出来的模拟器可执行文件是`build/simulator`，库文件是`build/lib/libtiny.a`。

如果`g++`或`riscv64-unknown-elf-gcc`等编译器工具链的路径需要指定，请修改`GNUmakefile`中的相应变量。

### 运行

运行`./build/simulator --help`可查看用法及命令行参数：

```
Usage: ./build/simulator [options] elf_file|trace_file [args...]

Options:
  -h, --help               Print this help
  -c, --config config_file Specify the configuration file,
                           default is 'default_config.yml'
Options for elf_file:
  -s                       Single step mode
  -i, --info info_file     Output filename of Elf information
  -v                       Verbose mode
```

运行该模拟器**需要有配置文件**。配置文件是YAML格式，默认是项目中已提供的`default_config.yml`。配置文件的内容及说明请见"配置文件说明"一节。

该模拟器的输入有两种：

1. RISCV格式并静态链接`libtiny`的ELF文件。编译前应确保源代码只包含一个头文件`tinylib.h`，并**确保源代码没有使用其他库函数**（`riscv64-unknown-elf-gcc`可能会默认链接glibc/newlib的标准库函数，tinylib的库函数列表见“库函数”一节）。编译命令请参考

```
riscv64-unknown-elf-gcc -Iinclude -O2 -Wa,-march=rv64imc -static -o [output_file] [your_source_file] -Lbuild/lib -ltiny
```

2. 访存trace文件。**注意这种文件必须以`.trace`为后缀。**

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
- `assert(expr)`：断言宏

## 配置文件说明

配置文件采用YAML格式，可配置的内容有

- `disassemble`：bool类型，表示是否反汇编（单步模式中打印流水线时会使用）
- `objdump`：string类型，表示riscv的objdump的路径（若不反汇编则可以忽略该参数）
- `data_forwarding`：bool类型，表示是否进行数据前递
- `branch_predictor`：string类型，表示转移预测策略。可选项有
  - `never_taken`
  - `always_taken`
  - `btfnt`（Backward Taken Forward Not Taken，后跳前不跳）
  - `branch_history_table`（pc后13位寻址的2-bit跳转历史表）
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
