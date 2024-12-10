# PA1.1

对于具有一定规模的项目，如果每次更新代码都只能完成走一遍编译构建运行的流程将会是巨大的额外时间开销，Debug更是难以想象。因此，项目需要一些基础设施来支撑。

SDB是NEMU的基础设施，并且由于其是NEMU的一部分，它可以很容易获取NEMU中运行的客户程序的信息，比如设置断点。我们需要为SDB实现下表的功能

| 命令         | 格式          | 使用举例          | 说明                                                         |
| ------------ | ------------- | ----------------- | ------------------------------------------------------------ |
| 帮助(1)      | `help`        | `help`            | 打印命令的帮助信息                                           |
| 继续运行(1)  | `c`           | `c`               | 继续运行被暂停的程序                                         |
| 退出(1)      | `q`           | `q`               | 退出NEMU                                                     |
| 单步执行     | `si [N]`      | `si 10`           | 让程序单步执行`N`条指令后暂停执行, 当`N`没有给出时, 缺省为`1` |
| 打印程序状态 | `info SUBCMD` | `info r` `info w` | 打印寄存器状态 打印监视点信息                                |
| 扫描内存(2)  | `x N EXPR`    | `x 10 $esp`       | 求出表达式`EXPR`的值, 将结果作为起始内存 地址, 以十六进制形式输出连续的`N`个4字节 |
| 表达式求值   | `p EXPR`      | `p $eax + 1`      | 求出表达式`EXPR`的值, `EXPR`支持的 运算请见[调试中的表达式求值](https://nju-projectn.github.io/ics-pa-gitbook/ics2024/1.6.html)小节 |
| 设置监视点   | `w EXPR`      | `w *0x2000`       | 当表达式`EXPR`的值发生变化时, 暂停程序执行                   |
| 删除监视点   | `d N`         | `d 2`             | 删除序号为`N`的监视点                                        |

# 解析命令

## 交互

NEMU通过`readline`库与用户进行交互，读取用户输入的命令。我们熟悉的通过上下方向键翻阅历史记录就是`readline()`提供的。

在这里可以分析一下NEMU是如何使用`readline()`的：这段代码位于`nemu/monitor/sdb/sdb.c`，我（GPT）自行添加了注释

```c
static char *rl_gets() {
  static char *line_read = NULL;	// 存储输入内容

  // 检查line_read是否有分配过的内存
  // C程序员是这样的
  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  // readline 会返回用户输入的一行文本，并自动为该文本分配内存。
  line_read = readline("(nemu) ");	// 提示符

  // 如果line_read及其解引用值非空，就加入历史记录
  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}
```



## 解析

输入的命令需要被解析才能确定该执行什么操作。解析的主要工作时识别出命令中的参数。可以使用`strtok()`进行处理，WHY老师在习题课上也有过演示。函数原型：

```c
char *strtok(char *restrict str, const char *restrict delim);
```

`str`是传入的字符串，`delim`则是需要进行分割的分隔符。根据文档，`strtok()`函数需要在第一次使用时传入字符串和分隔符，返回指向一个token的指针。之后的每次调用，它都会依次返回下一个token，直到no more tokens，返回`NULL`。这里特别注意的是，只有第一次调用`strtok()`时传入字符串，之后的每次调用都应该传入`NULL`才能持续解析同一个字符串。

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
  char *str1, *str2, *token, *subtoken;
  char *saveptr1, *saveptr2;
  int j;

  if (argc != 3) {
    fprintf(stderr, "Usage: %s string delim\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  char *tokstr = argv[1];
  char *tok = strtok(tokstr, " ");
  while (tok != NULL) {
    printf(" %s\n", tok);
    tok = strtok(NULL, " ");
  }
  exit(EXIT_SUCCESS);
}

// Run:
// $ ./strok "Hello World This is string" " "
// Output:
// Hello
// World
// This
// is
// string
```

当然还有很多字符串处理函数：

```cmd
man str<TAB><TAB>
```

还可以使用`sscanf()`，从字符串中读入格式化的内容。

```c
int sscanf(const char *restrict str,
                  const char *restrict format, ...);
```

`str`依然是待处理的字符串，而`format`则是格式字符串，和`printf`的格式化字符一样。`...`表示多个参数，根据对应格式存储对应的字符串解析出来的结果。如果输入字符串不符合格式化字符串，`sscanf` 会停止解析并返回已成功解析的项数。

```c
#include <stdio.h>

int main() {
    const char *datetime_str = "2024-12-07 14:30:00";
    int year, month, day, hour, minute, second;

    // 使用sscanf从字符串中提取年、月、日、小时、分钟、秒
    int ret = sscanf(datetime_str, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second);

    // 检查是否成功解析了所有的字段
    if (ret == 6) {
        printf("Parsed datetime:\n");
        printf("Year: %d\n", year);
        printf("Month: %d\n", month);
        printf("Day: %d\n", day);
        printf("Hour: %d\n", hour);
        printf("Minute: %d\n", minute);
        printf("Second: %d\n", second);
    } else {
        printf("Failed to parse the datetime string.\n");
    }

    return 0;
}
/*
Parsed datetime:
Year: 2024
Month: 12
Day: 7
Hour: 14
Minute: 30
Second: 0
*/
```



---

# Coding

## 单步执行

回忆之前分析过的`cmd_c()`命令，它调用了`cpu_exec(-1)`不断执行直到程序结束。即`cpu_exec()`的参数为执行的指令数，那么只需要解析出`si N`的参数作为数字调用`cpu_exec()`即可。

```c
static int cmd_si(char *args) {
  int step = 0;
  if (args == NULL) {
    step = 1;
  } else {
    sscanf(args, "%d", &step);  // scan in and parse
  }
  cpu_exec(step);
  return 0;
}
```

##  打印寄存器

寄存器是ISA相关的，我们需要去`nemu/src/isa/riscv32/reg.c`实现NEMU为我们准备好的API `void isa_reg_display()`。参考gdb的输出格式，我们格式化输出如下

```c
void isa_reg_display() {
  printf("%-15s%-17s%-15s\n", "Registers", "Hexadecimal", "Decimal");
  for(int i = 0; i < NR_REGS; ++i){
    printf("%-15s0x%-15x%-15u\n",regs[i], cpu.gpr[i], cpu.gpr[i]);
  }
}
```

`regs`数组在文件中已有定义，后面的`cpu.gpr`存储寄存器的值，

其定义位于`nemu/src/isa/riscv32/include/isa-def.h`

```c
typedef struct {
  word_t gpr[MUXDEF(CONFIG_RVE, 16, 32)];
  vaddr_t pc;
} MUXDEF(CONFIG_RV64, riscv64_CPU_state, riscv32_CPU_state);
```

## 扫描内存

API要求提供的是一个表达式，由于表达式求值尚未实现，这里可以先只实现对十六进制数的解析。在RTFSC时有提到对于内存的访问通过`paddr_read()`和`vaddr_read()`实现，在程序中我们自然要访问虚拟地址，即调用`vaddr_read()`。在下面的代码里我已经实现了表达式求值的函数`expr()`不过不影响扫描内存的代码：

```c
static int cmd_x(char *args) {
  char *cnt_str = strtok(args, " ");
  // printf("count received: %s\n", cnt_str);
  if (cnt_str != NULL) {
    int cnt = atoi(cnt_str);
    char *addr_str = strtok(NULL, "");

    bool success = false;
    // printf("addr received: %s\n", addr_str);
    int addr = expr(addr_str, &success);
    // printf("addr evaluated: 0x%x\n", addr);
    if (success == false) {
      printf("Invalid Expression\n");
      return 0;
    }

    printf("%-14s%-28s%-s\n", "Address", "Hexadecimal", "Decimal");
    for (int i = 0; i < cnt; ++i) {
      printf("0x%-12x0x%02x  0x%02x  0x%02x  0x%02x", (addr),
             vaddr_read(addr, 1), vaddr_read(addr + 1, 1),
             vaddr_read(addr + 2, 1), vaddr_read(addr + 3, 1));
      printf("\t  %04d  %04d  %04d  %04d\n", vaddr_read(addr, 1),
             vaddr_read(addr + 1, 1), vaddr_read(addr + 2, 1),
             vaddr_read(addr + 3, 1));
      addr += 4;
    }
  }
  return 0;
}
```



小小测试一下：还记得客户程序读入位置`RESET_VECTOR`吗，其值0x80000000正是客户程序开始执行的地址，另外读入的客户程序为：

```c
static const uint32_t img [] = {
  0x00000297,  // auipc t0,0
  0x00028823,  // sb  zero,16(t0)
  0x0102c503,  // lbu a0,16(t0)
  0x00100073,  // ebreak (used as nemu_trap)
  0xdeadbeef,  // some data
};
```

我们从0x80000000开始扫描5个字节的内存看看：

```cmd
Address       Hexadecimal                 Decimal
0x80000000    0x97  0x02  0x00  0x00      0151  0002  0000  0000
0x80000004    0x23  0x88  0x02  0x00      0035  0136  0002  0000
0x80000008    0x03  0xc5  0x02  0x01      0003  0197  0002  0001
0x8000000c    0x73  0x00  0x10  0x00      0115  0000  0016  0000
0x80000010    0xef  0xbe  0xad  0xde      0239  0190  0173  0222
```

果然和这个镜像文件对应上了！另外还可以注意到数据在内存中是小端存储的，在真实的计算机中也是如此，这点会在计组、体系结构、嵌入式等课程中学到。

> PA1.1 Done∎
