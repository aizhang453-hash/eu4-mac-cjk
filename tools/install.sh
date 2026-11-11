#!/bin/sh
# 把插件装进 Steam 版《欧陆风云4》: 以后从 Steam 开游戏就自动加载。
# 做的事: 1) 备份游戏程序 (eu4 → eu4.原版备份)  2) 插件放进 eu4.app/Contents/Frameworks
#         3) 在游戏程序的文件头里加一条「启动时加载插件」
# 还原: tools/uninstall.sh   (Steam 更新游戏或「验证文件完整性」以后要重新装一次)
cd "$(dirname "$0")/.." || exit 1
G="${EU4_DIR:-$HOME/Library/Application Support/Steam/steamapps/common/Europa Universalis IV}"
APP="$G/eu4.app/Contents"
BIN="$APP/MacOS/eu4"
BACKUP="$APP/MacOS/eu4.原版备份"
ORIG_SHA=bfb1f9dade119cb3adee4f595593df3983d9b7c3b480d3c2e2cb92e7576a7dbf   # 1.37.5.0 (Steam) 原版

[ -f "$BIN" ] || { echo "找不到游戏: $BIN"; exit 1; }
[ -f build/libeu4cjk.dylib ] || make || exit 1

sha=$(shasum -a 256 "$BIN" | cut -c1-64)
if [ "$sha" = "$ORIG_SHA" ]; then
    cp -p "$BIN" "$BACKUP" || exit 1
    echo "备份了原版游戏程序: $BACKUP"
elif [ -f "$BACKUP" ] && [ "$(shasum -a 256 "$BACKUP" | cut -c1-64)" = "$ORIG_SHA" ]; then
    echo "以前装过, 更新插件"
else
    echo "游戏程序不是认识的版本 (只支持 Steam 版 1.37.5.0), 不装。"
    exit 1
fi

cp build/libeu4cjk.dylib "$APP/Frameworks/libeu4cjk.dylib" || exit 1
python3 tools/add_dylib.py "$BIN" "@executable_path/../Frameworks/libeu4cjk.dylib" || exit 1
echo "装好了。从 Steam 开游戏, 在启动器里开上中文模组就行。日志在 ~/Library/Logs/eu4-mac-cjk.log"
