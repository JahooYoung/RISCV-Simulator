### 需求及方案

- 流水线：插入流水线寄存器，按顺序处理流水线（参考模板）
- 多周期：用bubble和stall来控制
  - 不考虑乱序执行等多执行单元的实现
- 控制相关，数据相关：支持bubble和stall，还要考虑组合冒险
- 转移预测：预留接口，用不同的类实现
  - 接口为提供指令地址，指令内容，返回预测地址
- 存储器访问延时：访存使用统一接口，包括指令和数据，用bubble和stall来控制
- 配置文件：https://github.com/nlohmann/json
- 支持单步模式，并支持查看寄存器和内存
  - 分成SEQ模式和PIPE模式

### 架构

- Simulator: contains
  - RegisterFile
    - generalReg[32]
    - pc?
  - Fetch
  - Decode
  - Execute
  - AccessMemory
  - WriteBack
- Memory: contains
  - Cache
  - backup memory
- ElfReader

