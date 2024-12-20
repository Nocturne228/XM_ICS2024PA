# RTFM & RTFSC

由于我们希望将程序编译成 NEMU 环境中可执行的文件，但是直接使用 gcc 工具将会默认根据 GNU/Linux 运行时环境编译，无法在 NEMU 中直接运行。这里会用到在嵌入式开发等领域中应用到的[交叉编译](http://en.wikipedia.org/wiki/Cross_compiler)，即在开发环境将源代码编译成目标环境运行的文件。

大致过程是：先将 AM 项目和 AM 中的运行库编译并归档，再将客户程序编译成目标文件 `.o`，最后根据 Makefile中的信息进行链接。

---

在 `am-kernels/tests/cpu-tests/ `目录下执行

```shell
make ARCH=riscv32-nemu ALL=dummy run
```

编译  `./cpu-tests/tests/dummy.c` 程序并在 NEMU 中执行。程序会报错：

```error
invalid opcode(PC = 0x80000000):
        13 04 00 00 17 91 00 00 ...
        00000413 00009117...
```

打开反汇编结果 `am-kernels/tests/cpu-tests/build/dummy-riscv32-nemu.txt` 文件找到对应位置的指令并查阅手册，在 NEMU 中实现。

# 实现指令

实际上指令的实现是挨个编译运行测试，找到报错位置的指令去实现，不过这里就换个顺序，总结各个指令类型的实现。

首先查阅手册可知指令有 R, I, S, B, U, J 六种类型，NEMU 另外定义了 N 类型作为没有含义的指令。使用 `enum` 定义各个类型后，实现各类型立即数的提取。以 B 和 J 类型为例，需要查阅手册，找到指令中各个位置的数据在对应类型指令的立即数组成中的位置，使用 `BITS` 读取， `<<` 移位，然后位或  `|`  进行拼接。最后要注意手册中要求 RISC-V 的指令是2字节对齐，也就是立即数必须为偶数。所以在移位后，整体还要左移1位，最后使用 `SEXT` 进行有符号位扩展。

然后是 `decode_oprand` 函数的实现。`rd` 只要存在于指令中，必然位于固定的位置。这里要做的是根据手册，根据各类型指令源操作数和立即数的有无和个数进行提取。例如对于 S 类型指令，就需要调用 `src1R`、`src2R`、`immS`提取各数据。而对于 U 和 J 类型指令只需要提取对应的立即数即可。

最后就是 `INSTPAT` 中的内容了，按照指令表的内容一一填写即可。这里以 `jal` 和 `jalr` 为例，偷懒没有参考官方手册内容（东西太多了），而是参考了 [RISCV_CARD](https://www.cs.sfu.ca/~ashriram/Courses/CS295/assets/notebooks/RISCV/RISCV_CARD.pdf) 和网站 [RISC-V Guide](https://marks.page/riscv/)。由于学过嵌入式课程，对于一些名词还是能理解的。真正的原版应该The RISC-V Instruction Set Manual ([Volume I](https://github.com/riscv/riscv-isa-manual/releases/download/Priv-v1.12/riscv-privileged-20211203.pdf), [Volume II](https://github.com/riscv/riscv-isa-manual/releases/download/Ratified-IMAFDQC/riscv-spec-20191213.pdf)) And [ABI for riscv](https://github.com/riscv-non-isa/riscv-elf-psabi-doc)

| Notation   | Description                               |
| :--------- | :---------------------------------------- |
| **pc**     | program counter                           |
| **rd**     | integer register destination              |
| **rsN**    | integer register source N                 |
| **imm**    | immediate operand value                   |
| **offset** | immediate program counter relative offset |

| Format               | Name                   | Pseudocode                |
| :------------------- | :--------------------- | :------------------------ |
| `JAL rd,offset`      | Jump and Link          | rd = PC+4; PC += imm      |
| `JALR rd,rs1,offset` | Jump and Link Register | rd = PC+4; PC = rs1 + imm |

还是贴上手册的描述……

> The jump and link (JAL) instruction uses the J-type format, where the J-immediate encodes a signed offset in multiples of 2 bytes. The offset is sign-extended and added to the address of the jump instruction to form the jump target address. JAL stores the address of the instruction following the jump (pc+4) into register rd. 
>
> The indirect jump instruction JALR (jump and link register) uses the I-type encoding. The target address is obtained by adding the sign-extended 12-bit I-immediate to the register rs1, then setting the least-significant bit of the result to zero. The address of the instruction following the jump (pc+4) is written to register rd. Register x0 can be used as the destination if the result is not required.

`jal` 指令采用 J 类型编码格式，表示无条件跳转到当前 pc +  offset 位置处；`jalr` 采用 I 类型编码格式，表示跳转到 rs1 中的内容与符号扩展后的立即数相加，然后将最小有效位置0得到的结果处。由于跳转位置由 `s->dnpc` 决定，所以这两个指令就是对 `dnpc` 的操作。

那么这两条指令实现如下：

```c
INSTPAT("??????? ????? ????? ??? ????? 11011 11", jal, J, s->dnpc = s->pc + imm; R(rd) = s->pc + 4);
INSTPAT("??????? ????? ????? 000 ????? 11001 11", jalr, I, s->dnpc = (src1 + imm) & ~(word_t)1; R(rd) = s->pc + 4);
```

根据不靠谱网站上的内容，`jalr` 的末位一开始没有置零，但是也能通过测试，英文手册都没有置零的伪代码描述，不过我在[RISC-V 手册](http://riscvbook.com/chinese/RISC-V-Reader-Chinese-v2p1.pdf)这本中文手册中找到了各指令的相对准确的伪代码实现，不过翻译稍有问题。所以结合这几方资料，实现测试集所需指令还是很容易的。

到此为止除了 `hello-str` 和 `string` 中需要 AM 中的 stdio 和 string 库之外，其他测试文件都可以 HIT GOOD TRAP。

> PA2.1 DONE∎

# Ref:

[RISCV_CARD (*Important)](https://www.cs.sfu.ca/~ashriram/Courses/CS295/assets/notebooks/RISCV/RISCV_CARD.pdf)

[RISC-V Instruction Set Specifications](https://msyksphinz-self.github.io/riscv-isadoc/html/index.html)

[An Embedded RISC-V Blog](https://five-embeddev.com/riscv-user-isa-manual/Priv-v1.12/instr-table.html)

[RISC-V Instruction Set Reference](https://marks.page/riscv/)

中文内容

[RISC-V 手册](http://riscvbook.com/chinese/RISC-V-Reader-Chinese-v2p1.pdf) 

[RISC-V指令集手册 第一卷：用户级ISA ](http://file.whycan.com/files/members/1390/riscv-spec-v2_2_cn.pdf)



![format](https://five-embeddev.com/riscv-user-isa-manual/latest-latex/rv32_03.svg)



![isa-mannual](https://five-embeddev.com/riscv-user-isa-manual/Priv-v1.12/instr-table_00.svg)
