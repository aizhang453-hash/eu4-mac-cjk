#!/bin/sh
# 做测试用的存档文件夹 build/userdir (不动用户自己 Documents 里的东西):
# 照抄用户的 settings.txt, 但声音全关、1280x800 窗口; 只开中文模组 + 中文补充包。
cd "$(dirname "$0")/.." || exit 1
U="$HOME/Documents/Paradox Interactive/Europa Universalis IV"
W="$HOME/Library/Application Support/Steam/steamapps/workshop/content/236850"
T=build/userdir
mkdir -p "$T/mod"
sed -E -e 's/fullScreen=yes/fullScreen=no/' \
    -e '/^	size=\{/,/\}/{s/x=[0-9]+/x=1280/;s/y=[0-9]+/y=800/;}' \
    -e 's/^(sound_fx_volume|music_volume|master_volume|ambient_volume|dev_master_volume)=.*/\1=0.000000/' \
    "$U/settings.txt" > "$T/settings.txt"
MODS=""
for id in 2976470733 1999055990; do      # 中文模组, 中文补充包
  { grep -v '^path=' "$W/$id/descriptor.mod"; echo; echo "path=\"$W/$id\""; } > "$T/mod/ugc_$id.mod"
  MODS="$MODS\"mod/ugc_$id.mod\","
done
echo "{\"enabled_mods\":[${MODS%,}],\"disabled_dlcs\":[]}" > "$T/dlc_load.json"
cat "$T/dlc_load.json"
