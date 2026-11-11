#!/bin/sh
# 把 build/ 里编好的插件装进 Steam 版《欧陆风云4》(开发用; 别人用发布页里的「安装.command」)。
# 还原: tools/uninstall.sh   (Steam 更新游戏或「验证文件完整性」以后要重新装一次)
cd "$(dirname "$0")/.." || exit 1
make -s build/libeu4cjk.dylib build/eu4cjk-tool || exit 1
build/eu4cjk-tool install build/libeu4cjk.dylib
