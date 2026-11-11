// 打字: 输入法打的中文进到输入框; 退格、删除、左右移动把一个汉字当一个字。
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "hook.h"
#include "patch.h"

void cjk_log(const char *fmt, ...);
size_t cjk_utf8_to_game(const char *in, unsigned char *out, size_t cap);

// ---- 收到输入法 (SDL_TEXTINPUT) 的一串字 ----
// 原来 (CSdlEvents::HandlePdxEvents): 只取第一个字, 而且只认 ASCII 和两字节的 UTF-8, 汉字直接扔掉。
// 现在: 整串换成游戏的写法 (汉字 = 转义 + 两个字节), 每个字节都当一次打字交给游戏 (EU4dll input.cpp 也是这样)。
// 输入框的禁止字符 (" § ¤ £ @ { } / \ 等) 中文模组的写法本来就避开了。
uintptr_t cjk_textin_ret;
void cjk_textin_stub(void);

void cjk_textin(const char *text, void *keyboard, int arg, void *handler) {
    void (*notify)(void *, int, int, int) = (void (*)(void *, int, int, int))(*(void ***)keyboard)[0x28 / 8];
    notify(keyboard, 0x303, arg, 0);                       // 原来就有的: 告诉键盘有文字输入
    unsigned char bytes[128];
    size_t n = cjk_utf8_to_game(text, bytes, sizeof bytes);
    if (getenv("EU4CJK_SPY")) cjk_log("输入法文字 %zu 字节", n);
    void (*make_text)(char *, char) = (void (*)(char *, char))GAME(0x1013fed70);          // CTextInputEvent::CTextInputEvent(char)
    void (*make_input)(void *, const char *) = (void (*)(void *, const char *))GAME(0x1013fdc74); // CInputEvent::CInputEvent(CTextInputEvent const&)
    void (*free_input)(void *) = (void (*)(void *))GAME(0x1013fdd9e);                    // CInputEvent::~CInputEvent()
    void (*dispatch)(void *, void *) = (void (*)(void *, void *))(*(void ***)handler)[0x20 / 8];
    for (size_t i = 0; i < n; i++) {
        if (!bytes[i]) continue;
        char tev;
        _Alignas(16) uint8_t ie[256];
        make_text(&tev, (char)bytes[i]);
        make_input(ie, &tev);
        dispatch(handler, ie);
        free_input(ie);
    }
}

// ---- 文字缓冲区 (CTextBuffer): 退格、删除、左右移动 ----
// 光标是按字节算的, 原来一次只动一个字节, 删汉字会留下半个字 (怪字)。现在碰到汉字一次做三遍。
typedef void (*tb_fn)(void *tb);
static tb_fn orig_backspace, orig_delete, orig_left, orig_right;

static uint8_t *tb_text(void *tb, size_t *n) {             // 文字在 +0x30 (CTextBuffer::GetString)
    uint8_t *s = (uint8_t *)tb + 0x30;
    if (s[0] & 1) { *n = *(size_t *)(s + 8); return *(uint8_t **)(s + 0x10); }
    *n = s[0] >> 1; return s + 1;
}
static int tb_pos(void *tb) { return ((int (*)(void *))GAME(0x1014fffa6))(tb); }   // GetCursorPositionInString()
static int tb_has_selection(void *tb) { return ((char (*)(void *))(*(void ***)tb)[0x170 / 8])(tb); }
static int is_esc(uint8_t c) { return c >= 0x10 && c <= 0x13; }

// 从头一个字一个字地数 (汉字的后两个字节也可能正好是 0x10~0x13, 所以不能往回看)
static void unit_around(const uint8_t *p, size_t n, size_t pos, int *before, int *after) {
    size_t i = 0;
    *before = pos > 0 ? 1 : 0; *after = pos < n ? 1 : 0;
    while (i < n) {
        size_t u = (is_esc(p[i]) && i + 2 < n) ? 3 : 1;
        if (i + u >= pos && i < pos) *before = (int)(pos - i);          // 光标前面那个字
        if (i == pos) { *after = (int)u; break; }                        // 光标后面那个字
        if (i > pos) { *after = (int)(i - pos); break; }                 // 光标落在汉字中间
        i += u;
    }
}

// 游戏的退格、删除里面自己会调左移、右移 (每删一个字节光标退一格), 那时不能再按「一个汉字」来跳
static int inside_delete;
static void repeat(tb_fn f, void *tb, int times) { for (int i = 0; i < times; i++) f(tb); }

static void dbg(const char *what, void *tb, int b, int a) {
    if (!getenv("EU4CJK_SPY")) return;
    size_t n; uint8_t *p = tb_text(tb, &n);
    char hex[300] = {0}; size_t m = n < 60 ? n : 60;
    for (size_t i = 0; i < m; i++) sprintf(hex + i * 3, "%02x ", p[i]);
    cjk_log("%s: 光标 %d 行 %d 列 %d, 长 %zu, 前 %d 后 %d [%s]", what, tb_pos(tb), *(uint16_t *)((uint8_t *)tb + 0x4e),
            *(uint16_t *)((uint8_t *)tb + 0x4c), n, b, a, hex);
}

static void my_backspace(void *tb) {
    if (tb_has_selection(tb)) { orig_backspace(tb); return; }
    size_t n; uint8_t *p = tb_text(tb, &n); int b, a;
    unit_around(p, n, (size_t)tb_pos(tb), &b, &a);
    dbg("退格", tb, b, a);
    inside_delete++;
    repeat(orig_backspace, tb, b ? b : 1);
    inside_delete--;
}
static void my_delete(void *tb) {
    if (tb_has_selection(tb)) { orig_delete(tb); return; }
    size_t n; uint8_t *p = tb_text(tb, &n); int b, a;
    unit_around(p, n, (size_t)tb_pos(tb), &b, &a);
    inside_delete++;
    repeat(orig_delete, tb, a ? a : 1);
    inside_delete--;
}
static void my_left(void *tb) {
    if (inside_delete) { orig_left(tb); return; }
    size_t n; uint8_t *p = tb_text(tb, &n); int b, a;
    unit_around(p, n, (size_t)tb_pos(tb), &b, &a);
    dbg("左", tb, b, a);
    repeat(orig_left, tb, b ? b : 1);
}
static void my_right(void *tb) {
    if (inside_delete) { orig_right(tb); return; }
    size_t n; uint8_t *p = tb_text(tb, &n); int b, a;
    unit_around(p, n, (size_t)tb_pos(tb), &b, &a);
    repeat(orig_right, tb, a ? a : 1);
}

int input_install(void) {
    // 收输入法文字: 0x101407de1 是 mov bl,[rbp-0x5c] (3) ; test bl,bl (2)
    const uint8_t want[] = {0x8A, 0x5D, 0xA4, 0x84, 0xDB};
    uintptr_t a = GAME(0x101407de1);
    if (!patch_expect(a, want, 5)) return -1;
    cjk_textin_ret = GAME(0x101408128);                     // 处理下一个事件
    if (hook_jmp(a, 5, (void *)cjk_textin_stub)) return -2;
    // 退格、删除、左移、右移: 开头都是 push rbp; mov rbp,rsp; push r14 (6 字节)
    const uint8_t head[] = {0x55, 0x48, 0x89, 0xE5, 0x41, 0x56};
    struct { uintptr_t at; tb_fn *orig; void *mine; } h[] = {
        {0x1015022ce, &orig_backspace, (void *)my_backspace},
        {0x101502624, &orig_delete, (void *)my_delete},
        {0x10150193a, &orig_left, (void *)my_left},
        {0x1015019e2, &orig_right, (void *)my_right},
    };
    for (int i = 0; i < 4; i++) {
        uintptr_t at = GAME(h[i].at);
        if (!patch_expect(at, head, 6)) return -3 - i;
        *h[i].orig = (tb_fn)hook_tramp(at, 6);
        if (!*h[i].orig || hook_jmp(at, 6, h[i].mine)) return -10 - i;
    }
    return 0;
}
