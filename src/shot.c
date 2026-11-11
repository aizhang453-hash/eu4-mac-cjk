// 调试用: 插件自己把游戏画面存成 PNG (不用系统的截屏权限)。
// 设了环境变量 EU4CJK_SHOTS=文件夹 才会存, 每 10 秒一张。
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "hook.h"

void cjk_log(const char *fmt, ...);
void cjk_tick(void);    // main.c: 定时写日志

static void (*orig_swap)(void *window);
static void (*sdl_drawable_size)(void *window, int *w, int *h);
static const char *shot_dir;
static int shot_n;
static double last_shot;

static double now_s(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void save_png(const char *path, int w, int h, uint8_t *rgba) {
    // OpenGL 的图是上下颠倒的, 先翻过来
    size_t row = (size_t)w * 4;
    uint8_t *tmp = malloc(row);
    for (int y = 0; y < h / 2; y++) {
        uint8_t *a = rgba + row * y, *b = rgba + row * (h - 1 - y);
        memcpy(tmp, a, row); memcpy(a, b, row); memcpy(b, tmp, row);
    }
    free(tmp);
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(rgba, w, h, 8, row, cs, kCGImageAlphaNoneSkipLast);
    CGImageRef img = CGBitmapContextCreateImage(ctx);
    CFStringRef s = CFStringCreateWithCString(NULL, path, kCFStringEncodingUTF8);
    CFURLRef url = CFURLCreateWithFileSystemPath(NULL, s, kCFURLPOSIXPathStyle, false);
    CGImageDestinationRef dst = CGImageDestinationCreateWithURL(url, CFSTR("public.png"), 1, NULL);
    if (dst) { CGImageDestinationAddImage(dst, img, NULL); CGImageDestinationFinalize(dst); CFRelease(dst); }
    CFRelease(url); CFRelease(s); CGImageRelease(img); CGContextRelease(ctx); CGColorSpaceRelease(cs);
}

// ---- 调试用: 模拟点鼠标 (EU4CJK_CLICKS="秒:x,y;秒:x,y", 秒数从插件加载算起, x,y 是窗口里的点) ----
typedef int (*send_motion_t)(void *window, uint32_t mouse, int relative, int x, int y);
typedef int (*send_button_t)(void *window, uint32_t mouse, uint8_t state, uint8_t button);
typedef int (*send_wheel_t)(void *window, uint32_t mouse, float x, float y, int direction);
static struct { double t; int x, y, wheel, done; } clicks[32];
static int nclicks;
static double t0;

static void parse_clicks(void) {
    const char *s = getenv("EU4CJK_CLICKS");
    while (s && *s && nclicks < 32) {
        double t; int x, y, n = 0;
        if (sscanf(s, "%lf:w%d%n", &t, &y, &n) == 2) {          // 滚轮
            clicks[nclicks].t = t; clicks[nclicks].wheel = 1; clicks[nclicks].y = y; nclicks++;
        } else if (sscanf(s, "%lf:%d,%d%n", &t, &x, &y, &n) == 3) {
            clicks[nclicks].t = t; clicks[nclicks].x = x; clicks[nclicks].y = y; nclicks++;
        } else break;
        s += n;
        if (*s == ';') s++;
    }
}

static void do_clicks(void *window, double t) {
    send_motion_t motion = (send_motion_t)GAME(0x1012dbb50);    // SDL_SendMouseMotion
    send_button_t button = (send_button_t)GAME(0x1012dc750);    // SDL_SendMouseButton
    for (int i = 0; i < nclicks; i++) {
        if (clicks[i].done || t - t0 < clicks[i].t) continue;
        clicks[i].done = 1;
        if (clicks[i].wheel) {
            ((send_wheel_t)GAME(0x1012dc760))(window, 0, 0.0f, (float)clicks[i].y, 0);   // SDL_SendMouseWheel
            cjk_log("滚轮 %d", clicks[i].y);
            continue;
        }
        motion(window, 0, 0, clicks[i].x, clicks[i].y);
        button(window, 0, 1, 1);    // 按下左键
        button(window, 0, 0, 1);    // 松开
        cjk_log("点了 (%d, %d)", clicks[i].x, clicks[i].y);
    }
}

static void my_swap(void *window) {
    cjk_tick();
    double t = now_s();
    do_clicks(window, t);
    if (shot_dir && t - last_shot >= 10.0) {
        last_shot = t;
        int w = 0, h = 0;
        sdl_drawable_size(window, &w, &h);
        if (w > 0 && h > 0 && w <= 8192 && h <= 8192) {
            GLint fb = 0, rb = 0, pack = 0;
            glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &fb);
            glGetIntegerv(GL_READ_BUFFER, &rb);
            glGetIntegerv(GL_PACK_ALIGNMENT, &pack);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            glReadBuffer(GL_BACK);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            uint8_t *buf = malloc((size_t)w * h * 4);
            glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, buf);
            glPixelStorei(GL_PACK_ALIGNMENT, pack);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, fb);
            glReadBuffer(rb);
            char path[4096];
            snprintf(path, sizeof path, "%s/shot-%03d.png", shot_dir, ++shot_n);
            save_png(path, w, h, buf);
            free(buf);
            cjk_log("截图 %s (%dx%d)", path, w, h);
        }
    }
    orig_swap(window);
}

int shot_install(void) {
    shot_dir = getenv("EU4CJK_SHOTS");
    t0 = now_s();
    parse_clicks();
    sdl_drawable_size = (void *)GAME(0x1012ca130);        // _SDL_GL_GetDrawableSize
    uintptr_t site = GAME(0x10135cbd0);                   // _SDL_GL_SwapWindow_REAL
    // 开头 6 字节: push rbp; mov rbp,rsp; push rbx; push rax
    orig_swap = hook_tramp(site, 6);
    if (!orig_swap) return -1;
    return hook_jmp(site, 6, (void *)my_swap);
}
