#!/bin/sh
# 反汇编苹果版游戏里的一个函数 (按函数名找)。
# 用法: tools/dis.sh 'CBitmapFont::ParseFontFile()'     (名字要跟 build/syms.txt 里的一字不差)
#       tools/dis.sh -g 'CBitmapFont::'                  (只列出名字里有这几个字的函数)
BIN="$HOME/Library/Application Support/Steam/steamapps/common/Europa Universalis IV/eu4.app/Contents/MacOS/eu4"
cd "$(dirname "$0")/.." || exit 1
SYMS=build/syms.txt
if [ ! -s "$SYMS" ]; then
  mkdir -p build
  nm -n "$BIN" | c++filt > "$SYMS"
fi
if [ "$1" = "-g" ]; then grep -F -- "$2" "$SYMS"; exit 0; fi
range=$(awk -v name="$1" '
  { addr=$1; $1=""; $2=""; sub(/^  /, ""); }
  found && addr != start { print start, addr; exit }
  $0 == name { start=addr; found=1 }
' "$SYMS")
[ -z "$range" ] && { echo "找不到: $1" >&2; exit 1; }
set -- $range
objdump -d --no-show-raw-insn --x86-asm-syntax=intel --start-address=0x$1 --stop-address=0x$2 "$BIN" | tail -n +7
