// 插件入口: 游戏一启动就先跑这里。
#include <mach-o/dyld.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <crt_externs.h>
#include "hook.h"
#include "patch.h"

int shot_install(void);
int font_install(void);
int sites_install(void);
int texture_limit_install(void);
int map_names_install(void);
void spy_install(void);
int textbuffer_install(void);
int font_glyph_count(void);

static FILE *g_log;

void cjk_log(const char *fmt, ...) {
    if (!g_log) return;
    va_list ap; va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fputc('\n', g_log);
    fflush(g_log);
}

static int is_eu4(void) {
    char path[4096]; uint32_t n = sizeof path;
    if (_NSGetExecutablePath(path, &n) != 0) return 0;
    const char *base = strrchr(path, '/');
    return base && strcmp(base + 1, "eu4") == 0;
}

// 每一帧调一次 (shot.c), 每 10 秒写一次计数
void cjk_tick(void) {
    static time_t last;
    time_t t = time(NULL);
    if (t - last < 10) return;
    last = t;
    cjk_log("[%ld] 汉字字形 %d 个", (long)t, font_glyph_count());
}

static void on_exit_log(void) {
    time_t t = time(NULL);
    cjk_log("进程 %d 正常退出 %s", getpid(), ctime(&t));
}

__attribute__((constructor))
static void cjk_init(void) {
    if (!is_eu4()) return;               // 别的程序 (比如启动器) 加载了也什么都不做
    const char *home = getenv("HOME");
    char logpath[4096];
    const char *custom = getenv("EU4CJK_LOG");
    if (custom) snprintf(logpath, sizeof logpath, "%s", custom);
    else snprintf(logpath, sizeof logpath, "%s/Library/Logs/eu4-mac-cjk.log", home ? home : "/tmp");
    g_log = fopen(logpath, "a");
    time_t now = time(NULL);
    cjk_log("==== eu4-mac-cjk 加载了 %s进程 %d, 父进程 %d", ctime(&now), getpid(), getppid());
    char **argv = *_NSGetArgv();
    for (int i = 0; i < *_NSGetArgc(); i++) cjk_log("  参数 %d: %s", i, argv[i]);
    atexit(on_exit_log);

    g_slide = _dyld_get_image_vmaddr_slide(0);
    // 先确认是我们认识的那个版本 (1.37.5): 看一处已知的指令对不对得上
    const uint8_t sig[] = {0x41, 0x81, 0xFE, 0xFF, 0x00, 0x00, 0x00};
    if (!patch_expect(GAME(0x10146cde7), sig, sizeof sig)) {
        cjk_log("游戏版本对不上, 什么都不改");
        return;
    }
    if (near_init() != 0) { cjk_log("要不到跳板内存, 什么都不改"); return; }
    cjk_log("截图: %d", shot_install());
    cjk_log("字体: %d", font_install());
    cjk_log("读字的地方: %d 处没改", sites_install());
    cjk_log("图片大小上限: %d", texture_limit_install());
    cjk_log("地图名字 (大写、插空格): %d", map_names_install());
    cjk_log("文字框切行: %d", textbuffer_install());
    spy_install();
}
