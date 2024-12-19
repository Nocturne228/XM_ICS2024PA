# PA2.2

## Runtime System/Environment

应用程序的运行都需要[运行时环境](http://en.wikipedia.org/wiki/Runtime_system)的支持，包括加载，销毁程序，以及提供程序运行时的各种动态链接库。

## Encapsulates library functions

运行时环境直接运行在计算机硬件上，其实现自然与架构相关。那么对于一个程序，要想适配不同架构，如果直接使用架构自身指令的话，则每个程序都要针对每个架构采用不同的实现，维护工程量大大增加。

对于这些相同功能的不同版本的实现，有一个有力的工具去简化实现，那就是**抽象**。我们只需要提供各自架构的 API，那么系统只需要维护这些 API，而程序只需要调用这些相同的 API，就能在不同架构上正确运行。例如 n 个程序想要在 m 个架构上运行，如果不采用抽象，需要维护 n*m 份代码；而通过 API，只需要维护 n+m 份代码即可。

> 这就是运行时环境的一种普遍的存在方式：库。运行程序所需要的公共要素被抽象成API，不同的架构只需要实现这些API，也就相当于实现了支撑程序运行的运行时环境，这提升了程序开发的效率：需要的时候只要调用这些API，就能使用运行时环境提供的相应功能。

## AM: bare-metal RE

一个相对更加复杂的程序必然需要依赖库来运行，实现更多更复杂的功能。我们把这些需求收集起来，提供不同架构的实现，即可使程序在运行时不必关心自己运行在什么架构上。由于这组统一抽象的API代表了程序运行对计算机的需求，所以我们把这组API称为抽象计算机。

> AM(Abstract machine)项目就是这样诞生的。为一个向程序提供运行时环境的库，AM根据程序的需求把库划分成以下模块
>
> ```
> AM = TRM + IOE + CTE + VME + MPE
> ```
>
> - TRM(Turing Machine) - 图灵机，最简单的运行时环境，为程序提供基本的计算能力
> - IOE(I/O Extension) - 输入输出扩展，为程序提供输出输入的能力
> - CTE(Context Extension) - 上下文扩展，为程序提供上下文管理的能力
> - VME(Virtual Memory Extension) - 虚存扩展，为程序提供虚存管理的能力
> - MPE(Multi-Processor Extension) - 多处理器扩展，为程序提供多处理器通信的能力 (MPE超出了ICS课程的范围，在PA中不会涉及)

关于 NEMU 与 AM 的关系，简单来说，NEMU 提供了硬件，而 AM 是软件，为程序的运行提供了所需的运行时环境。

> AM让NEMU和程序的界线更加泾渭分明，同时使得PA的流程更加明确
>
> ```
> (在NEMU中)实现硬件功能 -> (在AM中)提供运行时环境 -> (在APP层)运行程序
> (在NEMU中)实现更强大的硬件功能 -> (在AM中)提供更丰富的运行时环境 -> (在APP层)运行更复杂的程序
> ```

# RTFSC

AM 的结构并不是很复杂，我们目前主要关心的是 `am/platform/nemu` 中以 NEMU 为平台的 AM 实现，`am/riscv` 中 RISC-V32 的相关实现。需要实现的部分则位于 `klib` 中。相关 API 参阅：[AbstractMachine 规约 (Specifications)](https://jyywiki.cn/AbstractMachine/AM_Spec.md)

> AM 项目包括提供了不同架构 API 的 `am` 和架构无关的库函数 `klib`。`am/src/platform/nemu/trm.c` 中的代码展示了程序在 TRM 上运行只需要极少的 API：
>
> - `Area heap`结构用于指示堆区的起始和末尾
> - `void putch(char ch)`用于输出一个字符
> - `void halt(int code)`用于结束程序的运行
> - `void _trm_init()`用于进行TRM相关的初始化工作

关于 NEMU 通过交叉编译运行客户程序的过程在 PA2.0 已有描述。编译得到的可执行文件的行为如下：

查看 `abstract-machine/am/src/riscv/nemu/start.S` 的代码：

```asm
.section entry, "ax"
.globl _start
.type _start, @function

_start:
  mv s0, zero
  la sp, _stack_pointer
  call _trm_init

.size _start, . - _start
```

第一条指令就从此处开始，设置好栈顶指针后跳转到 `_trm_init` 函数：

```c
void _trm_init() {
  int ret = main(mainargs);
  halt(ret);
}
```

调用 `main()` 函数执行程序的主体功能，最后调用 `halt()` 停机。

---

任务：阅读 `abstract-machine` 项目的 Makefile 文件；

必做题：阅读 NEMU 的代码并合适地修改 Makefile，使得通过 AM 的 Makefile 可以默认启动批处理模式的 NEMU（自动执行 `c`）

AM 的 Makefile 的各部分功能都有简要的注释，这里进行总结：首先定义了一个 `html` 命令，调用 `markdown_py` 将此 Makefile 文件转换成 html 文件。

**基础设置与检查：**

判断 `$MAKECMDGOALS` 变量，默认创建一个裸机内核镜像。如果执行 `make clean/clean-all/html` 则不进行后续的核查。接下来开始打印项目构建信息。检查环境变量 `$AM_HOME` 是否正确设定指向 AM 项目目录路径，编译命令中输入的 `$ARCH` 是否为支持的架构，如果不是则打印错误信息。根据用户输入的 ARCH 信息进行字符串分割和提取，分别得到 ISA 和平台。最后检查是否有需要进行构建的文件。

> 注：PA 中采用 ISA-Platform 二元组来表示一个架构。

**通用编译目标：**

这一步定义了需要编译的文件

- 定义工作目录、目标目录，并确保目标目录存在。
- 定义镜像和档案文件的路径。
- 收集要链接的目标文件和库文件。

**通用编译标志：**

- 定义交叉编译工具
- 设置包含路径并定义各种C、C++和汇编的编译标志。

**架构规约配置：**

设定包含 `$(AM_HOME)/scripts/$(ARCH).mk` 配置文件。比如 `riscv32-nemu.mk` 文件定义了 riscv 架构的 mk 脚本文件和一些编译标志，也包括前面提到的 AM 源文件 `start.S` 等等。

**编译规则：**

- 定义将`.c`、`.cc`、`.cpp`和`.S`文件编译为目标文件的规则。

- 递归调用make来构建依赖库。

- 将目标文件和库文件链接为最终的ELF二进制文件。

- 将目标文件归档为静态库。

- 包含由编译器生成的依赖文件。

**杂项：**

- 定义构建顺序，防止并行构建`image-dep`，并提供清理目标以删除构建工件。

---

现在可以来看必做题了。由于是要启动 NEMU 的批处理模式，可以注意到 Makefile 中包含的架构相关的配置脚本还包含了如下的 mk 脚本

```Makefile
include $(AM_HOME)/scripts/isa/riscv.mk
include $(AM_HOME)/scripts/platform/nemu.mk
```

查看 `nemu.mk` ，可以看到设置了一些编译链接标志，注意到这里还设置了 `NEMUFLAGS`，目前只添加了 `-l`，其含义看起来是 输出日志文件。关于 NEMU 的参数含义，记得在 PA1 RTFSC 时，系统是首先初始化 monitor 的，其中似乎有一个`parse_args` 函数，前往 `monitor.c` 文件看看源码：

```c
static int parse_args(int argc, char *argv[]) {
  const struct option table[] = {
      {"batch", no_argument, NULL, 'b'},
      {"log", required_argument, NULL, 'l'},
      {"diff", required_argument, NULL, 'd'},
      {"port", required_argument, NULL, 'p'},
      {"help", no_argument, NULL, 'h'},
      {0, 0, NULL, 0},
  };
  int o;
  while ((o = getopt_long(argc, argv, "-bhl:d:p:", table, NULL)) != -1) {
    switch (o) {
      case 'b':
        sdb_set_batch_mode();
        break;
        ......
```

重要的代码就在这里了！如果 `getopt_long` 解析出 b 参数，那么将会执行 `sdb_set_batch_mode` 函数，看函数名称就是设置批处理模式。进入函数的源码看一看：

```c
void sdb_set_batch_mode() { is_batch_mode = true; }

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }
```

就是这个 b 参数没错了。这样只需要在 `$(AM_HOME)/scripts/platform/nemu.mk` 位置添加一个标志 `NEMUFLAGS += -b`，就能启动 NEMU 的批处理模式了。