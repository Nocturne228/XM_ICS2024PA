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


---

> AM 移植了 RISC-V 开源社区的一些测试集：
>
> - https://github.com/NJU-ProjectN/riscv-tests-am
> - https://github.com/NJU-ProjectN/riscv-arch-test-am

这里又发现关于 `div` 和 `rem` 等运算出现了  Floatpoint Exception，实际上是存在除零运算和数字溢出，值不符合 RISC-V 的规定。[StackOverflow](https://stackoverflow.com/questions/70876942) 上也有人提问，查阅 [An Embedded RISC-V Blog](https://five-embeddev.com/riscv-user-isa-manual/Priv-v1.12/m.html#tab:divby0) 得到下面表格：

| Condition              |  Dividend   | Divisor |  DIVU[W]  | REMU[W] |   DIV[W]    | REM[W] |
| :--------------------- | :---------: | :-----: | :-------: | :-----: | :---------: | :----: |
| Division by zero       |      x      |    0    | $2^L − 1$ |    x    |     − 1     |   x    |
| Overflow (signed only) | $− 2^L − 1$ |   − 1   |     –     |    –    | $− 2^L − 1$ |   0    |

根据这里定义的行为完善指令即可。

---

# KLIB

记得 AM 项目分为 `am` 和 `klib` 两大部分。我们可以说 AM 提供了运行时环境（RE），而 `am` 中是架构相关的RE，`klib` 则是封装了架构无关的 RE。我们需要实现 `abstract-machine/klib/src/string.c` 和 `abstract-machine/klib/src/stdio.c` 中的库函数。

> 为什么时时刻刻要求 TRFM？
>
> 手册其实折射出计算机系统工作的一种基本原则：遵守约定。我们都希望计算机返回的是确定的结果，约定就是这种确定。如果违反了约定呢？这就引入了[未定义行为(UB, Undefined Behavior)](https://en.wikipedia.org/wiki/Undefined_behavior)的概念。系统保证约定好的行为，而违约行为得到的结果不被保证。
>
> 计算机系统各个抽象层之间都是一种约定，无论是操作系统层次还是计算机网络层次。关于未定义行为，这个概念也会给约定的具体实现提供自由度，不同的具体架构可以对未定义行为有自己的实现。
>
> >  RTFM是了解接口行为和约定的过程：每个输入的含义是什么？查阅对象的具体行为是什么？输出什么？有哪些约束条件必须遵守？哪些情况下会报什么错误？哪些行为是UB?
>
> [这篇论文](http://www.cs.utah.edu/~regehr/papers/overflow12.pdf)对整数溢出的分类和行为进行了梳理：我们不仅需要编写通过测试的代码，而且需要编写符合语言规范的 well-defined 的代码
>
> ****

我们可以研究一下主流 libc 的实现，大型的 OS 如 [GNU C Library](https://www.gnu.org/software/libc/)，[BSD libc](https://svnweb.freebsd.org/base/head/lib/libc/)，小型的如 [musl](https://musl.libc.org/)，[diet libc](https://www.fefe.de/dietlibc/)，[Bionic](https://android.googlesource.com/platform/bionic/)，[New lib](https://sourceware.org/newlib/)，[uclibc](https://uclibc.org/)

## string

感谢[开源](https://github.com/torvalds/linux/blob/master/lib/string.c)，我直接 Copy，完美运行。

## stdio

为了通过测试 `hello-str`，我们需要实现 stdio 库的 `sprintf` 函数。关于 [RTFM](https://www.gnu.org/software/libc/manual/html_node/Formatted-Output-Functions.html) （GNU官方的手册）：

```shell
man 3 printf
```

可以看到 prinf 函数是一个大家族，我们先忽略其他牛鬼蛇神，主要关注 sprintf 和可变参数列表 `va_list` 及其对应的 `stdarg` 库：

```c
#include <stdio.h>

int printf(const char *restrict format, ...);
int fprintf(FILE *restrict stream,
           const char *restrict format, ...);
int dprintf(int fd,
           const char *restrict format, ...);
int sprintf(char *restrict str,
           const char *restrict format, ...);
int snprintf(char str[restrict .size], size_t size,
           const char *restrict format, ...);

int vprintf(const char *restrict format, va_list ap);
int vfprintf(FILE *restrict stream,
           const char *restrict format, va_list ap);
int vdprintf(int fd,
           const char *restrict format, va_list ap);
int vsprintf(char *restrict str,
           const char *restrict format, va_list ap);
int vsnprintf(char str[restrict .size], size_t size,
           const char *restrict format, va_list ap);
```

阅读手册可以知道，`sprintf` 的行为类似 `printf`，关于格式化的部分是完全一致的，区别是 `sprintf` 会将格式化后的字符串存入接收的字符串 `str` 中，并返回输出到字符串中的字符长度（不包含作为终止符的空字符）。注意函数会在尾部添加一个空字符 `'\0'`。未定义行为：`sprintf` 是一个不安全的函数，首先它不会检查缓冲区大小，可能造成缓冲区的溢出，其次如果目标字符数组 `str` 和格式化字符串中的某个参数存在重叠（例如，`str` 同时作为格式化输出的目标和格式化字符串的一部分），则行为是未定义的。

观察现实中 libc 的实现，对传入字符的格式化处理由 `vsnprintf` 完成，`sprintf` 只需要通过可变参数列表直接调用 `vsnprintf` 即可。vsnprintf 函数全名为 Formatted output conversion with a variable argument list，其函数原型为

```c
int vsnprintf(char *str, size_t size, const char *format, va_list ap);
```

字符串中格式化标记可查阅 [GNU手册](https://www.gnu.org/software/libc/manual/html_node/Formatted-Output-Basics.html)。实际上 `vsnprintf` 是 `vsprintf` 的安全版本，避免了缓冲区溢出。我们可以优先实现 `vsprintf`，也就是字符串格式化处理的基础。但是我们先按下不表，去看一看其他库。

## stdarg

首先可以看到 `va_list` 应该是作为可变参数列表，RTFSC 发现：

```c
typedef __builtin_va_list __gnuc_va_list;
typedef __gnuc_va_list va_list;
```

`va_list` 实际上是对内置的 `__builtin_va_list` 的重命名而已，这是 gcc 提供的内建类型，是一个指向栈上内存空间（参数列表）的指针，可参考 [How is "__builtin_va_list" implemented?](https://stackoverflow.com/questions/49733154/how-is-builtin-va-list-implemented)。以 [tinycc](https://github.com/frida/tinycc/blob/main/lib/va_list.c) 库中的实现为例：

```c
typedef struct {
    unsigned int gp_offset;             // 常规寄存器偏移
    unsigned int fp_offset;             // 浮点寄存器偏移
    union {
        unsigned int overflow_offset;   // 溢出偏移
        char *overflow_arg_area;        // 溢出参数区
    };
    char *reg_save_area;                // 保存寄存器的区域
} __builtin_va_list[1];

```

可以看出这个内建类型在管理存储在栈上的可变参数列表，记录了关于参数的存储位置、寄存器的偏移以及溢出参数的存储方式。这些信息用于获取不同类型参数，比如 `gp_offset` 记录常规寄存器的偏移量，`fp_offset` 记录浮点寄存器的偏移量，后续根据所需类型进行获取，并更新偏移量即可。函数 `va_arg` 通过 `va_list` 获取各种类型的参数，它也是 gcc 提供的内建函数，可查阅 [GCC 文档](https://gcc.gnu.org/onlinedocs/gcc-13.2.0/gdc/Variadic-Intrinsics.html)，我们可以学习 tinycc 的实现（注意 gcc 提供的内置工具都是架构相关的，此代码则是将架构相关的代码作为参数传入，如果以 x86_64 上的 gcc 为例，其参数只有前两个：`va_list` 和 `T`）：

```c
void *__va_arg(__builtin_va_list ap, int arg_type, int size, int align) {
  size = (size + 7) & ~7;
  align = (align + 7) & ~7;
  switch ((enum __va_arg_type)arg_type) {
    case __va_gen_reg:
      if (ap->gp_offset + size <= 48) {
        ap->gp_offset += size;
        return ap->reg_save_area + ap->gp_offset - size;
      }
      goto use_overflow_area;

    case __va_float_reg:
      if (ap->fp_offset < 128 + 48) {
        ap->fp_offset += 16;
        if (size == 8) return ap->reg_save_area + ap->fp_offset - 16;
        if (ap->fp_offset < 128 + 48) {
          memcpy(ap->reg_save_area + ap->fp_offset - 8,
                 ap->reg_save_area + ap->fp_offset, 8);
          ap->fp_offset += 16;
          return ap->reg_save_area + ap->fp_offset - 32;
        }
      }
      goto use_overflow_area;

    case __va_stack:
    use_overflow_area:
      ap->overflow_arg_area += size;
      ap->overflow_arg_area =
          (char *)((long long)(ap->overflow_arg_area + align - 1) & -align);
      return ap->overflow_arg_area - size;

    default: /* should never happen */
      abort();
      return 0;
  }
}
```

- `ap`：是传递给 `va_list` 类型的参数，表示当前可变参数列表的位置。它是一个指针，指向保存参数的内存区域（通常是栈或寄存器）。
- `arg_type`：表示当前要读取的参数类型，它是一个整数，表示不同的寄存器类型或栈类型（例如整型、浮点型等）。

代码的前两行用于8字节对齐以适配一些架构。接下来就通过 switch 语句，根据传入的变量类型获取对应类型的参数，然后移动指针，更新偏移量，用于访问下一个参数。

参考其他库函数，可以看到可变参数列表的应用还需要 `va_start` 和 `va_end` 包裹，[手册相关内容](https://gcc.gnu.org/onlinedocs/gccint/Varargs.html)。显然也是编译器内建的：

```c
#define va_start(ap, last)	__builtin_va_start(ap, last)
#define va_end(ap) 			__builtin_va_end((ap))
```

由于没有找到相关代码，因为这些都是编译器[内置](https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html)的，关于这些描述可以参考[回答](https://stackoverflow.com/questions/56412342/where-is-builtin-va-start-defined)。简单说 `va_start` 根据栈帧信息（由 `last` 参数的地址）初始化 `ap`，使得 `ap` 指向第一个可变参数的位置。`va_end` 宏只需要清理 `va_list` ，在许多平台上不需要进行任何操作，因为栈上空间的清理会有其他工具处理。

## sprintf

回到库函数的实现，我们需要先实现函数 `vsprintf`，将传入的字符串进行格式化处理。Linus 又帮我写了一点点[小程序](https://github.com/torvalds/linux/blob/master/arch/alpha/boot/stdio.c#L293)，这个程序没有什么其他依赖库，非常酷！

我们先直入主题，实现 `vsprintf`。首先是一些变量的声明：

- `flags`：标志位，用于控制输出格式（例如填充、对齐、符号显示等）。
- `field_width`：输出字段的宽度。
- `precision`：数字的最小位数或者字符串的最大字符数。
- `qualifier`：整数字段的转换限定符（如 `h`、`l` 等）。
- `base`：数值的基数（如 10、16、8 等）。
- `num`：存储当前数字。
- `s`：存储当前字符串参数。

接下来开始通过 for 循环遍历字符串，当遇到 `%` 后，开始进行特殊标志的判断：

- 对于数字格式化方式（如对齐、补零等等），更新  `flags` 进行记录
- 对于字段宽度，如果有数字则提取数字，若是 `*` 则获取后面的参数，若是负数则左对齐，更新 `field_wirdth`标志
- 精度的处理和字段宽度类似，差不多的方式更新 `precision`
- 获取转换限定符（例`l`、`h`、`ll`等）更新 `qualifier`

根据前面获取的各种标记，开始根据约定好的标记进行格式化字符处理、数字格式化处理、最后添加 `'\0'` 终结符，返回生成的字符串长度。

例如 `musl` 等运行库抽象出了 `print_core` 函数，而 `Bionic` 则各个 `printf` 函数体中相似的代码部分抽取出宏定义`PRINF_IMPL(expr)` 来简化代码。~~由于 NEMU 很喜欢各种各样难懂的宏，~~我们就采用宏定义避免大量的 copy-paste：

```c
#define PRINTF_IMPL(expr) \
    va_list ap; \
    va_start(ap, fmt); \
    int result = (expr); \
    va_end(ap); \
    return result;

int sprintf(char *out, const char *fmt, ...) {
  PRINTF_IMPL(_vsprintf(out, fmt, ap));
}
```

# trace

对于一些复杂的情况，如果用 gdb 一步一步查看过程，效率会很低。我们以前写算法题的时候也会习惯用 `printf` 打印出我们关心的信息的变化过程。记录状态机的转移过程，也就是程序执行过程信息的作法称为[踪迹(trace)](https://en.wikipedia.org/wiki/Tracing_(software))。

## itrace

NEMU 已经实现了一个简单的 itrace 功能，作用是记录系统执行的每一条指令并输出到 log 文件中。文档没有给出详细描述，而是留作了一个 RTFC 的任务。

既然 itrace 会记录 `inst_fetch` 取到的所有指令，那么只需要记得 NEMU 中的指令执行过程就好了。观察 `isa_exec_once` 函数中没有相关代码，而 `cpu-exec.c` 中的 `exec_once` 中发现在调用 `isa_exec_once` 后，会根据宏 `COONFIG_ITRACE` 进行一些操作。根据我们前面对 Kconfig 的了解，这自然就是在 `make menuconfig` 时设置的选项，itrace 的源码在这里没错了。

阅读源码可以发现，在执行指令后，系统会记录指令及其反汇编内容，添加到 `Decode s` 中的 `logbuf` 位置，最后在 `trace_and_difftest` 中调用 `log_write` 写入日志文件。根据宏的描述，我们也可以修改 menuconfig 的配置，使其打印在屏幕上。

## iringbuf

显然并不是所有指令都值得关心，一般来说我们只想看到出错位置附近的指令。也就是说需要有一个数据结构，不断记录一定数量的指令，当超过数量时，数据结构中最新的指令应该覆盖掉最早执行的指令，同时输出顺序正确。可以维护一个简单的数据结构，称为环形缓冲区（ring buffer）

NEMU 并没有给出待实现的 API，这是我们第一次自己添加一个新文件去编写新功能。我选择在 `nemu/src/utils` 下新建一个 `iringbuf.c` 文件，因为相关的日志打印和反汇编都定义在这里。只需要再头文件中包含 `utils.h` 并进行声明就可以在其他地方使用了。实现也很简单：

```c
#include <common.h>

#define MAX_IRINGBUF 16

typedef struct {
  word_t pc;
  uint32_t inst;
} ItraceNode;

ItraceNode iringbuf[MAX_IRINGBUF];
int p_cur = 0;
bool full = false;

void trace_inst(word_t pc, uint32_t inst) {
  iringbuf[p_cur].pc = pc;
  iringbuf[p_cur].inst = inst;
  p_cur = (p_cur + 1) % MAX_IRINGBUF;
  full = full || p_cur == 0;
}

void display_inst() {
  if (!full && !p_cur) return;

  int end = p_cur;
  int i = full ? p_cur : 0;

  void disassemble(char *str, int size, uint64_t pc, uint8_t *code, int nbyte);
  char buf[128];  // 128 should be enough!
  char *p;
  Log("Most recently executed instructions");
  do {
    p = buf;
    p += sprintf(buf, "%s" FMT_WORD ": %08x ",
                 (i + 1) % MAX_IRINGBUF == end ? " --> " : "     ",
                 iringbuf[i].pc, iringbuf[i].inst);
    disassemble(p, buf + sizeof(buf) - p, iringbuf[i].pc,
                (uint8_t *)&iringbuf[i].inst, 4);

    if ((i + 1) % MAX_IRINGBUF == end) printf(ANSI_FG_RED);
    puts(buf);
  } while ((i = (i + 1) % MAX_IRINGBUF) != end);
  puts(ANSI_NONE);
}
```

这里创建了一个数组，其中的元素保存 pc 和对应的指令。在 `trace_inst` 中，只需要利用 `%` 运算，就可以在这个数组中以环形记录指令，保持顺序的正确。最后定义一个 `display_inst` 函数用于打印指令和反汇编结果，指示出错位置。

完成 `iringbuf` 后需要在其他部分添加记录和打印指令的代码。在 `isa_exec_once` 中记录 `inst_fetch` 的指令：

```c
int isa_exec_once(Decode *s) {
  s->isa.inst = inst_fetch(&s->snpc, 4);
  IFDEF(CONFIG_ITRACE, trace_inst(s->pc, s->isa.inst));
  return decode_exec(s);
}
```

另外在 `cpu_exec.c` 中的 `assert_fail_msg` 函数中进行指令的打印（为了避免输出太长，注释掉了打印寄存器的功能）

```c
void assert_fail_msg() {
  display_inst();
  // isa_reg_display();
  statistic();
}
```

我选择在 NEMU 处于 `BAD TRAP` 和 `ABORT` 时打印信息，因此在 `cpu_exec` 中添加：

```c
  switch (nemu_state.state) {
    case NEMU_RUNNING: nemu_state.state = NEMU_STOP; break;

    case NEMU_END: case NEMU_ABORT:
      Log("nemu: %s at pc = " FMT_WORD,
          (nemu_state.state == NEMU_ABORT ? ANSI_FMT("ABORT", ANSI_FG_RED) :
           (nemu_state.halt_ret == 0 ? ANSI_FMT("HIT GOOD TRAP", ANSI_FG_GREEN) :
            ANSI_FMT("HIT BAD TRAP", ANSI_FG_RED))),
          nemu_state.halt_pc);
      // fall through
      if (nemu_state.state == NEMU_ABORT || nemu_state.halt_ret != 0) assert_fail_msg();
    case NEMU_QUIT: statistic();
  }
```

## mtrace

这个功能记录了对内存的读写，只需要在`paddr_read()`和`paddr_write()`中进行记录即可。

开启 mtrace 将会产生大量的输出，因此最好可以在不需要的时候关闭 mtrace。文档暗示了我们参考 itrace 的实现，实际上就是添加一个编译选项，仅当我们设置输出时才会打印信息。在 Kconfig 文件中 `ITRACE` 下添加 

```Kconfig
config MTRACE
  depends on TRACE
  bool "Enable memory tracer"
  default n
```

这样就可以在 `make menuconfig` 时进行设置了。mtrace 的实现也很简单

```c
void display_pread(paddr_t addr, int len) {
  printf(ANSI_FG_CYAN "MTRACE: pread at " FMT_PADDR " len=%d\n" ANSI_NONE, addr, len);
}

void display_pwrite(paddr_t addr, int len, word_t data) {
  printf(ANSI_FG_CYAN "MTRACE: pwrite at " FMT_PADDR " len=%d, data=" FMT_WORD "\n" ANSI_NONE, addr, len, data);
}
```

然后在 `paddr.c` 中添加

```c
word_t paddr_read(paddr_t addr, int len) {
  IFDEF(CONFIG_MTRACE, display_pread(addr, len));
  if (likely(in_pmem(addr))) return pmem_read(addr, len);
  IFDEF(CONFIG_DEVICE, return mmio_read(addr, len));
  out_of_bound(addr);
  return -1;
}

void paddr_write(paddr_t addr, int len, word_t data) {
  IFDEF(CONFIG_MTRACE, display_pwrite(addr, len, data));
  if (likely(in_pmem(addr))) { pmem_write(addr, len, data); return; }
  IFDEF(CONFIG_DEVICE, mmio_write(addr, len, data); return);
  out_of_bound(addr);
}
```

## ftrace

itrace 和 mtrace 是底层状态机视角的追踪，无法体现程序中包含的语义行为。因为在程序中，函数显然是携带语义信息的，这就需要实现一个 ftrace 工具，追踪程序执行过程中的函数调用和返回。

>   这其实并不困难, 因为itrace已经能够追踪程序执行的所有指令了, 要实现ftrace, 我们只需要关心函数调用和返回相关的指令就可以了. 我们可以在函数调用指令中记录目标地址, 表示将要调用某个函数; 然后在函数返回指令中记录当前PC, 表示将要从PC所在的函数返回. 我们很容易在相关指令的实现中添加代码来实现这些功能. 但目标地址和PC值仍然缺少程序语义, 如果我们能把它们翻译成函数名, 就更容易理解了!

我们需要根据 ELF 文件中的符号表（symbol table）来通过代码段地址得到它对应的函数。

以 `cpu-tests` 中 `add` 这个用户程序为例：

```shell
riscv64-linux-gnu-readelf -a add-riscv32-nemu.elf
```

这个输出对于理解 ELF 文件非常有用，但是现在只需要关心符号表的信息

```
Symbol table '.symtab' contains 35 entries:
   Num:    Value  Size Type    Bind   Vis      Ndx Name
     0: 00000000     0 NOTYPE  LOCAL  DEFAULT  UND
     1: 80000000     0 SECTION LOCAL  DEFAULT    1 .text
     2: 80000128     0 SECTION LOCAL  DEFAULT    2 .rodata
```

在这里找到 `Type` 属性为 `FUNC` 的表项，给出了关于程序中的函数的信息：

```
    16: 80000108    32 FUNC    GLOBAL HIDDEN     1 _trm_init
    25: 80000010    24 FUNC    GLOBAL HIDDEN     1 check
    27: 80000000    16 FUNC    GLOBAL DEFAULT    1 _start
    29: 80000028   212 FUNC    GLOBAL HIDDEN     1 main
    33: 800000fc    12 FUNC    GLOBAL HIDDEN     1 halt
```

观察这里的函数正是系统运行 C 程序调用的函数。

>   什么才是真正的*符号*（symbol）？
>
>   [这篇文章](https://intezer.com/blog/malware-analysis/executable-linkable-format-101-part-2-symbols/)讨论了 ELF 文件中的符号。代码中的函数或变量会保存为文件的符号信息，在编译成机器码时作为符号引用标识地址和偏移量。链接器通常与符号表交互，以便在链接时匹配/引用/修改 ELF 对象中的给定符号。

查看 readelf 中的 Header Section：

```elf
Section Headers:
  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  [ 0]                   NULL            00000000 000000 000000 00      0   0  0
  [ 1] .text             PROGBITS        80000000 001000 000128 00  AX  0   0  4
  [ 2] .rodata           PROGBITS        80000128 001128 000040 00   A  0   0  4
  [ 3] .data.ans         PROGBITS        80000168 001168 000100 00  WA  0   0  4
  [ 4] .data.test_data   PROGBITS        80000268 001268 000020 00  WA  0   0  4
  [ 5] .comment          PROGBITS        00000000 001288 00002b 01  MS  0   0  1
  [ 6] .riscv.attributes RISCV_ATTRIBUTE 00000000 0012b3 00001f 00      0   0  1
  [ 7] .symtab           SYMTAB          00000000 0012d4 000230 10      8  16  4
  [ 8] .strtab           STRTAB          00000000 001504 0000a6 00      0   0  1
  [ 9] .shstrtab         STRTAB          00000000 0015aa 00005e 00      0   0  1
```

可以看到字符串表 .strtab 位于偏移 0x1504 处，使用 `hd` 查看 elf 文件的十六进制编码格式，找到对应位置的编码

```shell
hd add-riscv32-nemu.elf
```

```
00001500  11 02 04 00 00 73 74 61  72 74 2e 6f 00 24 78 00  |.....start.o.$x.|
00001510  61 64 64 2e 63 00 74 72  6d 2e 63 00 6d 61 69 6e  |add.c.trm.c.main|
00001520  61 72 67 73 00 5f 74 72  6d 5f 69 6e 69 74 00 5f  |args._trm_init._|
...
```

要做的显而易见了：解析 ELF 文件，找到 `.symtab` 中所有函数符号，根据 `.strtab` 中的地址信息寻找对应地址的对应函数，将函数符号与函数名字符串联系起来，输出具有语义的函数调用信息。

# AM

就像我们都希望写有标准答案的试卷一样，如果单纯在自己实现的 NEMU 上运行程序，总对程序是否正确不是很有信心。如果要找一个系统的标准答案，那自然是我们本机的真实硬件，显然真实的硬件实现必然是要被相信是正确的。`abstract-machine`中有一个特殊的架构叫`native`，是用GNU/Linux默认的运行时环境来实现的AM API，这就是提供的标准答案。 

>   **如何生成native的可执行文件**
>
>   很简单，关键代码是
>
>   ```makefile
>   ### Rule (link): objects (*.o) and libraries (*.a) -> IMAGE.elf, the final ELF binary to be packed into image (ld)
>   $(IMAGE).elf: $(LINKAGE) $(LDSCRIPTS)
>   	@echo \# Creating image [$(ARCH)]
>   	@echo + LD "->" $(IMAGE_REL).elf
>   ifneq ($(filter $(ARCH),native),)
>   	@$(CXX) -o $@ -Wl,--whole-archive $(LINKAGE) -Wl,-no-whole-archive $(LDFLAGS_CXX)
>   else
>   	@$(LD) $(LDFLAGS) -o $@ --start-group $(LINKAGE) --end-group
>   endif
>   ```
>
>   -   如果选择本地架构（`native`），则使用 `C++` 编译器 (`$(CXX)`) 进行链接，并且使用 `--whole-archive` 强制链接所有库。
>
>   -   如果选择交叉编译（不是本地架构），则使用 `ld` 链接器进行链接，并通过 `--start-group` 和 `--end-group` 确保库的正确链接。
>
>   目标是生成的 ELF 文件，这一步则是最后的链接规则。

>   **奇怪的错误码**
>
>   `make` 通过在执行每个命令时检查命令的退出状态来确定是否有错误发生。具体步骤如下：
>
>   -   每个命令执行完毕后，系统会返回一个退出状态码。
>   -   `make` 会检查命令的退出码。如果返回值是非零，表示命令执行失败，`make` 会停止执行当前目标的后续命令，并将错误码传递给上级。
>
>   ```makefile
>   $(IMAGE).elf: $(LINKAGE) $(LDSCRIPTS)
>   	@echo "Creating image"
>   	@$(LD) $(LDFLAGS) -o $@ --start-group $(LINKAGE) --end-group
>   ```
>
>   在上面的代码中，如果 `$(LD)` 命令失败，`make` 会捕获该失败并返回退出码 `1`（或其他非零值）。

# Differential Testing

简单来说就是对比标准答案，看有哪些不同（我可太擅长干这事了）在软件测试领域称为[differential testing](https://en.wikipedia.org/wiki/Differential_testing)。要检查指令的实现是否正确，只要检查执行指令之后DUT和REF的状态是否一致就可以了。

>   通常来说, 进行DiffTest需要提供一个和DUT(Design Under Test, 测试对象) 功能相同但实现方式不同的REF(Reference, 参考实现), 然后让它们接受相同的有定义的输入, 观测它们的行为是否相同.

在 menuconfig 中打开 Enable differential testing，然后重新编译NEMU并运行即可，NEMU的配置系统会根据ISA选择合适的模拟器作为REF，RISC-V 选用 Spike，RISC-V 社区的一个全系统模拟器。

```bash
sudo apt install device-tree-compiler -y
```

我们需要实现的是在`nemu/src/isa/$ISA/difftest/dut.c`中定义的 `isa_difftest_checkregs()`函数：

```c
bool isa_difftest_checkregs(CPU_state *ref_r, vaddr_t pc) {
  int reg_num = ARRLEN(cpu.gpr);
  for (int i = 0; i < reg_num; i++) {
    if (ref_r->gpr[i] != cpu.gpr[i]) {
      return false;
    }
  }
  if (ref_r->pc != cpu.pc) {
    return false;
  }
  return true;
}
```

# Regression Testing

当我们通过某个实例，又添加了某项功能时，我们并不知道新功能是否会对系统原有功能产生影响，这就需要再运行一遍做过的测试，这个过程称为[回归测试](https://en.wikipedia.org/wiki/Regression_testing)。`cpu-tests`提供了一键回归测试的功能:

```bash
make ARCH=$ISA-nemu run
```



>   NEMU是一个用来执行其它程序的程序. 在可计算理论中, 这种程序有一个专门的名词, 叫通用程序(Universal Program), 它的通俗含义是: 其它程序能做的事情, 它也能做. 通用程序的存在性有专门的证明. 
>
>   我们可以在计算机上做各种各样的事情, 其背后都蕴含着通用程序的思想: NEMU和各种模拟器只不过是通用程序的实例化, 我们也可以毫不夸张地说, 计算机就是一个通用程序的实体化. 通用程序的存在性为计算机的出现奠定了理论基础, 是可计算理论中一个极其重要的结论, 如果通用程序的存在性得不到证明, 我们就没办法放心地使用计算机, 同时也不能义正辞严地说"机器永远是对的".
>
>   在1983年, [Martin Davis教授](http://en.wikipedia.org/wiki/Martin_Davis)就在他出版的"Computability, complexity, and languages: fundamentals of theoretical computer science" 一书中提出了一种仅有三种指令的程序设计语言L语言, 并且证明了L语言和其它所有编程语言的计算能力等价. L语言中的三种指令分别是:
>
>   ```
>   V = V + 1
>   V = V - 1
>   IF V != 0 GOTO LABEL
>   ```
>
>   Martin Davis教授还证明了, 在不考虑物理限制的情况下(认为内存容量无限多, 每一个内存单元都可以存放任意大的数), 用L语言也可以编写出一个和NEMU类似的通用程序! 而且这个用L语言编写的通用程序的框架, 竟然还和NEMU中的`cpu_exec()`函数如出一辙: 取指, 译码, 执行... 这其实并不是巧合, 而是[模拟(Simulation)](http://en.wikipedia.org/wiki/Simulation#Computer_science)在计算机科学中的应用.
>
>   早在Martin Davis教授提出L语言之前, 科学家们就已经在探索什么问题是可以计算的了. 回溯到19世纪30年代, 为了试图回答这个问题, 不同的科学家提出并研究了不同的计算模型, 包括[Gödel](http://en.wikipedia.org/wiki/Godel), [Herbrand](http://en.wikipedia.org/wiki/Jacques_Herbrand)和[Kleen](http://en.wikipedia.org/wiki/Stephen_Cole_Kleene)研究的[递归函数](http://en.wikipedia.org/wiki/Μ-recursive_function), [Church](http://en.wikipedia.org/wiki/Alonzo_Church)提出的[λ-演算](http://en.wikipedia.org/wiki/Lambda_calculus), [Turing](http://en.wikipedia.org/wiki/Alan_Turing)提出的[图灵机](http://en.wikipedia.org/wiki/Turing_machine), 后来发现这些模型在计算能力上都是等价的; 到了40年代, 计算机就被制造出来了. 后来甚至还有人证明了, 如果使用无穷多个算盘拼接起来进行计算, 其计算能力和图灵机等价! 我们可以从中得出一个推论, 通用程序在不同的计算模型中有不同的表现形式. 计算的极限](https://zhuanlan.zhihu.com/p/270155475)这一系列科普文章叙述了可计算理论的发展过程

---

> **捕捉死循环**
> 
> 当用户程序陷入死循环时, 让用户程序暂停下来, 并输出相应的提示信息
>
> 应该如何实现? 
