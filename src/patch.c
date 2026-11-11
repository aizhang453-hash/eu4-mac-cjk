// 改游戏代码用的小工具: 写内存、做跳转。
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <string.h>
#include <stdint.h>
#include "patch.h"

int patch_write(uintptr_t addr, const void *bytes, size_t len) {
    mach_port_t task = mach_task_self();
    mach_vm_address_t page = addr & ~(mach_vm_address_t)0xFFF;
    mach_vm_size_t size = ((addr + len + 0xFFF) & ~(uintptr_t)0xFFF) - page;
    kern_return_t kr = mach_vm_protect(task, page, size, 0,
                                       VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY);
    if (kr != KERN_SUCCESS) return -1;
    memcpy((void *)addr, bytes, len);
    kr = mach_vm_protect(task, page, size, 0, VM_PROT_READ | VM_PROT_EXECUTE);
    if (kr != KERN_SUCCESS) return -2;
    return 0;
}

// 在 addr 写一个跳到 target 的指令 (14 字节: jmp [rip+0]; 后面跟 8 字节地址),
// 多出来的位置 (len > 14) 用 nop 填满。
int patch_jmp(uintptr_t addr, const void *target, size_t len) {
    uint8_t buf[64];
    if (len < 14 || len > sizeof buf) return -3;
    buf[0] = 0xFF; buf[1] = 0x25; buf[2] = buf[3] = buf[4] = buf[5] = 0;
    uint64_t t = (uint64_t)(uintptr_t)target;
    memcpy(buf + 6, &t, 8);
    memset(buf + 14, 0x90, len - 14);
    return patch_write(addr, buf, len);
}

int patch_expect(uintptr_t addr, const void *bytes, size_t len) {
    return memcmp((const void *)addr, bytes, len) == 0;
}
