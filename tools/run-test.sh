#!/bin/sh
# 带插件开游戏试一下 (用 build/userdir 当存档文件夹, 不动用户自己的)。
# 用法: tools/run-test.sh [秒数] [游戏参数...]   到时间自动关游戏 (默认 60 秒)
#       例: tools/run-test.sh 150 -start_tag=FRA   (直接开一局, 当法国)
#       模拟点鼠标: EU4CJK_CLICKS="70:642,740" tools/run-test.sh ...  (第 70 秒点窗口里的 (642,740); "75:w-5" = 第 75 秒滚轮往下 5 格)
cd "$(dirname "$0")/.." || exit 1
G="$HOME/Library/Application Support/Steam/steamapps/common/Europa Universalis IV"
SECS=${1:-60}
[ $# -gt 0 ] && shift
rm -f build/eu4cjk.log
[ -d build/userdir ] || tools/setup-userdir.sh
cd "$G" || exit 1
mkdir -p "$OLDPWD/build/shots"; rm -f "$OLDPWD"/build/shots/*.png
SteamAppId=236850 SteamGameId=236850 EU4CJK_SHOTS="$OLDPWD/build/shots" DYLD_INSERT_LIBRARIES="$OLDPWD/build/libeu4cjk.dylib" EU4CJK_LOG="$OLDPWD/build/eu4cjk.log" \
  ./eu4.app/Contents/MacOS/eu4 -userdir="$OLDPWD/build/userdir" "$@" > "$OLDPWD/build/stdout.txt" 2>&1 &
PID=$!
echo "游戏进程 $PID"
i=0; while [ $i -lt "$SECS" ] && kill -0 $PID 2>/dev/null; do sleep 1; i=$((i+1)); done
if kill -0 $PID 2>/dev/null; then kill $PID; sleep 2; kill -9 $PID 2>/dev/null; echo "到时间, 关掉了"; else wait $PID; echo "游戏自己退出了, 返回 $?"; fi
