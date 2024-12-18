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

YEMU是一个简单的CPU模拟器，实现了类似下面的指令：

```
                                                     4  2  0
            |                        |        | +----+--+--+
mov   rt,rs | R[rt] <- R[rs]         | R-type | |0000|rt|rs|
            |                        |        | +----+--+--+
            |                        |        | +----+--+--+
add   rt,rs | R[rt] <- R[rs] + R[rt] | R-type | |0001|rt|rs|
            |                        |        | +----+--+--+
            |                        |        | +----+--+--+
load  addr  | R[0] <- M[addr]        | M-type | |1110| addr|
            |                        |        | +----+--+--+
            |                        |        | +----+--+--+
store addr  | M[addr] <- R[0]        | M-type | |1111| addr|
            |                        |        | +----+--+--+
```

分析程序的源码可以看到：

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
