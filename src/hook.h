#pragma once
#include <stddef.h>
#include <stdint.h>
extern intptr_t g_slide;
#define GAME(a) ((uintptr_t)(a) + (uintptr_t)g_slide)   // 游戏里的地址 (按 IDA/objdump 看到的) 换成运行时的地址
int  near_init(void);
void *near_alloc(size_t n);
int  hook_jmp(uintptr_t site, size_t len, const void *target);     // site 处改成跳到 target (len >= 5, 多的填 nop)
void *hook_tramp(uintptr_t site, size_t n);                        // 抄 site 开头 n 字节 + 跳回, 用来调用原函数
int  hook_probe(uintptr_t site, size_t len, uint64_t *counter);    // 计数: 每经过一次 counter+1, 原指令照样执行
