#!/bin/sh
# 把插件从游戏里拿掉, 换回原版游戏程序。
G="${EU4_DIR:-$HOME/Library/Application Support/Steam/steamapps/common/Europa Universalis IV}"
APP="$G/eu4.app/Contents"
BACKUP="$APP/MacOS/eu4.原版备份"
if [ -f "$BACKUP" ]; then
    mv -f "$BACKUP" "$APP/MacOS/eu4" && echo "换回了原版游戏程序"
else
    echo "没有备份 (可能没装过, 或者 Steam 已经换回原版了)"
fi
rm -f "$APP/Frameworks/libeu4cjk.dylib" && echo "删掉了插件"
