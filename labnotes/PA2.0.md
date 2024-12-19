# PA2.0

## 一些问题

克隆`am-kernels`子项目：

```bash
cd ics2024
bash init.sh am-kernels
```

测试执行一个简单的C程序`am-kernels/tests/cpu-tests/tests/dummy.c`

以riscv32为例，需要安装对应的gcc和binutils：

```bash
sudo apt-get install g++-riscv64-linux-gnu binutils-riscv64-linux-gnu
```

在`am-kernels/tests/cpu-tests/`目录下执行

```bash
cd ./am-kernels/tests/cpu-tests/
make ARCH=$ISA-nemu ALL=dummy run
```

编译`dummy`程序时报告了如下错误:

```bash
/usr/riscv64-linux-gnu/include/bits/wordsize.h:28:3: error: #error "rv32i-based targets are not supported"
```

则需要使用sudo权限修改以下文件:

```diff
--- /usr/riscv64-linux-gnu/include/bits/wordsize.h
+++ /usr/riscv64-linux-gnu/include/bits/wordsize.h
@@ -25,5 +25,5 @@
 #if __riscv_xlen == 64
 # define __WORDSIZE_TIME64_COMPAT32 1
 #else
-# error "rv32i-based targets are not supported"
+# define __WORDSIZE_TIME64_COMPAT32 0
 #endif
```

如果报告的是如下错误:

```cmd
/usr/riscv64-linux-gnu/include/gnu/stubs.h:8:11: fatal error: gnu/stubs-ilp32.h: No such file or directory
```

则需要使用sudo权限修改以下文件:

```diff
--- /usr/riscv64-linux-gnu/include/gnu/stubs.h
+++ /usr/riscv64-linux-gnu/include/gnu/stubs.h
@@ -5,5 +5,5 @@
 #include <bits/wordsize.h>

 #if __WORDSIZE == 32 && defined __riscv_float_abi_soft
-# include <gnu/stubs-ilp32.h>
+//# include <gnu/stubs-ilp32.h>
 #endif
```

如果提示

```bash
/bin/sh: 1: python: not found
make[1]: *** [/home/xiaoma/Downloads/ics2024/abstract-machine/scripts/platform/nemu.mk:22: insert-arg] Error 127
```

说明系统只安装了`python3`，需要执行下面的命令创建一个 `python` 的符号链接，指向 `python3`

```bash
sudo ln -s /usr/bin/python3 /usr/bin/python
```

# YEMU

YEMU是一个简单的CPU模拟器，分析程序的源码可以看到一些有趣的事：

```c
#include <stdint.h>
#include <stdio.h>

#define NREG 4	// 4寄存器
#define NMEM 16	// 16字节内存

// 定义指令格式
typedef union {
  struct { uint8_t rs : 2，rt : 2，op : 4; } rtype;
  struct { uint8_t addr : 4      ，op : 4; } mtype;
  uint8_t inst;
} inst_t;

#define DECODE_R(inst) uint8_t rt = (inst).rtype.rt，rs = (inst).rtype.rs
#define DECODE_M(inst) uint8_t addr = (inst).mtype.addr

uint8_t pc = 0;       // PC，C语言中没有4位的数据类型，我们采用8位类型来表示
uint8_t R[NREG] = {}; // 寄存器
uint8_t M[NMEM] = {   // 内存，其中包含一个计算z = x + y的程序
  0b11100110， // load  6#     | R[0] <- M[y]
  0b00000100， // mov   r1，r0 | R[1] <- R[0]
  0b11100101， // load  5#     | R[0] <- M[x]
  0b00010001， // add   r0，r1 | R[0] <- R[0] + R[1]
  0b11110111， // store 7#     | M[z] <- R[0]
  0b00010000， // x = 16
  0b00100001， // y = 33
  0b00000000， // z = 0
};

int halt = 0; // 结束标志

// 执行一条指令
void exec_once() {
  inst_t this;
  this.inst = M[pc]; // 取指
  switch (this.rtype.op) {
  //  操作码译码       操作数译码           执行
    case 0b0000: { DECODE_R(this); R[rt]   = R[rs];   break; }
    case 0b0001: { DECODE_R(this); R[rt]  += R[rs];   break; }
    case 0b1110: { DECODE_M(this); R[0]    = M[addr]; break; }
    case 0b1111: { DECODE_M(this); M[addr] = R[0];    break; }
    default:
      printf("Invalid instruction with opcode = %x，halting...\n"，this.rtype.op);
      halt = 1;
      break;
  }
  pc++; // 更新PC
}

int main() {
  while (1) {
    exec_once();
    if (halt) break;
  }
  printf("The result of 16 + 33 is %d\n"，M[7]);
  return 0;
}
```

- 使用`union`定义了两种类型的指令格式：`rtype`和`mtype`，分别对应寄存器指令和内存指令。
- 定义了两个宏用于解码两种类型的机器码。
- 译码出op段进行匹配和执行
- 持续执行`exec_once`直到停机

可以看到 YEMU 采用了一个简单的8位长度指令的ISA，指令有 R 和 M 两种类型。首先定义了指令格式和组成，这样会方便后续的解码。解码出源寄存器和目的寄存器、操作符后，进行对应操作的实现。最后更新 PC。这段代码实际就是后需要完成的任务的简化版。我们要做的就是根据手册填写匹配规则，提取出指令中的信息，然后实现它。

# RTFSC

Refs：

- [这篇文章](http://blog.sciencenet.cn/blog-414166-1089206.html)叙述了RISC-V的理念以及成长的一些历史
- [这篇文章](http://blog.sciencenet.cn/blog-414166-763326.html)，里面讲述了一个从RISC世界诞生，到与CISC世界融为一体的故事
- [这里](http://cs.stanford.edu/people/eroberts/courses/soco/projects/risc/risccisc)有一篇对比RISC和CISC的小短文

`exec_once()`接受一个`Decode`类型的结构体指针`s`：`exec_once()`会先把当前的PC保存到`s`的成员`pc`和`snpc`中，其中`s->pc`就是当前指令的PC，而`s->snpc`则是下一条指令的PC

```c
typedef struct Decode {
  vaddr_t pc;
  vaddr_t snpc; // static next pc
  vaddr_t dnpc; // dynamic next pc
  ISADecodeInfo isa;
  IFDEF(CONFIG_ITRACE，char logbuf[128]);
} Decode;

typedef struct {
  uint32_t inst;
} MUXDEF(CONFIG_RV64，riscv64_ISADecodeInfo，riscv32_ISADecodeInfo);

static void exec_once(Decode *s，vaddr_t pc) {
  s->pc = pc;
  s->snpc = pc;
  isa_exec_once(s);
  cpu.pc = s->dnpc;
  ...
}
```

代码会调用`isa_exec_once()`函数，它会随着取指的过程修改`s->snpc`的值，使得从`isa_exec_once()`返回后`s->snpc`正好为下一条指令的PC。接下来代码将会通过`s->dnpc`来更新PC。

`isa_exec_once()`首先进行取指`ins_fetch()`，相当于对内存特定地址进行访问，也就是最后会调用`paddr_read()`，同时更新`s->snpc`。最后进入`decode_exec()`。

```c
int isa_exec_once(Decode *s) {
  s->isa.inst = inst_fetch(&s->snpc, 4);
  return decode_exec(s);
}
```

`decode_exec()`进行译码工作。

```c
static int decode_exec(Decode *s) {
  s->dnpc = s->snpc;

#define INSTPAT_INST(s) ((s)->isa.inst)
#define INSTPAT_MATCH(s, name, type, ... /* execute body */ ) { \
  int rd = 0; \
  word_t src1 = 0, src2 = 0, imm = 0; \
  decode_operand(s, &rd, &src1, &src2, &imm, concat(TYPE_, type)); \
  __VA_ARGS__ ; \
}
```

值得注意的是`s`中有三个`pc`：

- `pc`：显然是指向当前执行指令的`pc`
- `snpc`：Static next PC，指向下一条静态指令
- `sdpc`：Dynamic next PC，指向下一条动态指令

> 在程序分析领域中，静态指令是指程序代码中的指令，动态指令是指程序运行过程中的指令。例如对于以下指令序列
>
> ```asm
> 100: jmp 102
> 101: add
> 102: xor
> ```
>
> `jmp`指令的下一条静态指令是`add`指令，而下一条动态指令则是`xor`指令。对于跳转指令，`snpc`和`dnpc`会有所不同，`dnpc`应该指向跳转目标的指令。显然，我们应该使用`s->dnpc`来更新PC，并且在指令执行的过程中正确地维护`s->dnpc`。

---

总的来说，`exec_once`函数完成了一次取指、译码、执行的全部工作。它接受的是一个 `Decode`类型的结构体指针，气保焊执行指令所需的信息，主要是 PC 和 下一条指令的 PC，另外还包含 ISA 相关的信息，使用结构类型 `ISADecodeInfo` 抽象。函数调用 `isa_exec_once()` ，进行取指 `inst_fetch()` 和解码 `dexode_exec()`，解码完成后在 `exec_once()` 中使用 `s->dnpc` 更新 cpu 的 `pc`。

---

取指函数 `inst_fetch()` 调用了内存访问函数 `vaddr_ifetch()`，后者定义在 `vaddr.c` 中，调用 `paddr_read` 读取位于`snpc`处4字节长度的指令数据。

NEMU 使用了高度抽象的模式匹配来解码指令。模式匹配的过程由宏定义 `INSTPAT_START` 和 `INSTPAT_END` 包裹，其内部则是 `INSTPAT` 宏进行解码将 NEMU 程序中实现的 `auipc` 指令的部分代码进行宏展开可以得到：

```c
// *---- 展开前 ----
INSTPAT_START();
INSTPAT("??????? ????? ????? ??? ????? 00101 11", auipc, U, R(rd) = s->pc + imm);
// ...
INSTPAT_END();

// *---- 展开后 ----
{ const void * __instpat_end = &&__instpat_end_;
do {
  uint64_t key, mask, shift;
  pattern_decode("??????? ????? ????? ??? ????? 00101 11", 38, &key, &mask, &shift);
  if ((((uint64_t)s->isa.inst >> shift) & mask) == key) {
    {
      int rd = 0;
      word_t src1 = 0, src2 = 0, imm = 0;
      decode_operand(s, &rd, &src1, &src2, &imm, TYPE_U);
      R(rd) = s->pc + imm;
    }
    goto *(__instpat_end);
  }
} while (0);
// ...
__instpat_end_: ; }
```

这里提到 `&&__instpat_end_` 使用了 GCC 提供的[标签地址](https://gcc.gnu.org/onlinedocs/gcc/Labels-as-Values.html)扩展功能，即单目运算符 `&&` 可以获取定义在当前函数中的标签的地址，以便后面使用 `goto` 语句进行跳转。在这里，当指令成功匹配后，将会直接跳转到 `__instpar_end_` 位置，结束匹配尝试。

`INSTPAT` 宏的用法是如下：

```c
INSTPAT(模式字符串, 指令名称, 指令类型, 指令执行操作);
```

其中 `指令名称` 在代码中仅作为注释，`指令类型` 用于译码，`指令执行操作` 使用C语言代码模拟指令执行。

另外注意到三个变量：`__key`、`__mask`、`__shift`，通过参数可以知道在 `pattern_decode`函数中，`macro64(i)` 宏会将模式字符串中的 `0` `1` 会被提取到整型变量 `key` 中，表示指令中具体的 `0` `1` 值， 同时将对应数字的位置更新到掩码 `__mask` 中，来表示指令中有效位的位置，这样就可以实现提取确定的 `0` `1` 并模糊匹配 `?` 位置，最后 `__shift` 表示匹配过程中需要进行的位移数，可以看做是最后一个 `01` 相对最右侧需要左移的位置。举例：题目中的`"??????? ????? ????? ??? ????? 00101 11"`将会提取出 `key=0x17; //10111`，`mask=0x7f //1111111` ，`shift=0` （不需要位移）

```c
// --- pattern matching mechanism ---
__attribute__((always_inline))
static inline void pattern_decode(const char *str, int len,
    uint64_t *key, uint64_t *mask, uint64_t *shift) {
  uint64_t __key = 0, __mask = 0, __shift = 0;
#define macro(i) \
  if ((i) >= len) goto finish; \
  else { \
    char c = str[i]; \
    if (c != ' ') { \
      Assert(c == '0' || c == '1' || c == '?', \
          "invalid character '%c' in pattern string", c); \
      __key  = (__key  << 1) | (c == '1' ? 1 : 0); \
      __mask = (__mask << 1) | (c == '?' ? 0 : 1); \
      __shift = (c == '?' ? __shift + 1 : 0); \
    } \
  }

#define macro2(i)  macro(i);   macro((i) + 1)
#define macro4(i)  macro2(i);  macro2((i) + 2)
#define macro8(i)  macro4(i);  macro4((i) + 4)
#define macro16(i) macro8(i);  macro8((i) + 8)
#define macro32(i) macro16(i); macro16((i) + 16)
#define macro64(i) macro32(i); macro32((i) + 32)
  macro64(0);
  panic("pattern too long");
#undef macro
finish:
  *key = __key >> __shift;
  *mask = __mask >> __shift;
  *shift = __shift;
}

#define INSTPAT_INST(s) ((s)->isa.inst)
#define INSTPAT_MATCH(s, name, type, ... /* execute body */ ) { \
  int rd = 0; \
  word_t src1 = 0, src2 = 0, imm = 0; \
  decode_operand(s, &rd, &src1, &src2, &imm, concat(TYPE_, type)); \
  __VA_ARGS__ ; \
}

#define INSTPAT(pattern, ...) do { \
  uint64_t key, mask, shift; \
  pattern_decode(pattern, STRLEN(pattern), &key, &mask, &shift); \
  if ((((uint64_t)INSTPAT_INST(s) >> shift) & mask) == key) { \
    INSTPAT_MATCH(s, ##__VA_ARGS__); \
    goto *(__instpat_end); \
  } \
} while (0)
```

另外还通过 `decode_oprand()` 根据传入的指令类型解码并提取出了目的寄存器、两个源操作数和立即数，这些解析会在文件开头实现。框架代码定义了 `src1R()` 和 `src2R()` 辅助宏将寄存器的读取结果记录到相应的操作数变量中。 `immI()` 等宏用于提取各个类型的指令中的立即数。

`decode_operand` 函数中还用到了两个重要的宏  `BITS` 和 `SEXT` ，定义如下：

```c
#define BITMASK(bits) ((1ull << (bits)) - 1)
#define BITS(x, hi, lo) (((x) >> (lo)) & BITMASK((hi) - (lo) + 1)) // similar to x[hi:lo] in verilog
#define SEXT(x, len) ({ struct { int64_t n : len; } __x = { .n = x }; (uint64_t)__x.n; })
```

很容易可以看出，`BITS(x, hi, lo)` 表示提取 `x` 从高 `hi` 到低 `lo` 位的数字，而 `SEXT(x, len)` 则是将长度为 `len` 的数据进行符号拓展到64位（RISC-V32的指令是32位，但符号拓展后的高32位并不会产生影响）。

> **为什么有这么多的辅助宏和函数？**
>
> 简而言之：为了实现代码的解耦。
>
> 在一个系统中，显而易见地会存在许多逻辑相似的处理过程。以 ISA 为例，同样类型的指令会具有同样的译码过程，而同一指令的不同形式又会具有相同的执行过程。如果每条指令的每个形式都独立实现，则会产生大量的代码，尤其是开发者会自然地进行大量 copy-paste。这样的缺陷是如果 copy 的代码存在一处BUG，那么 所有相关的代码都需要修改，然而这样的 copy-paste 早就散落在项目的各处，难以修复和维护。

> 在这里，通过 `INSTPAT` 将指令的解码和执行进行解耦，分别编写译码和执行的代码并将其进行**组合**，大大提高了可读性和可维护性，简洁优美。

# 全过程

必答题：请整理一条指令在NEMU中的执行过程.

既然题目只问了指令的执行过程，关于客户程序的装载可以参考 PA1 RTFSC 部分。当 NEMU 通过 cpu 一路调用到 `exec_once` 后，更新 `s->pc` 和 `s->snpc` ，进入 `isa_exec_once(s)`。这一步的更新是后续指令执行的基础，即执行 PC 处的指令。为什么不更新 `s->dnpc` ？因为这是动态的下一条指令地址，需要等待指令本身进行更新，如果是 J(ump) 类型指令，`s->dnpc` 将指向跳转位置的指令而非代码文本中的下一条指令。

在 `isa_exec_once` 中进行取指，读取 `s->snpc` 位置的指令后，进入指令的解码 `decode_exec`。首先更新 `s->dnpc = s->snpc`，如果有跳转指令则会进一步更新。然后提取出 `Decode *s` 结构体指针 `s` 中的指令部分 `inst` ，再调用 `decode_operand` 提取出目的寄存器、两个源操作数、立即数以便后续的译码。提取完成后回到 `decode_exec` 函数，开始对指令进行模式匹配。具体匹配过程在上文描述，匹配并按照对应的具体执行过程的实现完成指令执行后，回到 `exec_once` ，接下来进行日志的记录，一次指令执行就完成了。

> PA2.0 DONE∎