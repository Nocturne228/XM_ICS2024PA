# PA1.0

> 这里是进行代码工作之前的一些思想小补充~

在进行PA1前，需要完成一点git操作：

```shell
git commit --allow-empty -am "before starting pa1"
git checkout master
git merge pa0
git checkout -b pa1
```

下图展示了PA的各个子目录

```
ics2024
├── abstract-machine   # 抽象计算机
├── am-kernels         # 基于抽象计算机开发的应用程序
├── fceux-am           # 红白机模拟器
├── init.sh            # 初始化脚本
├── Makefile           # 用于工程打包提交
├── nemu               # NEMU
└── README.md
```



> ! **Warning：**
>
> 文档进行了事先声明：讲义并不会事无巨细地讲清楚每一处细节与原理，很多地方只会阐述大致流程和目标，需要自行探索实现方式并承担出错的风险，这也是PA期望进行训练的能力之一。（CS144的lab文档也同样指明了这一点）

> 我在做什么？
>
> 目前的PA1主要聚焦于NEMU的基础设施，或者说我们正在实现NEMU。NEMU是参考了QEMU进行简化和修改而来的项目，也就是一个虚拟机，简单来说模拟了计算机硬件环境，当然是以软件的方式进行的。自然在这硬件之上会运行软件，也就是说NEMU，即虚拟机，是一个运行软件的软件。（虚拟机技术是很重要的一个领域，再后来还有了更火的容器技术，如Docker）
>
> 而模拟（或者说虚拟化Virtualization）的一个强大之处在于运行在其上的程序并不能感知它自身是运行在真实硬件上还是虚拟环境中。

PA提供了三条“主线”：三种主流的ISA---x86、MIPS、RISCV。有很多原因使得PA更推荐选择RISCV32(64)，这里就也选择RISCV32，相关文档参考（重要！！！）：

- The RISC-V Instruction Set Manual ([Volume I](https://github.com/riscv/riscv-isa-manual/releases/download/Priv-v1.12/riscv-privileged-20211203.pdf), [Volume II](https://github.com/riscv/riscv-isa-manual/releases/download/Ratified-IMAFDQC/riscv-spec-20191213.pdf))
- [ABI for riscv](https://github.com/riscv-non-isa/riscv-elf-psabi-doc)

---

# 计算机？

既然要虚拟化一个计算机，首先要知道什么是计算机。

既然要执行程序，那么**程序该放在哪里**？---> 存储器

如何**执行程序**？---> CPU

CPU与存储器之间的数据交换的时间开销过大 ---> 寄存器暂存CPU的计算结果

最重要的一点，能不能让程序自动控制CPU的执行？这一定程度上是计算机的本质特征，也就是不再需要人类一步步地操控它，而是丢给它一套流程（算法），计算机自己便知道怎样去完成。

于是约定一个特殊的寄存器：PC（Program Counter）程序计数器来指示CPU的执行位置，这样CPU就知道现在执行到哪条指令了。如果每次执行指令都能更新PC的值，那么一个计算机就诞生了：

```c
while (1) {
  从PC指示的存储器位置取出指令;
  执行指令;
  更新PC;
}
```

这就是计算机的核心思想，由伟大的Alan Turing提出，被称为Universal Turing Machine

## 状态机

jyy一直在强调一个观点：程序是一个状态机，这个思想大概来自数字逻辑电路。简单说就是事件从一个状态，根据某个转移条件，进入另一个状态。在学习编译原理时还给出了有限状态机的数学定义，不再赘述。

于是，状态机的观点给出了一个看待程序的视角，绘制出这样的执行图景：当一个程序进入内存开始执行，相当于在一个状态转移图中制定了一个初始状态，程序从这个初始状态开始进行一次次确定的状态转移，对应现实中，一条条执行指令。*至于状态转移图，可能类似PA文档中给出的这样：*

<img src="https://nju-projectn.github.io/ics-pa-gitbook/ics2024/images/state-machine.png" alt="FSM" style="zoom:50%;" />

现在，根据一个计算1到100的自然数之和的程序：

```assembly
// PC: instruction    | // label: statement
0: mov  r1, 0         |  pc0: r1 = 0;
1: mov  r2, 0         |  pc1: r2 = 0;
2: addi r2, r2, 1     |  pc2: r2 = r2 + 1;
3: add  r1, r1, r2    |  pc3: r1 = r1 + r2;
4: blt  r2, 100, 2    |  pc4: if (r2 < 100) goto pc2;   // branch if less than
5: jmp 5              |  pc5: goto pc5;
```

可以使用三元组`(PC, r1, r2)`绘制出状态机：

```python
(0, x, x) -> (1, 0, x) -> (2, 0, 0) -> (3, 0, 1) -> (4, 1, 1) -> (2, 1, 1) -> (3, 1, 2) -> (4, 3, 2) -> ... -> (2, 4950, 99) -> (3, 4950, 100) -> (4, 5050, 100) -> (5, 5050, 100) -> (5, 5050, 100) -> ...
```

> 在学习操作系统和编译原理，或者可能程序设计课中就讲过，一个程序需要从静态和动态两个视角来分析。首先是静态的代码文本，描述了程序应该具有怎样的行为逻辑，对于人类来说**相对**直观和清晰，我们借此理解程序的整体语义。然后是动态的执行过程，也可以看做进程（当然忽略了操作系统调度层面的复杂性）：具体且详细地揭示了程序的执行过程，刻画了程序在计算机上运行的行为本质，但是计算量太大，状态数量众多，人类小小的大脑难以承受。这种情况下，可以清晰地看到程序局部的具体行为，帮助掌握宏观上难以理解的结果。

关于动态分析的一个例子就是递归程序。递归在代码和逻辑层面上很难直观地去理解，这时通过状态机的视角进行分析可能会有奇效！

# RTFSC?

下面是ICS PA项目的各个子项目：

```
ics2024
├── abstract-machine   # 抽象计算机
├── am-kernels         # 基于抽象计算机开发的应用程序
├── fceux-am           # 红白机模拟器
├── init.sh            # 初始化脚本
├── Makefile           # 用于工程打包提交
├── nemu               # NEMU
└── README.md
```

现在主要聚焦于nemu，未来会常回来看看的：

```
emu
├── configs                    # 预先提供的一些配置文件
├── include                    # 存放全局使用的头文件
│   ├── common.h               # 公用的头文件
│   ├── config                 # 配置系统生成的头文件, 用于维护配置选项更新的时间戳
│   ├── cpu
│   │   ├── cpu.h
│   │   ├── decode.h           # 译码相关
│   │   ├── difftest.h
│   │   └── ifetch.h           # 取指相关
│   ├── debug.h                # 一些方便调试用的宏
│   ├── device                 # 设备相关
│   ├── difftest-def.h
│   ├── generated
│   │   └── autoconf.h         # 配置系统生成的头文件, 用于根据配置信息定义相关的宏
│   ├── isa.h                  # ISA相关
│   ├── macro.h                # 一些方便的宏定义
│   ├── memory                 # 访问内存相关
│   └── utils.h
├── Kconfig                    # 配置信息管理的规则
├── Makefile                   # Makefile构建脚本
├── README.md
├── resource                   # 一些辅助资源
├── scripts                    # Makefile构建脚本
│   ├── build.mk
│   ├── config.mk
│   ├── git.mk                 # git版本控制相关
│   └── native.mk
├── src                        # 源文件
│   ├── cpu
│   │   └── cpu-exec.c         # 指令执行的主循环
│   ├── device                 # 设备相关
│   ├── engine
│   │   └── interpreter        # 解释器的实现
│   ├── filelist.mk
│   ├── isa                    # ISA相关的实现
│   │   ├── mips32
│   │   ├── riscv32
│   │   ├── riscv64
│   │   └── x86
│   ├── memory                 # 内存访问的实现
│   ├── monitor
│   │   ├── monitor.c
│   │   └── sdb                # 简易调试器
│   │       ├── expr.c         # 表达式求值的实现
│   │       ├── sdb.c          # 简易调试器的命令处理
│   │       └── watchpoint.c   # 监视点的实现
│   ├── nemu-main.c            # 你知道的...
│   └── utils                  # 一些公共的功能
│       ├── log.c              # 日志文件相关
│       ├── rand.c
│       ├── state.c
│       └── timer.c
└── tools                      # 一些工具
    ├── fixdep                 # 依赖修复, 配合配置系统进行使用
    ├── gen-expr
    ├── kconfig                # 配置系统
    ├── kvm-diff
    ├── qemu-diff
    └── spike-diff
```



> PA1.0 DONE∎
