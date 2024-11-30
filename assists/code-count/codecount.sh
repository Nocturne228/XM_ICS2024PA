#!/bin/bash

# 设置默认目录为 /home/user/my_project
DIR="${1:-../nemu}"
# 设置默认文件扩展名过滤器
EXTENSIONS="${2:-*.c,*.h}"

# 将逗号分隔的扩展名转化为数组
IFS=',' read -r -a EXT_ARRAY <<< "$EXTENSIONS"

# 初始化行数和字节数
TOTAL_LINES=0
TOTAL_BYTES=0

# 打开或创建 counttrack 文件
OUTPUT_FILE="counttrack"

# 在 counttrack 文件中写入日期和传入的参数（注意：这里是追加内容，避免覆盖）
echo "" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"
echo "Date: $(date)" >> "$OUTPUT_FILE"
echo "Directory: $DIR" >> "$OUTPUT_FILE"
echo "Extensions: $EXTENSIONS" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# 遍历目录中的文件
for EXT in "${EXT_ARRAY[@]}"; do
    # 查找所有符合扩展名的文件并统计行数和字节数
    for FILE in $(find "$DIR" -type f -name "$EXT"); do
        # 统计文件的行数
        FILE_LINES=$(wc -l < "$FILE")
        # 统计文件的字节数
        FILE_BYTES=$(wc -c < "$FILE")

        # 累加总行数和字节数
        TOTAL_LINES=$((TOTAL_LINES + FILE_LINES))
        TOTAL_BYTES=$((TOTAL_BYTES + FILE_BYTES))

        # 打印每个文件的统计信息并写入到 counttrack 文件
        echo "$FILE - Lines: $FILE_LINES, Bytes: $FILE_BYTES" # | tee -a "$OUTPUT_FILE"
    done
done

# 打印总行数和总字节数，并写入到 counttrack 文件
echo "Total Lines: $TOTAL_LINES" | tee -a "$OUTPUT_FILE"
echo "Total Bytes: $TOTAL_BYTES" | tee -a "$OUTPUT_FILE"
