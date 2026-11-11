// 字体: 让游戏收下 0x100 以上的字 (汉字), 存在我们自己的表里。
// 我们的表挂在游戏字形表的第 0x10 格 (0x10 是控制字符, 本来就空着; 也是中文模组的转义字节)。
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "hook.h"
#include "patch.h"

void cjk_log(const char *fmt, ...);

#define FONT_GLYPHS   0xe8            // CBitmapFont 里 256 格字形表的位置
#define SIDE_SLOT     0x10
#define SIDE_GLYPHS   0x10000
typedef void *(*cstring_add_char_t)(void *self, char c);

typedef struct { int16_t x, y, w, h, xoff, yoff, xadv; uint8_t kerning, pad; } Glyph;  // 跟游戏的一样, 16 字节

uintptr_t cjk_parse_ret_small, cjk_parse_ret_loop;
void cjk_parse_stub(void);
static int glyph_count;

int date_install(void);

void cjk_add_glyph(uint8_t *font, int id, int x, int y, int w, int h, int xoff, int yoff, int xadv) {
    if (glyph_count == 0)     // 有中文字体才改日期格式 (没开中文模组时「年」「日」那两格是空的, 会变成 1444November11)
        cjk_log("日期格式: %d", date_install());
    void ***slot = (void ***)(font + FONT_GLYPHS + SIDE_SLOT * 8);
    void **side = *slot;
    if (!side) {
        // 一整块: 前面 65536 个指针, 后面 65536 个字形。游戏关字体时会 delete 这一格, 正好整块释放。
        // 再后面 65536 个是排版用的副本 (高度 0), 跟原字形正好隔 0x100000 字节 (stubs.S 里这么算)
        side = calloc(1, SIDE_GLYPHS * 8 + 2 * SIDE_GLYPHS * sizeof(Glyph));
        if (!side) return;
        *slot = side;
    }
    Glyph *g = (Glyph *)((uint8_t *)side + SIDE_GLYPHS * 8) + id;
    g->x = x; g->y = y; g->w = w; g->h = h; g->xoff = xoff; g->yoff = yoff; g->xadv = xadv;
    g->kerning = 0;
    Glyph *layout = (Glyph *)((uint8_t *)g + SIDE_GLYPHS * sizeof(Glyph));
    *layout = *g;
    layout->h = 0;
    side[id] = g;
    glyph_count++;
}

int font_glyph_count(void) { return glyph_count; }

int font_install(void) {
    uintptr_t site = GAME(0x10146cde7);
    const uint8_t want[] = {0x41, 0x81, 0xFE, 0xFF, 0x00, 0x00, 0x00, 0x0F, 0x87};
    if (!patch_expect(site, want, sizeof want)) return -1;
    cjk_parse_ret_small = GAME(0x10146cdf8);   // je (id == 0xa)
    cjk_parse_ret_loop  = GAME(0x10146cb8a);   // 读下一行
    return hook_jmp(site, 7, (void *)cjk_parse_stub);
}

// ---- 读图片文件的大小上限: 原来 16MB, 地图用的中文字体图片 (zh-hans-map.dds) 有 49MB 读不进来, 画成白块 ----
// CTextureHandler::LoadTexture 里两处 cmp ..., 0xffffff 改成 0x3ffffff (64MB, 跟 EU4dll 一样)
int texture_limit_install(void) {
    const uint8_t a[] = {0x81, 0xFB, 0xFF, 0xFF, 0xFF, 0x00};          // cmp ebx, 0xffffff
    const uint8_t b[] = {0x41, 0x81, 0xFC, 0xFF, 0xFF, 0xFF, 0x00};    // cmp r12d, 0xffffff
    uintptr_t pa = GAME(0x1015d9521), pb = GAME(0x1015d9717);
    if (!patch_expect(pa, a, sizeof a) || !patch_expect(pb, b, sizeof b)) return -1;
    if (patch_write(pa + 5, "\x03", 1) || patch_write(pb + 6, "\x03", 1)) return -2;
    return 0;
}

// ---- 右上角的日期: 原来 "d w mw w y" (11 11月 1444), 改成 1444年11月11日 (跟 EU4dll 一样) ----
// 0x0F、0x0E 是中文字体里的「年」「日」(中文模组的字体在这两个位置放了这两个字)
int date_install(void) {
    const char old_fmt[] = "d w mw w y";
    const char new_fmt[] = {'y', ' ', 0x0F, ' ', 'm', 'w', ' ', 'd', ' ', 0x0E, 0};
    uintptr_t a = GAME(0x101b74ef9);
    if (!patch_expect(a, old_fmt, sizeof old_fmt)) return -1;
    return patch_write(a, new_fmt, sizeof new_fmt);
}

// ---- CString 的读法 (跟 libc++ 的 string 一样: 第一个字节最低位 1 = 长字符串) ----
static uint8_t *cstr_data(uint8_t *s, size_t *n) {
    if (s[0] & 1) { *n = *(size_t *)(s + 8); return *(uint8_t **)(s + 0x10); }
    *n = s[0] >> 1; return s + 1;
}
static int is_esc(uint8_t c) { return c >= 0x10 && c <= 0x13; }

// ---- CString::ToUpper / ToLower 整个换掉: 汉字 (转义 + 两个字节) 跳过, 别的照原来的做法 ----
// 地图上的国名省名会先转大写, 原来把汉字的后两个字节也「转大写」了, 变成别的字 (EU4dll map_adjustment 也取消了这个)
int __toupper(int), __tolower(int);
static void cstr_case(uint8_t *s, int upper) {
    size_t n; uint8_t *p = cstr_data(s, &n);
    for (size_t i = 0; i < n && p[i]; ) {
        if (is_esc(p[i])) { i += 3; continue; }
        int c = (signed char)p[i];
        p[i] = (uint8_t)(upper ? __toupper(c) : __tolower(c));
        i++;
    }
}
static void my_cstring_toupper(uint8_t *s) { cstr_case(s, 1); }
static void my_cstring_tolower(uint8_t *s) { cstr_case(s, 0); }

// ---- CString::RemoveSpecialCharacters 整个换掉 ----
// 原来: 每个 0x80 以上的字节换成长得像的英文字母 (Õ→O 这种, 用游戏自己的对照表 nReplacement), 汉字的后两个字节常在这个范围,
// 存档名「法兰西1444_11_11」就变成「沃兰444_11_11」。现在汉字 (转义 + 两个字节) 跳过, 别的照原来的做法。
static void my_remove_special(uint8_t *s) {
    size_t n; uint8_t *p = cstr_data(s, &n);
    const uint8_t *table = (const uint8_t *)GAME(0x101bf1810);    // nReplacement, 下标是 字节 ^ 0x80
    for (size_t i = 0; i < n; ) {
        uint8_t c = p[i];
        if (is_esc(c) && i + 2 < n) { i += 3; continue; }
        if (c >= 0x80) {
            if (c == 0xD0) p[i] = 'D';
            else if (c == 0xDE) p[i] = 'T';
            else if (c == 0xF0) p[i] = 'd';
            else if (c == 0xFE) p[i] = 't';
            else p[i] = table[c ^ 0x80];
        }
        i++;
    }
}

// ---- 地图名字字和字之间插空格 (CGenerateNamesWork::AddNameArea 里): 一个汉字的三个字节当一个整体 ----
typedef void *(*cstring_add_cstr_t)(void *self, const void *other);
void cjk_spaced(void *dst, uint8_t *src, void *spaces) {
    cstring_add_char_t add = (cstring_add_char_t)GAME(0x1018659d2);   // CString::operator+=(char)
    cstring_add_cstr_t addstr = (cstring_add_cstr_t)GAME(0x101865952); // CString::operator+=(CString const&)
    size_t n; uint8_t *p = cstr_data(src, &n);
    for (size_t i = 0; i < n; ) {
        size_t unit = (is_esc(p[i]) && i + 2 < n) ? 3 : 1;
        for (size_t k = 0; k < unit; k++) add(dst, (char)p[i + k]);
        i += unit;
        if (i < n) addstr(dst, spaces);
    }
}
// ---- 文字框 (CTextBuffer, 提示框用它按行切) 算「一行放得下几个字节」: 落在汉字中间就退到这个汉字前面 ----
// 原来切出来会把汉字拆开, 提示框里出现「效蜊s」这种怪字 (EU4dll tooltip_and_button 管的就是这一块)
typedef int (*glcs_t)(void *self, uint8_t *cstr);
static glcs_t orig_glcs;
static int my_glcs(void *self, uint8_t *cstr) {
    int n = orig_glcs(self, cstr);
    size_t len; uint8_t *p = cstr_data(cstr, &len);
    if (n <= 0 || (size_t)n >= len) return n;
    size_t pos = 0;
    while (pos < (size_t)n) {
        size_t unit = (is_esc(p[pos]) && pos + 2 < len) ? 3 : 1;
        if (pos + unit > (size_t)n) break;
        pos += unit;
    }
    if (pos == 0) pos = (is_esc(p[0]) && len >= 3) ? 3 : 1;    // 一个字都放不下也至少放一个, 不然会一直切不动
    return (int)pos;
}

extern void cjk_ana_space(void);
extern uintptr_t cjk_ana_space_ret;

int textbuffer_install(void) {
    const uint8_t head[] = {0x55, 0x48, 0x89, 0xE5, 0x41, 0x57};   // push rbp; mov rbp,rsp; push r15
    uintptr_t a = GAME(0x101500a8e);                                // CTextBuffer::GetLineCharSize
    if (!patch_expect(a, head, 6)) return -1;
    orig_glcs = (glcs_t)hook_tramp(a, 6);
    return (orig_glcs && hook_jmp(a, 6, (void *)my_glcs) == 0) ? 0 : -2;
}

int remove_special_install(void) {
    const uint8_t head[] = {0x55, 0x48, 0x89, 0xE5, 0x8A, 0x0F};   // push rbp; mov rbp,rsp; mov cl,[rdi]
    uintptr_t a = GAME(0x1018671da);                                // CString::RemoveSpecialCharacters
    if (!patch_expect(a, head, 6)) return -1;
    return hook_jmp(a, 6, (void *)my_remove_special) ? -2 : 0;
}

int map_names_install(void) {
    // 转大写、转小写: 开头 push rbp; mov rbp,rsp; push r15 (6 字节)
    const uint8_t head[] = {0x55, 0x48, 0x89, 0xE5, 0x41, 0x57};
    uintptr_t up = GAME(0x101866f48), low = GAME(0x101866ee2);
    if (!patch_expect(up, head, 6) || !patch_expect(low, head, 6)) return -1;
    if (hook_jmp(up, 6, (void *)my_cstring_toupper) || hook_jmp(low, 6, (void *)my_cstring_tolower)) return -2;
    // 插空格: 0x100e0d4bd 是 lea rdi, [rbp-0x90] (7 字节), 一直到 0x100e0d566 整段换掉
    const uint8_t lea[] = {0x48, 0x8D, 0xBD, 0x70, 0xFF, 0xFF, 0xFF};
    uintptr_t sp = GAME(0x100e0d4bd);
    if (!patch_expect(sp, lea, 7)) return -3;
    cjk_ana_space_ret = GAME(0x100e0d566);
    return hook_jmp(sp, 7, (void *)cjk_ana_space) ? -4 : 0;
}

// ---- 读字的地方 (stubs.S 里的 SITE_A 等) ----

void cjk_append2(void *word, const uint8_t *p) {      // 单词 += 汉字的后两个字节
    cstring_add_char_t add = (cstring_add_char_t)GAME(0x1018659d2);   // CString::operator+=(char)
    add(word, (char)p[1]);
    add(word, (char)p[2]);
}

#define STUB(n) extern void cjk_##n(void); extern uintptr_t cjk_##n##_ret;
STUB(rtt1) STUB(rtt2) STUB(rts1) STUB(rts2) STUB(gh) STUB(gw)
STUB(fvb1) STUB(fvb2) STUB(gars) STUB(grs) STUB(garrsa)
STUB(ana) STUB(ann) STUB(curve) STUB(r3d1) STUB(r3d2)
extern uintptr_t cjk_ptext;

// kind: 0 = movzx eax,[rax] + 查表; 1 = 只有查表; 2 = mov rcx,r14 + 查表; 3 = movzx + mov rcx,[rbp-0x1128] + 查表;
//       4 = movzx + mov r15,[r12+8*rax] (CurveText); 5 = movzx + mov rcx,[rbp-0xf0] + 查表 (Render3d)
static const struct { const char *name; uintptr_t at; int len, kind; void (*stub)(void); uintptr_t *ret; uintptr_t back; } sites[] = {
    {"RenderToTexture 排版", 0x10146da2b, 11, 0, cjk_rtt1,   &cjk_rtt1_ret,   0x10146da36},
    {"RenderToTexture 画",   0x10146f8f3, 11, 0, cjk_rtt2,   &cjk_rtt2_ret,   0x10146f8fe},
    {"RenderToScreen 排版",  0x101470cb2, 11, 2, cjk_rts1,   &cjk_rts1_ret,   0x101470cbd},
    {"RenderToScreen 画",    0x101471e44,  8, 1, cjk_rts2,   &cjk_rts2_ret,   0x101471e4c},
    {"GetHeightOfString",    0x101472bf4, 11, 0, cjk_gh,     &cjk_gh_ret,     0x101472bff},
    {"GetWidthOfString",     0x101472f7e,  8, 1, cjk_gw,     &cjk_gw_ret,     0x101472f86},
    {"FillVertexBuffer 1",   0x1014745ae, 11, 0, cjk_fvb1,   &cjk_fvb1_ret,   0x1014745b9},
    {"FillVertexBuffer 2",   0x101474a2d, 11, 0, cjk_fvb2,   &cjk_fvb2_ret,   0x101474a38},
    {"GetActualRequiredSize",0x101475179, 18, 3, cjk_gars,   &cjk_gars_ret,   0x10147518b},
    {"GetRequiredSize",      0x101475957, 11, 0, cjk_grs,    &cjk_grs_ret,    0x101475962},
    {"GetActualRealRequiredSizeActually", 0x101476267, 11, 0, cjk_garrsa, &cjk_garrsa_ret, 0x101476272},
    {"地图名字 AddNameArea",   0x100e0d88e, 11, 0, cjk_ana,   &cjk_ana_ret,   0x100e0d899},
    {"地图名字 AddNudgedNames",0x100e0f438, 11, 0, cjk_ann,   &cjk_ann_ret,   0x100e0f443},
    {"地图名字 CurveText",     0x100e0e875,  7, 4, cjk_curve, &cjk_curve_ret, 0x100e0e87c},
    {"Render3d 排版",          0x1014736e5, 18, 5, cjk_r3d1,  &cjk_r3d1_ret,  0x1014736f7},
    {"Render3d 画",            0x101473d10, 11, 0, cjk_r3d2,  &cjk_r3d2_ret,  0x101473d1b},
};

static int lookup_ok(const uint8_t *p) {    // mov r64, [base + 8*idx + 0xe8]
    return (p[0] & 0xF0) == 0x40 && p[1] == 0x8B && p[4] == 0xE8 && !p[5] && !p[6] && !p[7];
}

// 循环结尾 (stubs.S 的 SKIP_AT_END): mov ebx, [rbp-0xb4] (8B 9D 4C FF FF FF) ; inc ebx (FF C3)
extern void cjk_grs_end(void), cjk_garrsa_end(void);
extern uintptr_t cjk_grs_end_ret, cjk_garrsa_end_ret;
static int loop_end_install(void) {
    const uint8_t want[] = {0x8B, 0x9D, 0x4C, 0xFF, 0xFF, 0xFF, 0xFF, 0xC3};
    uintptr_t a = GAME(0x101475bc7), b = GAME(0x10147671f);
    if (!patch_expect(a, want, 8) || !patch_expect(b, want, 8)) return -1;
    cjk_grs_end_ret = GAME(0x101475bcf);
    cjk_garrsa_end_ret = GAME(0x101476727);
    return (hook_jmp(a, 8, (void *)cjk_grs_end) || hook_jmp(b, 8, (void *)cjk_garrsa_end)) ? -2 : 0;
}

int sites_install(void) {
    cjk_ptext = GAME(0x102155c90);      // RenderToScreen 用的全局 pText
    int bad = 0;
    if (loop_end_install() != 0) { cjk_log("循环结尾没改上"); bad++; }
    for (size_t i = 0; i < sizeof sites / sizeof sites[0]; i++) {
        const uint8_t *p = (const uint8_t *)GAME(sites[i].at);
        const uint8_t movzx[] = {0x0F, 0xB6, 0x00}, movrcx[] = {0x4C, 0x89, 0xF1},
                      ldrcx[] = {0x48, 0x8B, 0x8D, 0xD8, 0xEE, 0xFF, 0xFF};
        int ok;
        switch (sites[i].kind) {
        case 0: ok = !memcmp(p, movzx, 3) && lookup_ok(p + 3); break;
        case 1: ok = lookup_ok(p); break;
        case 2: ok = !memcmp(p, movrcx, 3) && lookup_ok(p + 3); break;
        case 3: ok = !memcmp(p, movzx, 3) && !memcmp(p + 3, ldrcx, 7) && lookup_ok(p + 10); break;
        case 5: { const uint8_t ld[] = {0x48, 0x8B, 0x8D, 0x10, 0xFF, 0xFF, 0xFF};   // mov rcx, [rbp-0xf0]
                  ok = !memcmp(p, movzx, 3) && !memcmp(p + 3, ld, 7) && lookup_ok(p + 10); break; }
        default: { const uint8_t ld[] = {0x4D, 0x8B, 0x3C, 0xC4}; ok = !memcmp(p, movzx, 3) && !memcmp(p + 3, ld, 4); break; }
        }
        if (!ok) { cjk_log("对不上, 没改: %s", sites[i].name); bad++; continue; }
        *sites[i].ret = GAME(sites[i].back);
        if (hook_jmp((uintptr_t)p, sites[i].len, (void *)sites[i].stub) != 0) { cjk_log("改不了: %s", sites[i].name); bad++; }
    }
    return bad;
}
