# 在苹果程序 (Mach-O, 64 位) 的文件头里加一条「启动时加载这个 dylib」(LC_LOAD_WEAK_DYLIB)。
# 用 weak: dylib 不在也照样能开游戏。已经有了就什么都不做。
# 用法: python3 add_dylib.py 程序 @executable_path/../Frameworks/libeu4cjk.dylib
import os, struct, sys, tempfile

LC_LOAD_DYLIB, LC_LOAD_WEAK_DYLIB, LC_SEGMENT_64 = 0xc, 0x80000018, 0x19
path, lib = sys.argv[1], sys.argv[2].encode()
d = bytearray(open(path, 'rb').read())
magic, cpu, sub, ftype, ncmds, sizeofcmds = struct.unpack_from('<IiiIII', d, 0)
if magic != 0xfeedfacf:
    sys.exit('不是 64 位的苹果程序 (可能是好几种芯片合在一起的, 这个脚本不支持)')

off, first = 32, len(d)
for _ in range(ncmds):
    cmd, size = struct.unpack_from('<II', d, off)
    if cmd in (LC_LOAD_DYLIB, LC_LOAD_WEAK_DYLIB):
        nameoff = struct.unpack_from('<I', d, off + 8)[0]
        if bytes(d[off + nameoff:off + size]).rstrip(b'\0') == lib:
            print('已经加过了'); sys.exit(0)
    if cmd == LC_SEGMENT_64:
        for j in range(struct.unpack_from('<I', d, off + 64)[0]):
            s = off + 72 + j * 80
            soff = struct.unpack_from('<I', d, s + 48)[0]
            if soff: first = min(first, soff)
    off += size

name = lib + b'\0'
cmdsize = (24 + len(name) + 7) & ~7
if 32 + sizeofcmds + cmdsize > first:
    sys.exit('文件头里没有空位了')
cmd = struct.pack('<IIIIII', LC_LOAD_WEAK_DYLIB, cmdsize, 24, 2, 0x10000, 0x10000) + name
cmd += b'\0' * (cmdsize - len(cmd))
end = 32 + sizeofcmds
if any(d[end:end + cmdsize]):
    sys.exit('文件头后面不是空的, 不敢写')
d[end:end + cmdsize] = cmd
struct.pack_into('<II', d, 16, ncmds + 1, sizeofcmds + cmdsize)

mode = os.stat(path).st_mode
fd, tmp = tempfile.mkstemp(dir=os.path.dirname(path))
with os.fdopen(fd, 'wb') as f:
    f.write(d)
os.chmod(tmp, mode)
os.replace(tmp, path)
print('加好了')
