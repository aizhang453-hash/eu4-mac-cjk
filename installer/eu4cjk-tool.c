// 安装工具: 把中文插件装进 Steam 版《欧陆风云4》(1.37.5.0), 或者拿掉。
// 不用 Python、不用开发工具, 别人的电脑上也能直接跑。
//
// 用法: eu4cjk-tool install [插件 dylib 的路径]   (不写路径就用跟这个工具放在一起的 libeu4cjk.dylib)
//       eu4cjk-tool uninstall
// 游戏文件夹: 先看环境变量 EU4_DIR, 再看 Steam 的默认位置和别的 Steam 库文件夹。
//
// 装的时候: 游戏程序备份成「eu4.原版备份」; 插件放进 eu4.app/Contents/Frameworks;
//           在游戏程序的文件头里加一条「启动时加载插件」(LC_LOAD_WEAK_DYLIB, 插件不在也照样能开游戏)。
//
// 跟「回主菜单不重开」插件 (eu4-mac-norestart) 一起用: 两个的安装工具守同一个规矩 ——
//   游戏程序 = 原版 + 下面 PLUGINS 里的几条 (按固定顺序), 备份共用「eu4.原版备份」。
//   装 = 在现有的几条上加自己那条; 卸 = 只去掉自己那条, 一条都不剩才换回原版。认不出来的样子不碰。
//   (这一段在两个项目里一样, 改的时候两边一起改)
#include <CommonCrypto/CommonDigest.h>
#include <errno.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <unistd.h>

#define ORIG_SHA  "bfb1f9dade119cb3adee4f595593df3983d9b7c3b480d3c2e2cb92e7576a7dbf"   // Steam 版 1.37.5.0 原版
#define GAME_SUB  "/steamapps/common/Europa Universalis IV"
static const char *PLUGINS[] = {"libeu4cjk.dylib", "libeu4norestart.dylib"};   // 顺序固定, 别改
#define NPLUG 2
#define SELF  0                                 // 这个工具管的是 PLUGINS[SELF]
#define TOOL  "eu4cjk-tool"

static int read_file(const char *path, uint8_t **buf, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    *buf = malloc(n > 0 ? (size_t)n : 1);
    if (!*buf || fread(*buf, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(*buf); return -1; }
    fclose(f);
    *len = (size_t)n;
    return 0;
}

// 先写到同一个文件夹里的临时文件, 写好再换上去 (写到一半出错不会把游戏弄坏)
static int write_file(const char *path, const uint8_t *buf, size_t len, mode_t mode) {
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof tmp, "%s." TOOL "-tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f) return -1;
    if (fwrite(buf, 1, len, f) != len) { fclose(f); unlink(tmp); return -1; }
    if (fclose(f) != 0) { unlink(tmp); return -1; }
    chmod(tmp, mode);
    if (rename(tmp, path) != 0) { unlink(tmp); return -1; }
    return 0;
}

static void sha256_hex(const uint8_t *buf, size_t len, char out[65]) {
    unsigned char d[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(buf, (CC_LONG)len, d);
    for (int i = 0; i < CC_SHA256_DIGEST_LENGTH; i++) sprintf(out + i * 2, "%02x", d[i]);
}

static int exists(const char *path) { struct stat st; return stat(path, &st) == 0; }

// 找游戏: EU4_DIR → 默认 Steam 库 → libraryfolders.vdf 里列的别的库
static int find_game(char *out, size_t cap) {
    const char *env = getenv("EU4_DIR");
    char probe[PATH_MAX];
    if (env && *env) {
        snprintf(out, cap, "%s", env);
        snprintf(probe, sizeof probe, "%s/eu4.app/Contents/MacOS/eu4", out);
        return exists(probe) ? 0 : -1;
    }
    const char *home = getenv("HOME");
    if (!home) return -1;
    char steam[PATH_MAX];
    snprintf(steam, sizeof steam, "%s/Library/Application Support/Steam", home);
    snprintf(out, cap, "%s" GAME_SUB, steam);
    snprintf(probe, sizeof probe, "%s/eu4.app/Contents/MacOS/eu4", out);
    if (exists(probe)) return 0;
    char vdf[PATH_MAX];
    snprintf(vdf, sizeof vdf, "%s/steamapps/libraryfolders.vdf", steam);
    FILE *f = fopen(vdf, "r");
    if (!f) return -1;
    char line[PATH_MAX + 64];
    while (fgets(line, sizeof line, f)) {
        char *k = strstr(line, "\"path\"");
        if (!k) continue;
        char *a = strchr(k + 6, '"'), *b = a ? strchr(a + 1, '"') : NULL;
        if (!a || !b) continue;
        *b = 0;
        snprintf(out, cap, "%s" GAME_SUB, a + 1);
        snprintf(probe, sizeof probe, "%s/eu4.app/Contents/MacOS/eu4", out);
        if (exists(probe)) { fclose(f); return 0; }
    }
    fclose(f);
    return -1;
}

// 在程序文件头加一条 LC_LOAD_WEAK_DYLIB (加载 path)。返回新的内容 (要 free); 出错返回 NULL
static uint8_t *add_load_cmd(const uint8_t *in, size_t len, const char *path, const char **err) {
    if (len < sizeof(struct mach_header_64)) { *err = "文件太小"; return NULL; }
    const struct mach_header_64 *h = (const void *)in;
    if (h->magic != MH_MAGIC_64) { *err = "不是 64 位的苹果程序"; return NULL; }
    size_t off = sizeof *h, first = len;
    for (uint32_t i = 0; i < h->ncmds; i++) {
        const struct load_command *lc = (const void *)(in + off);
        if (lc->cmd == LC_LOAD_DYLIB || lc->cmd == LC_LOAD_WEAK_DYLIB) {
            const struct dylib_command *dc = (const void *)lc;
            if (!strcmp((const char *)lc + dc->dylib.name.offset, path)) { *err = "已经加过了"; return NULL; }
        }
        if (lc->cmd == LC_SEGMENT_64) {
            const struct segment_command_64 *sc = (const void *)lc;
            const struct section_64 *s = (const void *)(sc + 1);
            for (uint32_t j = 0; j < sc->nsects; j++)
                if (s[j].offset && s[j].offset < first) first = s[j].offset;
        }
        off += lc->cmdsize;
    }
    size_t namelen = strlen(path) + 1;
    uint32_t cmdsize = (uint32_t)((sizeof(struct dylib_command) + namelen + 7) & ~7u);
    size_t end = sizeof *h + h->sizeofcmds;
    if (end + cmdsize > first) { *err = "文件头里没有空位"; return NULL; }
    for (size_t i = end; i < end + cmdsize; i++)
        if (in[i]) { *err = "文件头后面不是空的"; return NULL; }
    uint8_t *outb = malloc(len);
    if (!outb) { *err = "内存不够"; return NULL; }
    memcpy(outb, in, len);
    struct dylib_command dc = {LC_LOAD_WEAK_DYLIB, cmdsize, {{sizeof dc}, 2, 0x10000, 0x10000}};
    memcpy(outb + end, &dc, sizeof dc);
    memcpy(outb + end + sizeof dc, path, namelen);
    struct mach_header_64 *oh = (void *)outb;
    oh->ncmds += 1;
    oh->sizeofcmds += cmdsize;
    return outb;
}

// 原版 + mask 里的几条 (第 i 位 = PLUGINS[i]), 按固定顺序加。返回新的内容 (要 free)
static uint8_t *build(const uint8_t *orig, size_t len, unsigned mask, const char **err) {
    uint8_t *cur = malloc(len);
    if (!cur) { *err = "内存不够"; return NULL; }
    memcpy(cur, orig, len);
    for (int i = 0; i < NPLUG; i++) {
        if (!(mask & (1u << i))) continue;
        char path[256];
        snprintf(path, sizeof path, "@executable_path/../Frameworks/%s", PLUGINS[i]);
        uint8_t *next = add_load_cmd(cur, len, path, err);
        free(cur);
        if (!next) return NULL;
        cur = next;
    }
    return cur;
}

// 现在的游戏程序是「原版 + 哪几条」; 认不出来返回 -1
static int which(const uint8_t *orig, size_t olen, const uint8_t *cur, size_t clen) {
    if (clen != olen) return -1;
    for (unsigned mask = 0; mask < (1u << NPLUG); mask++) {
        const char *err = NULL;
        uint8_t *b = build(orig, olen, mask, &err);
        int same = b && !memcmp(b, cur, clen);
        free(b);
        if (same) return (int)mask;
    }
    return -1;
}

static int copy_file(const char *src, const char *dst) {
    uint8_t *buf; size_t len;
    if (read_file(src, &buf, &len) != 0) return -1;
    int r = write_file(dst, buf, len, 0755);
    free(buf);
    removexattr(dst, "com.apple.quarantine", 0);     // 从网上下载的会带「隔离」标记, 去掉免得被系统拦
    return r;
}

static void paths(const char *game, char *bin, char *backup, char *lib, size_t cap) {
    snprintf(bin, cap, "%s/eu4.app/Contents/MacOS/eu4", game);
    snprintf(backup, cap, "%s/eu4.app/Contents/MacOS/eu4.原版备份", game);
    snprintf(lib, cap, "%s/eu4.app/Contents/Frameworks/%s", game, PLUGINS[SELF]);
}

static int do_install(const char *game, const char *dylib) {
    char bin[PATH_MAX], backup[PATH_MAX], lib[PATH_MAX];
    paths(game, bin, backup, lib, PATH_MAX);
    if (!exists(dylib)) { printf("找不到插件文件: %s\n", dylib); return 1; }

    uint8_t *cur, *orig = NULL; size_t cur_len, orig_len = 0;
    if (read_file(bin, &cur, &cur_len) != 0) { printf("读不了游戏程序: %s\n", bin); return 1; }
    char sha[65]; sha256_hex(cur, cur_len, sha);
    int mask;
    if (!strcmp(sha, ORIG_SHA)) {                       // 原版: 先备份
        orig = cur; orig_len = cur_len; mask = 0;
        if (write_file(backup, orig, orig_len, 0755) != 0) { printf("备份游戏程序失败 (%s)\n", strerror(errno)); free(orig); return 1; }
        printf("备份了原版游戏程序 (eu4.原版备份)\n");
    } else {                                            // 以前装过插件: 应该是「原版 + 几条」
        char bsha[65] = "";
        if (read_file(backup, &orig, &orig_len) == 0) sha256_hex(orig, orig_len, bsha);
        mask = (orig && !strcmp(bsha, ORIG_SHA)) ? which(orig, orig_len, cur, cur_len) : -1;
        free(cur);
        if (mask < 0) {
            free(orig);
            printf("游戏程序不是认识的版本, 没有装。\n"
                   "这个插件只支持 Steam 版《欧陆风云4》1.37.5.0。如果游戏刚更新过, 要等插件出新版。\n");
            return 1;
        }
        printf(mask & (1u << SELF) ? "以前装过, 更新插件\n" : "游戏里已经装着别的插件, 一起留着\n");
    }

    if (copy_file(dylib, lib) != 0) { free(orig); printf("放插件失败 (%s)\n", strerror(errno)); return 1; }
    unsigned want = (unsigned)mask | (1u << SELF);
    if (want != (unsigned)mask) {
        const char *err = NULL;
        uint8_t *patched = build(orig, orig_len, want, &err);
        if (!patched) { free(orig); printf("改游戏程序失败: %s\n", err); return 1; }
        int r = write_file(bin, patched, orig_len, 0755);
        free(patched);
        if (r != 0) { free(orig); printf("写游戏程序失败 (%s)\n", strerror(errno)); return 1; }
    }
    free(orig);
    printf("装好了。\n");
    return 0;
}

static int do_uninstall(const char *game) {
    char bin[PATH_MAX], backup[PATH_MAX], lib[PATH_MAX];
    paths(game, bin, backup, lib, PATH_MAX);
    int did = 0;
    if (exists(backup)) {
        uint8_t *orig = NULL, *cur = NULL; size_t on = 0, cn = 0; char bsha[65] = "";
        if (read_file(backup, &orig, &on) == 0) sha256_hex(orig, on, bsha);
        int mask = -1;
        if (orig && !strcmp(bsha, ORIG_SHA) && read_file(bin, &cur, &cn) == 0) mask = which(orig, on, cur, cn);
        free(cur);
        if (mask < 0) {
            unlink(backup);                       // 游戏已经被 Steam 换成别的了, 备份没用了
            printf("游戏程序已经不是装插件时的那个了 (可能 Steam 更新过), 删掉了旧的备份\n");
            did = 1;
        } else if (mask & (1u << SELF)) {
            unsigned left = (unsigned)mask & ~(1u << SELF);
            if (left == 0) {
                if (rename(backup, bin) != 0) { free(orig); printf("换回原版失败 (%s)\n", strerror(errno)); return 1; }
                printf("换回了原版游戏程序\n");
            } else {                              // 别的插件还在: 只去掉自己那条, 备份留着
                const char *err = NULL;
                uint8_t *b = build(orig, on, left, &err);
                if (!b || write_file(bin, b, on, 0755) != 0) { free(b); free(orig); printf("改游戏程序失败\n"); return 1; }
                free(b);
                printf("去掉了这个插件; 游戏里别的插件还留着\n");
            }
            did = 1;
        }
        free(orig);
    }
    if (exists(lib)) { unlink(lib); printf("删掉了插件文件\n"); did = 1; }
    if (!did) printf("没装过这个插件, 不用卸载\n");
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2 || (strcmp(argv[1], "install") && strcmp(argv[1], "uninstall"))) {
        printf("用法: %s install [插件路径] | uninstall\n", argv[0]);
        return 2;
    }
    char game[PATH_MAX];
    if (find_game(game, sizeof game) != 0) {
        printf("找不到 Steam 版《欧陆风云4》。\n");
        return 1;
    }
    printf("游戏在: %s\n", game);
    if (!strcmp(argv[1], "uninstall")) return do_uninstall(game);

    char dylib[PATH_MAX];
    if (argc >= 3) snprintf(dylib, sizeof dylib, "%s", argv[2]);
    else {                                         // 跟这个工具放在一起的那份
        char self[PATH_MAX]; uint32_t n = sizeof self;
        if (_NSGetExecutablePath(self, &n) != 0) return 1;
        char *slash = strrchr(self, '/');
        if (slash) *slash = 0;
        snprintf(dylib, sizeof dylib, "%s/%s", self, PLUGINS[SELF]);
    }
    return do_install(game, dylib);
}
