# PA0

官方文档提示安装的包如下

```bash
sudo apt install build-essential    # build-essential packages, include binary utilities, gcc, make, and so on
sudo apt install man                # on-line reference manual
sudo apt install gcc-doc            # on-line reference manual for gcc
sudo apt install gdb                # GNU debugger
sudo apt install git                # revision control system
sudo apt install libreadline-dev    # a library used later
sudo apt install libsdl2-dev        # a library used later

```

过程中可能会出现错误或依赖问题，观察报错信息，安装缺失的包，有时需要更换一下源就可以了。

开发过程中还会用到很多著名且强大的工具，如**git**、**gdb**、**make**等等等等，关于gdb的使用可以阅读教程https://linuxconfig.org/gdb-debugging-tutorial-for-beginners；

> 关于UNIX哲学
>
> 文档给出一个例子：**如何列出一个C语言项目中所有被包含过的头文件?**
>
> ```shell
> find . -name "*.[ch]" | xargs grep "#include" | sort | uniq
> ```
>
> - 每个工具只做一件事情, 但做到极致
> - 工具采用文本方式进行输入输出, 从而易于使用
> - 通过工具之间的组合来解决复杂问题
>
> Unix哲学的最后一点最能体现Linux和Windows的区别: 编程创造. 如果把工具比作代码中的函数, 工具之间的组合就是一种编程。

## 初步

下面是从零开始获取项目源代码的流程

```shell
git clone -b 2024 git@github.com:NJU-ProjectN/ics-pa.git ics2024
cd ics2024
git branch -m master
bash init.sh nemu
bash init.sh abstract-machine
source ~/.bashrc
```

最后一步是将子项目NEMU和Abstract-Machine的路径写入环境变量，尝试下面的命令以检查效果：

```shell
echo $NEMU_HOME
echo $AM_HOME
cd $NEMU_HOME
cd $AM_HOME
```

如果是迁移项目代码，则可以输入

```shell
echo 'export NEMU_HOME="$(pwd)/nemu"' >> ~/.bashrc
echo 'export AM_HOME="$(pwd)/abstract-machine"' >> ~/.bashrc
source ~/.bashrc
```

ICS PA通过Makefile来追踪学生的实际开发过程，大致原理是每次`make`、`make run`和`make gdb`，即编译、运行和调试时执行`git commit`，通过git记录来进行Integrity Check。我们可以删除根目录`Makefile`文件中的下面内容来避免`git commit`记录过多：

```diff
diff --git a/Makefile b/Makefile
index c9b1708..b7b2e02 100644
--- a/Makefile
+++ b/Makefile
@@ -9,6 +9,6 @@
 define git_commit
-  -@git add .. -A --ignore-errors
-  -@while (test -e .git/index.lock); do sleep 0.1; done
-  -@(echo "> $(1)" && echo $(STUID) $(STUNAME) && uname -a && uptime) | git commit -F - $(GITFLAGS)
-  -@sync
+# -@git add .. -A --ignore-errors
+# -@while (test -e .git/index.lock); do sleep 0.1; done
+# -@(echo "> $(1)" && echo $(STUID) $(STUNAME) && uname -a && uptime) | git commit -F - $(GITFLAGS)
+# -@sync
 endef
```



PA0的git记录：

```shell
git checkout -b pa0
git add .
git commit -m "Init commit"
```

# 编译

进入`nemu/`目录，执行

```shell
make menuconfig
```

将会自动根据选项生成对应的头文件、宏定义等配置项，这也是Linux内核编译的第一步。这里注意选择选项Build Options->[*] Enable debug information，以便于后续的调试。

接着进行编译和运行

```shell
make -j 4	# 四个线程并发编译来加速（一般虚拟机都是分配4核）
make run
# make gdb		编译模式
# make clean	清理编译产生的文件（位于nemu/build/)
```



> PA0 DONE∎
