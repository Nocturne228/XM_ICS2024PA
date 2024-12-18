# PA1.01 (RTFSC!)

项目代码的结构已展示在上一篇文章中，这里主要是框架部分具体代码的初探。

## ISA

NEMU分离了ISA无关的基本框架和ISA相关的具体实现，体现抽象的思想: 框架代码将ISA之间的差异抽象成API, 基本框架会调用这些API, 从而无需关心ISA的具体细节。如果将来使用不同的ISA，将会发现：基础框架代码完全不需要修改！

 [NEMU ISA相关的API说明文档](https://nju-projectn.github.io/ics-pa-gitbook/ics2024/nemu-isa-api.html)

## 配置

NEMU中的配置系统位于`nemu/tools/kconfig`, 它来源于GNU/Linux项目中的kconfig，用于指定一些可配置选项。目前我们只需要关心配置系统生成的如下文件:

- `nemu/include/generated/autoconf.h`, 阅读C代码时使用
- `nemu/include/config/auto.conf`, 阅读Makefile时使用

## 构建

NEMU的Makefile非常精巧，它会关联配置系统生成的变量（即包含`auto.conf`文件），根据子目录下的一些`filelist.mk`文件中维护的变量选择参与编译的源文件，从而达到根据配置项进行不同的编译的效果。

Makefile的编译规则在`nemu/scripts/build.mk`中定义:

```makefile
$(OBJ_DIR)/%.o: %.c
  @echo + CC $<
  @mkdir -p $(dir $@)
  @$(CC) $(CFLAGS) -c -o $@ $<
  $(call call_fixdep, $(@:.o=.d), $@)
```

可以键入`make -nB`, 它会让`make`程序以"只输出命令但不执行"的方式强制构建目标，用来分析编译过程。

> PA定义了很多神器而又难懂的宏：
>
> `nemu/include/macro.h`中定义了一些专门用来对宏进行测试的宏. 例如`IFDEF(CONFIG_DEVICE, init_device());`表示, 如果定义了`CONFIG_DEVICE`, 才会调用`init_device()`函数; 而`MUXDEF(CONFIG_TRACE, "ON", "OFF")`则表示, 如果定义了`CONFIG_TRACE`, 则预处理结果为`"ON"`(`"OFF"`在预处理后会消失), 否则预处理结果为`"OFF"`.
>
> 这些宏会帮助开发、调试，后续会慢慢接触。

# NEMU

NEMU主要由4个模块构成：monitor，CPU，memory，设备。Monitor（监视器）模块是为了方便地监控客户计算机的运行状态而引入的. 它除了负责与GNU/Linux进行交互（例如读入客户程序）之外, 还带有调试器的功能, 为NEMU的调试提供了方便的途径.

## 客户程序

NEMU是用来执行程序的程序，准确来说是执行客户程序的程序。也就是说，需要读入外部的程序到计算机中，这项任务由monitor负责。

在`nemu/src/monitor/monitor.c`中，可以看到函数`init_monitor()`完成对monitor的初始化,其内部是四个函数调用：`parse_args()`, `init_rand()`, `init_log()`和`init_mem()`

> `init_monitor()`的代码中都是函数调用，我想好处可能有：可读性、可拓展性、便于调试时的错误定位。

值得注意的是monitor初始化过程调用的`parse_args()`，可以看到首先使用一个结构体table对参数表进行定义，然后调用`getopt_long()`函数进行解析，关于此函数的手册可查看`man 3 getopt_long`。

然后是`init_isa()`进行ISA相关的初始化工作。初始化的行为即将内置的客户程序读入到内存的事先约定好的地址，即让monitor直接把客户程序读入到一个固定的内存位置（使用`RESET_VECTOR`定义）

上述关于内存工作的代码位于`nemu/include/memory/paddr.c`。

`init_isa()`还通过调用`restart()`对寄存器进行了初始化，阅读源码可知行为是将PC（`cpu.pc`）也设置为`RESET_VECTOR`。这是简单粗暴的提前约定，这样就能确定客户程序读入到内存的位置，也方便CPU执行客户程序。在真实世界中，会有专门的BIOS负责这项工作。对于RISCV32，寄存器结构体`CPU_state`的定义位于`nemu/src/isa/riscv32/include/isa-def.h`，而CPU则作为全局变量在`nemu/src/cpu/cpu-exec.c`中定义。

> 将PC设置成刚才加载客户程序的内存位置, 这样就可以让CPU从我们约定的内存位置开始执行客户程序了。而在NEMU中，我们用`nemu/src/memory/paddr.c`中定义的连续的数组`pmem`模拟128MB的物理内存。
>
> riscv32的物理地址从`0x80000000`开始编址，因此需要进行地址映射，将CPU将要访问的内存地址映射到`pmem`中的相应偏移位置（定义为宏`CONFIG_MBASE`）。此工作由`guest_to_host()`完成，在ISA的初始化中也有它的身影。
>
> 经过地址映射，如果CPU想要访问`pmem[0]`，将会被定位到0x80000000。

> 注意，`RESET_VECTOR`定义了事先约定好的客户程序读入的位置，而`CONFIG_MBASE`则是针对不同架构的地址映射设置的相对偏移量。

---

Monitor读入客户程序并对寄存器进行初始化后, 这时内存的布局如下:

```
pmem:

CONFIG_MBASE      RESET_VECTOR
      |                 |
      v                 v
      -----------------------------------------------
      |                 |                  |
      |                 |    guest prog    |
      |                 |                  |
      -----------------------------------------------
                        ^
                        |
                       pc
```

这里也可以看到，`CONFIG_MBASE`代表存储空间的起始位置，而`RESET_VECTOR`则是存储空间内约定好的一个固定位置，客户程序被读入到这里，pc也被初始化到这里。

> NEMU返回到`init_monitor()`函数中, 继续调用`load_img()`函数 (在`nemu/src/monitor/monitor.c`中定义). 这个函数会将一个有意义的客户程序从[镜像文件](https://en.wikipedia.org/wiki/Disk_image)读入到内存, 覆盖刚才的内置客户程序. 这个镜像文件是运行NEMU的一个可选参数, 在运行NEMU的命令中指定. 如果运行NEMU的时候没有给出这个参数, NEMU将会运行内置客户程序.

总结：NEMU的第一步是初始化监视器，`init_monitor()`中进行了一系列的初始化。

# 运行

完成了Monitor的初始化后，主函数将调用`engine_start()`函数。查看源代码，会看到它进入了简易调试器sdb的主循环`sdb_mainloop()`。

（`engine_start()`在`nemu/src/engine/interpreter/init.c`中定义）

另外，sdb装载的客户程序如下，这段代码位于`nemu/src/isa/riscv32/init.c`，在后续`si`单步运行时可以看到执行的正是这里的指令。

```c
static const uint32_t img [] = {
  0x00000297,  // auipc t0,0
  0x00028823,  // sb  zero,16(t0)
  0x0102c503,  // lbu a0,16(t0)
  0x00100073,  // ebreak (used as nemu_trap)
  0xdeadbeef,  // some data
};
```

接下来重点分析sdb中执行的具体行为：输入`c`

在调试模式下，分析函数调用栈可以看到：

```c
(gdb) bt
#0  exec_once (s=s@entry=0x7fffffffda30, pc=2147483648)
    at src/cpu/cpu-exec.c:44
#1  0x000055555555741a in execute (n=n@entry=18446744073709551615)
    at src/cpu/cpu-exec.c:77
#2  0x00005555555574d4 in cpu_exec (n=n@entry=18446744073709551615)
    at src/cpu/cpu-exec.c:111
#3  0x00005555555579a7 in cmd_c (args=<optimized out>)
    at src/monitor/sdb/sdb.c:46
#4  0x0000555555557b41 in sdb_mainloop () at src/monitor/sdb/sdb.c:130
#5  0x0000555555556e5c in engine_start () at src/engine/interpreter/init.c:25
#6  0x00005555555565a0 in main (argc=<optimized out>, argv=<optimized out>)
    at src/nemu-main.c:32

```

这里可以清晰地看到执行一条指令的函数调用链：

从输入的`cmd_c`开始，`cpu_exec`->`execute`->`exec_once`。最后一个函数完成的工作就是让CPU执行当前PC指向的一条指令, 然后更新PC。

> -1在`unsigned int` 中代表最大数，`c`命令调用的`cpu_exec(-1)`则代表不停地执行程序直到程序停止。对应了gdb中c的行为。

> 三个对调试有用的宏(在`nemu/include/debug.h`中定义)
>
> - `Log()`是`printf()`的升级版, 专门用来输出调试信息, 同时还会输出使用`Log()`所在的源文件, 行号和函数. 当输出的调试信息过多的时候, 可以很方便地定位到代码中的相关位置
> - `Assert()`是`assert()`的升级版, 当测试条件为假时, 在assertion fail之前可以输出一些信息
> - `panic()`用于输出信息并结束程序, 相当于无条件的assertion fail

# 下马威

NEMU的第一个问题是根据断言报错信息找到对应位置的代码并删除，略。

如果在运行NEMU之后直接键入`q`退出, 你会发现终端输出了一些错误信息：

```
make: *** [/home/xiaoma/ics2024/nemu/scripts/native.mk:38: run] Error 1
```

看起来有点无厘头，因为找到这个位置的代码为：

```makefile
run: run-env
	$(call git_commit, "run NEMU")
->	$(NEMU_EXEC)
```

我们需要另辟蹊径，尝试其他方法。如果从`q`命令下手呢？查看`cmd_q`，它直接返回-1。进入调试模式，在`cmd_q`处打上断点，执行到这里然后单步跳入，发现了一个看起来与问题很有关的函数：`is_exit_status_bad()`

```c
(nemu) q

Breakpoint 1, cmd_q (args=0x0) at src/monitor/sdb/sdb.c:56
56      static int cmd_q(char *args) {
(gdb) s
58        return -1;
(gdb) s
sdb_mainloop () at src/monitor/sdb/sdb.c:213
213         return;
(gdb) s
main (argc=<optimized out>, argv=<optimized out>) at src/nemu-main.c:34
34        return is_exit_status_bad();
```

继续进入函数，查看源代码：

```c
int is_exit_status_bad() {
  int good = (nemu_state.state == NEMU_END && nemu_state.halt_ret == 0) ||
    (nemu_state.state == NEMU_QUIT);
  return !good;
}
```

这下清晰了，如果good为0，那么就会返回一个象征错误的1。而`nemu_state.state`应该为NEMU_QUIT，才能正常返回0。如果不放心，在这里打印`nemu_state.state`，可以看到值为1，对应NEMU_STOP而非NEMU_QUIT。

知道病根之后，只需要在`cmd_q()`的代码中添加一句`nemu_state.state = NEMU_QUIT;`就好了~

> PA1.01 DONE∎
