// 文件名里的汉字。
// 游戏里的文字是中文模组的写法 (0x10~0x13 + 两个字节 = 一个汉字, 其他字节是 cp1252),
// 苹果电脑的文件名要标准的 UTF-8。所以:
//   游戏打开 / 建 / 查 / 改名 / 删文件时, 路径里的模组写法换成 UTF-8 (存档文件就叫「法兰西1444_11_11.eu4」);
//   游戏读文件夹时, 有汉字的文件名换回模组写法 (读档列表才显示得出来; 再拿这个名字开文件时又会换回 UTF-8)。
// 换法照 EU4dll 的 escape_tool.cpp。
#include <dirent.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void cjk_log(const char *fmt, ...);

// ---- cp1252 的 0x80~0x9F ----
static const unsigned short cp1252_hi[32] = {
    0x20AC, 0x81, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x8D, 0x017D, 0x8F,
    0x90, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x9D, 0x017E, 0x0178};

static unsigned cp1252_to_ucs(unsigned char c) { return (c >= 0x80 && c < 0xA0) ? cp1252_hi[c - 0x80] : c; }
static int ucs_to_cp1252(unsigned cp) {       // 能用一个字节表示就返回那个字节, 不能就 -1
    if (cp < 0x80 || (cp >= 0xA0 && cp < 0x100)) return (int)cp;
    for (int i = 0; i < 32; i++) if (cp1252_hi[i] == cp) return 0x80 + i;
    return -1;
}

static int put_utf8(char *out, unsigned cp) {
    if (cp < 0x80) { out[0] = (char)cp; return 1; }
    if (cp < 0x800) { out[0] = (char)(0xC0 | cp >> 6); out[1] = (char)(0x80 | (cp & 0x3F)); return 2; }
    out[0] = (char)(0xE0 | cp >> 12); out[1] = (char)(0x80 | ((cp >> 6) & 0x3F)); out[2] = (char)(0x80 | (cp & 0x3F));
    return 3;
}

// 从 s 开头读一个 UTF-8 字, 返回长度 (不是合法 UTF-8 就返回 0)
static int get_utf8(const unsigned char *s, unsigned *cp) {
    if (s[0] < 0x80) { *cp = s[0]; return 1; }
    if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        *cp = (s[0] & 0x1Fu) << 6 | (s[1] & 0x3F);
        return *cp >= 0x80 ? 2 : 0;
    }
    if ((s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        *cp = (s[0] & 0x0Fu) << 12 | (s[1] & 0x3Fu) << 6 | (s[2] & 0x3F);
        return *cp >= 0x800 ? 3 : 0;
    }
    if ((s[0] & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80) {
        *cp = (s[0] & 0x07u) << 18 | (s[1] & 0x3Fu) << 12 | (s[2] & 0x3Fu) << 6 | (s[3] & 0x3F);
        return *cp >= 0x10000 ? 4 : 0;
    }
    return 0;
}

// ---- 模组写法 → UTF-8 (给文件系统) ----
// 只在路径里有转义字节 (0x10~0x13) 或者不合法的 UTF-8 时才换; 本来就是 UTF-8 的部分 (比如用户名是中文的家目录) 原样留着。
static const char *to_fs(const char *path, char *buf, size_t size) {
    if (!path) return path;
    const unsigned char *s = (const unsigned char *)path;
    int need = 0;
    for (const unsigned char *p = s; *p; ) {
        unsigned cp;
        if (*p >= 0x10 && *p <= 0x13) { need = 1; break; }
        if (*p < 0x80) { p++; continue; }
        int n = get_utf8(p, &cp);
        if (!n) { need = 1; break; }
        p += n;
    }
    if (!need) return path;
    size_t o = 0;
    for (size_t i = 0; s[i] && o + 4 < size; ) {
        unsigned cp;
        if (s[i] >= 0x10 && s[i] <= 0x13 && s[i + 1] && s[i + 2]) {
            cp = s[i + 1] | (unsigned)s[i + 2] << 8;
            if (s[i] == 0x11) cp -= 0xE;
            else if (s[i] == 0x12) cp += 0x900;
            else if (s[i] == 0x13) cp += 0x8F2;
            cp &= 0xFFFF;
            if (cp > 0x100 && cp < 0x98F) cp = 0x2026;
            o += put_utf8(buf + o, cp);
            i += 3;
            continue;
        }
        if (s[i] < 0x80) { buf[o++] = (char)s[i++]; continue; }
        int n = get_utf8(s + i, &cp);
        if (n) { memcpy(buf + o, s + i, n); o += n; i += n; continue; }   // 本来就是 UTF-8
        o += put_utf8(buf + o, cp1252_to_ucs(s[i]));                        // 游戏的 cp1252 字节
        i++;
    }
    buf[o] = 0;
    return buf;
}

// ---- UTF-8 → 模组写法 (读文件夹时给游戏) ----
static int bad_byte(unsigned char b) {          // EU4dll: 这些字节不能直接放在转义后面
    static const unsigned char bad[] = {0xA4, 0xA3, 0xA7, 0x24, 0x5B, 0x00, 0x5C, 0x20, 0x0D, 0x0A, 0x22, 0x7B, 0x7D, 0x40,
                                        0x80, 0x7E, 0x2F, 0x5F, 0xBD, 0x3B, 0x5D, 0x3D, 0x23, 0x3F, 0x3A, 0x3C, 0x3E, 0x2A, 0x7C};
    return memchr(bad, b, sizeof bad) != NULL;
}

// 名字里有 0x100 以上的字 (汉字等) 才换, 换好写回 name (缓冲区 cap 字节), 返回新长度; 不换返回 -1
static int to_game(char *name, size_t len, size_t cap) {
    const unsigned char *s = (const unsigned char *)name;
    int has_wide = 0;
    for (size_t i = 0; i < len; ) {
        unsigned cp; int n = get_utf8(s + i, &cp);
        if (!n) return -1;                       // 不是 UTF-8, 不动
        if (cp >= 0x100 && ucs_to_cp1252(cp) < 0) has_wide = 1;
        i += n;
    }
    if (!has_wide) return -1;
    unsigned char out[1024 * 2];
    size_t o = 0;
    for (size_t i = 0; i < len && o + 3 < sizeof out; ) {
        unsigned cp; int n = get_utf8(s + i, &cp);
        i += n;
        int b = ucs_to_cp1252(cp);
        if (b >= 0) { out[o++] = (unsigned char)b; continue; }
        if (cp > 0xFFFF) cp = 0x2026;             // 太大的字 (表情符号之类) 放不下
        if (cp > 0x100 && cp < 0xA00) cp += 0xE000;
        unsigned char hi = cp >> 8, lo = cp & 0xFF, esc = 0x10;
        if (bad_byte(hi)) esc += 2;
        if (bad_byte(lo)) esc += 1;
        if (esc == 0x11 || esc == 0x13) lo += 14;
        if (esc == 0x12 || esc == 0x13) hi -= 9;
        out[o++] = esc; out[o++] = lo; out[o++] = hi;
    }
    if (o + 1 > cap) return -1;
    memcpy(name, out, o);
    name[o] = 0;
    return (int)o;
}

// ---- 包住游戏用到的文件操作 (dyld 的 interpose: 游戏和它带的库调这些函数时先到我们这里) ----
#define PATHBUF char buf_[4096]; const char *p_ = to_fs(path, buf_, sizeof buf_)

int stat_i64(const char *, struct stat *) __asm("_stat$INODE64");
int lstat_i64(const char *, struct stat *) __asm("_lstat$INODE64");
DIR *opendir_i64(const char *) __asm("_opendir$INODE64");
struct dirent *readdir_i64(DIR *) __asm("_readdir$INODE64");
FILE *fopen_ext(const char *, const char *) __asm("_fopen$DARWIN_EXTSN");

static int my_open(const char *path, int flags, ...) {
    int mode = 0;
    if (flags & O_CREAT) { va_list ap; va_start(ap, flags); mode = va_arg(ap, int); va_end(ap); }
    PATHBUF; return open(p_, flags, mode);
}
static FILE *my_fopen(const char *path, const char *m) { PATHBUF; return fopen(p_, m); }
static FILE *my_fopen_ext(const char *path, const char *m) { PATHBUF; return fopen_ext(p_, m); }
static int my_stat(const char *path, struct stat *st) { PATHBUF; return stat_i64(p_, st); }
static int my_lstat(const char *path, struct stat *st) { PATHBUF; return lstat_i64(p_, st); }
static int my_access(const char *path, int m) { PATHBUF; return access(p_, m); }
static int my_mkdir(const char *path, mode_t m) { PATHBUF; return mkdir(p_, m); }
static int my_remove(const char *path) { PATHBUF; return remove(p_); }
static DIR *my_opendir(const char *path) { PATHBUF; return opendir_i64(p_); }
static int my_rename(const char *from, const char *to) {
    char a[4096], b[4096];
    return rename(to_fs(from, a, sizeof a), to_fs(to, b, sizeof b));
}
static struct dirent *my_readdir(DIR *d) {
    struct dirent *e = readdir_i64(d);
    if (e) {
        int n = to_game(e->d_name, e->d_namlen, sizeof e->d_name);
        if (n >= 0) e->d_namlen = (uint16_t)n;
    }
    return e;
}

#define INTERPOSE(mine, theirs) \
    __attribute__((used)) static struct { const void *a, *b; } ip_##mine \
    __attribute__((section("__DATA,__interpose"))) = {(const void *)mine, (const void *)theirs};
INTERPOSE(my_open, open)
INTERPOSE(my_fopen, fopen)
INTERPOSE(my_fopen_ext, fopen_ext)
INTERPOSE(my_stat, stat_i64)
INTERPOSE(my_lstat, lstat_i64)
INTERPOSE(my_access, access)
INTERPOSE(my_mkdir, mkdir)
INTERPOSE(my_remove, remove)
INTERPOSE(my_opendir, opendir_i64)
INTERPOSE(my_rename, rename)
INTERPOSE(my_readdir, readdir_i64)
