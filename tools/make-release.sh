#!/bin/sh
# 打包发布用的 zip: tools/make-release.sh v1.0   → build/release/eu4-mac-cjk-v1.0.zip
cd "$(dirname "$0")/.." || exit 1
V=${1:?要写版本号, 比如 v1.0}
make -s clean && make -s build/libeu4cjk.dylib build/eu4cjk-tool || exit 1
D=build/release/eu4-mac-cjk-$V
rm -rf build/release && mkdir -p "$D/files"
cp build/libeu4cjk.dylib build/eu4cjk-tool "$D/files/"
cp release/安装.command release/卸载.command LICENSE "$D/"
sed "s/VERSION/$V/" release/使用说明.txt > "$D/使用说明.txt"
chmod 755 "$D"/*.command "$D/files/eu4cjk-tool"
ditto -c -k --keepParent "$D" "build/release/eu4-mac-cjk-$V.zip"
ls -la build/release
