#!/bin/bash

# 用法：./monitor_top.sh $(pidof screener) &

# 检查是否提供了PID参数
if [ -z "$1" ]; then
    echo "Usage: $0 <pid>"
    exit 1
fi

PID=$1

# 定义执行top命令的函数
run_top() {
    LOG_FILE="top_$(date +"%Y%m%d-%H%M").txt"
    echo -e "\n=== $(date) ===" >> "$LOG_FILE"
    top -p "$PID" -H -d 2 -b -n 3 >> "$LOG_FILE"
}

# 首次执行
run_top

# 循环循环
while true; do
    sleep 600  # 10分钟 = 600秒
    run_top
done

