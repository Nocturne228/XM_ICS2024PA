# 设置默认编译器和编译选项
CC = gcc
CFLAGS = -Wall -g

# 源文件和目标文件的自动推导
SRC = yemu.c
OBJ = $(SRC:.c=.o)
EXEC = $(SRC:.c=)

# 默认目标：编译并生成可执行文件
all: $(EXEC)

# 编译规则：自动将源文件编译成目标文件，并在链接后删除 .o 文件
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 链接规则：将目标文件链接成可执行文件，并在完成后删除 .o 文件
$(EXEC): $(OBJ)
	$(CC) $(OBJ) -o $(EXEC)
	rm -f $(OBJ)  # 删除 .o 文件

# 运行目标
run: $(EXEC)
	./$(EXEC)

# 清理生成的文件
clean:
	rm -f $(OBJ) $(EXEC)

# 伪目标
.PHONY: all run clean
