// 调试用的探针 (stubs.S 的 SPY): 在游戏代码某处把寄存器和一些数据记进日志。
// 只在设了环境变量 EU4CJK_SPY=1 时装上。
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hook.h"

void cjk_log(const char *fmt, ...);
enum { R15, R14, R13, R12, R11, R10, R9, R8, RDI, RSI, RBP, RBX, RDX, RCX, RAX };

static void cstr_hex(const uint8_t *s, char *out) {     // CString 的内容 (十六进制)
    size_t n; const uint8_t *p;
    if (s[0] & 1) { n = *(size_t *)(s + 8); p = *(uint8_t **)(s + 0x10); } else { n = s[0] >> 1; p = s + 1; }
    size_t m = n < 24 ? n : 24;
    out += sprintf(out, "%zu:", n);
    for (size_t i = 0; i < m; i++) out += sprintf(out, " %02x", p[i]);
}

// 文字里有没有「转义字节后面紧跟换行」(汉字被换行切开了)
static int has_split(const uint8_t *s, char *out) {
    size_t n; const uint8_t *p;
    if (s[0] & 1) { n = *(size_t *)(s + 8); p = *(uint8_t **)(s + 0x10); } else { n = s[0] >> 1; p = s + 1; }
    for (size_t i = 0; i + 1 < n; i++) {
        if (p[i] >= 0x10 && p[i] <= 0x13 && (p[i + 1] == 0x0A || (i + 2 < n && p[i + 2] == 0x0A))) {
            size_t a = i >= 6 ? i - 6 : 0, b = i + 8 < n ? i + 8 : n;
            out += sprintf(out, "第 %zu 字节:", i);
            for (size_t k = a; k < b; k++) out += sprintf(out, " %02x", p[k]);
            return 1;
        }
    }
    return 0;
}

static int counts[8];
void cjk_spy(int id, uint64_t *r) {
    if (id >= 2) {          // 函数入口: 看收到的文字有没有被切开的汉字
        static const char *names[] = {"", "", "RenderToScreen", "RenderToTexture", "GetActualRealRequiredSizeActually", "GetRequiredSize"};
        const uint8_t *str = (const uint8_t *)(id == 3 ? r[R9] : r[RSI]);
        char buf[200];
        if (counts[id] < 5 && has_split(str, buf)) {
            counts[id]++;
            // 谁调的: 函数入口时栈顶是返回地址; 再往上顺着 rbp 链找
            uint64_t *entry_rsp = (uint64_t *)((uint8_t *)r + 16 * 8 + 128);
            uint64_t *fp = (uint64_t *)r[RBP];
            cjk_log("探针%d %s 收到的文字已经切开了: %s | 调用的地方 %llx %llx %llx %llx", id, names[id], buf,
                    (unsigned long long)entry_rsp[0], (unsigned long long)fp[1],
                    (unsigned long long)((uint64_t *)fp[0])[1], (unsigned long long)((uint64_t *)((uint64_t *)fp[0])[0])[1]);
        }
        return;
    }
    if (counts[id]++ >= 15) return;      // 每个探针只记前 15 次
    char hex[200];
    uint8_t *rbp = (uint8_t *)r[RBP];
    switch (id) {
    case 0:   // AddNameArea: FillVertexBuffer 刚返回
        cstr_hex(rbp - 0x90, hex);
        cjk_log("探针0 AddNameArea 名字 [%s] 看得见的字 %d, FillVertexBuffer 返回 %d", hex, (int)r[R13], (int)r[RAX]);
        break;
    case 1:   // AddNameArea: CurveText 刚返回
        cjk_log("探针1 CurveText 返回 %d", (int)r[RAX]);
        break;
    }
}

extern void cjk_spy_0(void), cjk_spy_1(void);
extern uintptr_t cjk_spy_0_cont, cjk_spy_1_cont;

void spy_install(void) {
    if (!getenv("EU4CJK_SPY")) return;
    // 0x100e0d933: add rsp,0x10 ; mov rsi,r14 (7 字节)
    cjk_spy_0_cont = (uintptr_t)hook_tramp(GAME(0x100e0d933), 7);
    hook_jmp(GAME(0x100e0d933), 7, (void *)cjk_spy_0);
    // 0x100e0d9ca: add rsp,0x40 (4) ; mov r14,r13 (3)
    cjk_spy_1_cont = (uintptr_t)hook_tramp(GAME(0x100e0d9ca), 7);
    hook_jmp(GAME(0x100e0d9ca), 7, (void *)cjk_spy_1);
    // 四个函数的入口: push rbp; mov rbp,rsp; push r15 (6 字节)
    extern void cjk_spy_2(void), cjk_spy_3(void), cjk_spy_4(void), cjk_spy_5(void);
    extern uintptr_t cjk_spy_2_cont, cjk_spy_3_cont, cjk_spy_4_cont, cjk_spy_5_cont;
    uintptr_t fn[] = {GAME(0x10147024c), GAME(0x10146d3e0), GAME(0x101475ec6), GAME(0x101475648)};
    void (*stub[])(void) = {cjk_spy_2, cjk_spy_3, cjk_spy_4, cjk_spy_5};
    uintptr_t *cont[] = {&cjk_spy_2_cont, &cjk_spy_3_cont, &cjk_spy_4_cont, &cjk_spy_5_cont};
    for (int i = 0; i < 4; i++) { *cont[i] = (uintptr_t)hook_tramp(fn[i], 6); hook_jmp(fn[i], 6, (void *)stub[i]); }
    cjk_log("探针装上了");
}
