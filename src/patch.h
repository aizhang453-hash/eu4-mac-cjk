#pragma once
#include <stddef.h>
#include <stdint.h>
int patch_write(uintptr_t addr, const void *bytes, size_t len);
int patch_jmp(uintptr_t addr, const void *target, size_t len);
int patch_expect(uintptr_t addr, const void *bytes, size_t len);
