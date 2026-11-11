#!/bin/sh
# 把插件从游戏里拿掉, 换回原版游戏程序。
cd "$(dirname "$0")/.." || exit 1
make -s build/eu4cjk-tool || exit 1
build/eu4cjk-tool uninstall
