#!/bin/sh
# 双击这个文件, 把中文插件从《欧陆风云4》里拿掉, 换回原版。
cd "$(dirname "$0")" || exit 1
xattr -dr com.apple.quarantine . 2>/dev/null
./files/eu4cjk-tool uninstall
echo
echo "按回车键关闭这个窗口"
read x
