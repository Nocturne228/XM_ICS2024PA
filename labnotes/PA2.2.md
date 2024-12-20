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