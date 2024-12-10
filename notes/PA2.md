# PA2



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

其执行加法过程的状态机如下：

```
+--------------------------+
|   State 0: Init          |  -->  pc = 0, R[0] = 0, R[1] = 0, M[5] = 16, M[6] = 33
+--------------------------+
           |
           v
+--------------------------+
|   State 1: load y        |  -->  Load M[6] = 33 into R[0]
+--------------------------+
           |
           v
+--------------------------+
|   State 2: mov r1, r0    |  -->  Copy R[0] to R[1], R[1] = 33
+--------------------------+
           |
           v
+--------------------------+
|   State 3: load x        |  -->  Load M[5] = 16 into R[0]
+--------------------------+
           |
           v
+--------------------------+
|   State 4: add r0, r1    |  -->  R[0] = R[0] + R[1], R[0] = 49
+--------------------------+
           |
           v
+--------------------------+
|   State 5: store z       |  -->  Store R[0] = 49 into M[7]
+--------------------------+
           |
           v
+--------------------------+
|   State 6: End           |  -->  Output M[7] = 49
+--------------------------+

```

