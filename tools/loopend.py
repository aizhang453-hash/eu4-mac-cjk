# 找包住某个地址的循环: 打印循环头, 以及跳回循环头那条指令前面几行 (看下标怎么加)。
# 用法: python3 tools/loopend.py 函数反汇编.s 地址
import re, sys
lines = open(sys.argv[1]).read().splitlines()
site = int(sys.argv[2], 16)
ins = []
for l in lines:
    m = re.match(r'^([0-9a-f]+):\s+(.*)$', l)
    if m: ins.append((int(m.group(1), 16), m.group(2)))
best = None
for i, (a, t) in enumerate(ins):
    m = re.search(r'\bj[a-z]+\s+0x([0-9a-f]+)', t)
    if not m: continue
    tgt = int(m.group(1), 16)
    if tgt <= site < a:                   # 往回跳, 跨过 site
        if best is None or tgt > best[1]: best = (i, tgt)
if not best: print("没找到循环"); sys.exit()
i, tgt = best
print("循环头 %x" % tgt)
for a, t in ins[max(0, i - 6): i + 1]:
    print("  %x  %s" % (a, re.sub(r'<__Z[^>]*\+', '<+', t)))
