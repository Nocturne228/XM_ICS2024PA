#!/bin/bash

executable="~/Downloads/ics2024/nemu/tools/gen-expr/build/gen-expr"
output_file="./output.txt"
iterations=20

: > "$output_file"

for i in $(seq 1 "$iterations"); do
	echo "Running iteration $i..." | tee -a "$output_file"
	$executable >> "$output_file" 2>&1
done

cat $output_file
