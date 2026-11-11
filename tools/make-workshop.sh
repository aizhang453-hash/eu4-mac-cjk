#!/bin/sh
# 做创意工坊用的模组文件夹: tools/make-workshop.sh v1.0
# 放在「文稿/Paradox Interactive/Europa Universalis IV/mod/eu4_mac_cjk」, 然后在 Paradox 启动器里「上传 Mod」。
# 上传过以后启动器会在 descriptor.mod 里记下 remote_file_id, 再做新版时留着它 (更新同一个创意工坊物品)。
cd "$(dirname "$0")/.." || exit 1
V=${1:?要写版本号, 比如 v1.0}
tools/make-release.sh "$V" >/dev/null || exit 1
R="build/release/eu4-mac-cjk-$V"
M="$HOME/Documents/Paradox Interactive/Europa Universalis IV/mod"
D="$M/eu4_mac_cjk"
OLD_ID=$(grep -h '^remote_file_id=' "$D/descriptor.mod" 2>/dev/null | head -1)
rm -rf "$D" && mkdir -p "$D" || exit 1
cp -R "$R/" "$D/"
cp docs/workshop-thumbnail.png "$D/thumbnail.png"
{
  echo "version=\"${V#v}\""
  printf 'tags={\n\t"Translation"\n\t"Utilities"\n}\n'
  echo 'name="欧陆风云4 苹果版中文插件 (Mac Chinese Patch)"'
  echo 'picture="thumbnail.png"'
  echo 'supported_version="v1.37.5.*"'
  [ -n "$OLD_ID" ] && echo "$OLD_ID"
} > "$D/descriptor.mod"
{ cat "$D/descriptor.mod"; echo "path=\"$D\""; } > "$M/eu4_mac_cjk.mod"
echo "做好了: $D"
ls -la "$D" "$D/files"
