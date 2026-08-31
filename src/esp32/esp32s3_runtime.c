// Minimal freestanding C runtime for the ESP32-S3 Call0 ABI
//
// Copyright (C) 2026  Xiaokui Zhao <xiaok@zxkxz.cn>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include <stddef.h>
#include <stdint.h>

#define RUNTIME_ENTRY __attribute__((externally_visible, used))

RUNTIME_ENTRY
void *
memcpy(void *restrict dest, const void *restrict src, size_t count)
{
    uint8_t *d = dest;
    const uint8_t *s = src;
    while (count--)
        *d++ = *s++;
    return dest;
}

RUNTIME_ENTRY
void *
memmove(void *dest, const void *src, size_t count)
{
    uint8_t *d = dest;
    const uint8_t *s = src;
    if ((uintptr_t)d <= (uintptr_t)s) {
        while (count--)
            *d++ = *s++;
    } else {
        d += count;
        s += count;
        while (count--)
            *--d = *--s;
    }
    return dest;
}

RUNTIME_ENTRY
void *
memset(void *dest, int value, size_t count)
{
    uint8_t *d = dest;
    while (count--)
        *d++ = value;
    return dest;
}

RUNTIME_ENTRY
void *
memchr(const void *src, int value, size_t count)
{
    const uint8_t *s = src;
    while (count--) {
        if (*s == (uint8_t)value)
            return (void *)s;
        s++;
    }
    return NULL;
}

RUNTIME_ENTRY
int
memcmp(const void *left, const void *right, size_t count)
{
    const uint8_t *l = left, *r = right;
    while (count--) {
        if (*l != *r)
            return *l - *r;
        l++;
        r++;
    }
    return 0;
}

RUNTIME_ENTRY
int
strcmp(const char *left, const char *right)
{
    while (*left && *left == *right) {
        left++;
        right++;
    }
    return (uint8_t)*left - (uint8_t)*right;
}

RUNTIME_ENTRY
uint64_t
__ashldi3(uint64_t value, int shift)
{
    union { uint64_t full; uint32_t word[2]; } data = { value };
    while (shift-- > 0) {
        data.word[1] = (data.word[1] << 1) | (data.word[0] >> 31);
        data.word[0] <<= 1;
    }
    return data.full;
}

RUNTIME_ENTRY
int64_t
__ashrdi3(int64_t value, int shift)
{
    union { int64_t full; struct { uint32_t low; int32_t high; } word; } data
        = { value };
    while (shift-- > 0) {
        data.word.low = (data.word.low >> 1) | ((uint32_t)data.word.high << 31);
        data.word.high >>= 1;
    }
    return data.full;
}

RUNTIME_ENTRY
uint64_t
__lshrdi3(uint64_t value, int shift)
{
    union { uint64_t full; uint32_t word[2]; } data = { value };
    while (shift-- > 0) {
        data.word[0] = (data.word[0] >> 1) | (data.word[1] << 31);
        data.word[1] >>= 1;
    }
    return data.full;
}
