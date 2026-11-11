#!/bin/sh
# 双击这个文件, 把中文插件装进 Steam 版《欧陆风云4》。
cd "$(dirname "$0")" || exit 1
xattr -dr com.apple.quarantine . 2>/dev/null     # 从网上下载的文件带「隔离」标记, 去掉免得被系统拦
./files/eu4cjk-tool install
echo
echo "按回车键关闭这个窗口"
read x
