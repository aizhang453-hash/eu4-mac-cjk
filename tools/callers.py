# 在整个反汇编 (build/eu4.s) 里找含有某段文字的指令, 列出在哪个函数里。
# 用法: python3 tools/callers.py '0x1018cc926'
import re, sys, subprocess
pat = sys.argv[1]
fn = None
hits = {}
with open('build/eu4.s', errors='replace') as f:
    for line in f:
        if line.endswith('>:\n') and not line.startswith(' '):
            fn = line.split('<', 1)[1][:-3]
        elif pat in line:
            hits.setdefault(fn, []).append(line.split(':', 1)[0])
names = list(hits)
dem = subprocess.run(['c++filt'], input='\n'.join(names), capture_output=True, text=True).stdout.splitlines()
for n, d in zip(names, dem):
    print(len(hits[n]), d[:110], ' '.join(hits[n][:4]))
