// 在游戏代码附近 (2GB 以内) 要一块内存放跳板, 这样改一处只要 5 个字节 (jmp rel32)。
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach-o/dyld.h>
#include <string.h>
#include "hook.h"
#include "patch.h"

intptr_t g_slide;
static uint8_t *near_base, *near_cur, *near_end;

int near_init(void) {
    // 游戏程序占到 0x102EBC000 为止 (__LINKEDIT 结尾), 从后面找空地方
    mach_vm_address_t want = GAME(0x102F00000);
    for (int i = 0; i < 256; i++, want += 0x100000) {
        mach_vm_address_t a = want;
        if (mach_vm_allocate(mach_task_self(), &a, 0x100000, VM_FLAGS_FIXED) == KERN_SUCCESS) {
            mach_vm_protect(mach_task_self(), a, 0x100000, 0,
                            VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE);
            near_base = near_cur = (uint8_t *)a;
            near_end = near_base + 0x100000;
            return 0;
        }
    }
    return -1;
}

void *near_alloc(size_t n) {
    n = (n + 15) & ~(size_t)15;
    if (!near_cur || near_cur + n > near_end) return NULL;
    void *p = near_cur; near_cur += n; return p;
}

static int rel32_ok(uintptr_t from, uintptr_t to) {
    intptr_t d = (intptr_t)to - (intptr_t)from;
    return d >= INT32_MIN && d <= INT32_MAX;
}

static void put_jmp_abs(uint8_t *p, const void *target) {   // 14 字节
    p[0] = 0xFF; p[1] = 0x25; memset(p + 2, 0, 4);
    uint64_t t = (uint64_t)(uintptr_t)target; memcpy(p + 6, &t, 8);
}

int hook_jmp(uintptr_t site, size_t len, const void *target) {
    if (len < 5 || len > 32) return -1;
    uint8_t *thunk = near_alloc(16);
    if (!thunk) return -2;
    put_jmp_abs(thunk, target);
    if (!rel32_ok(site + 5, (uintptr_t)thunk)) return -3;
    uint8_t buf[32];
    buf[0] = 0xE9;
    int32_t rel = (int32_t)((intptr_t)thunk - (intptr_t)(site + 5)); memcpy(buf + 1, &rel, 4);
    memset(buf + 5, 0x90, len - 5);
    return patch_write(site, buf, len);
}

void *hook_tramp(uintptr_t site, size_t n) {
    uint8_t *t = near_alloc(n + 5);
    if (!t) return NULL;
    memcpy(t, (void *)site, n);
    t[n] = 0xE9;
    int32_t rel = (int32_t)((intptr_t)(site + n) - (intptr_t)(t + n + 5)); memcpy(t + n + 1, &rel, 4);
    return t;
}

int hook_probe(uintptr_t site, size_t len, uint64_t *counter) {
    // 跳板: inc qword [counter] ; 原来的指令 ; jmp 回去
    uint8_t *t = near_alloc(7 + len + 5);
    if (!t || !rel32_ok((uintptr_t)t + 7, (uintptr_t)counter)) return -1;
    t[0] = 0x48; t[1] = 0xFF; t[2] = 0x05;
    int32_t rel = (int32_t)((intptr_t)counter - (intptr_t)(t + 7)); memcpy(t + 3, &rel, 4);
    memcpy(t + 7, (void *)site, len);
    t[7 + len] = 0xE9;
    rel = (int32_t)((intptr_t)(site + len) - (intptr_t)(t + 7 + len + 5)); memcpy(t + 7 + len + 1, &rel, 4);
    uint8_t buf[32]; buf[0] = 0xE9;
    rel = (int32_t)((intptr_t)t - (intptr_t)(site + 5)); memcpy(buf + 1, &rel, 4);
    memset(buf + 5, 0x90, len - 5);
    return patch_write(site, buf, len);
}
