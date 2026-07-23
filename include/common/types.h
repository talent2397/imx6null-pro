/**
 * @file    types.h
 * @brief   基础类型定义 — 平台无关的定长整数类型
 *
 * 嵌入式系统中必须使用定长类型 (u8/u16/u32/u64)，
 * 禁止使用 int/long 等平台相关的可变长度类型。
 *
 * 参考: MISRA C:2012 Dir 4.6, Rule 6.3
 */

#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ================================================================
 * 无符号定长整数 (推荐使用)
 * ================================================================ */
typedef uint8_t   u8;     /*  8-bit 无符号: 0 .. 255            */
typedef uint16_t  u16;    /* 16-bit 无符号: 0 .. 65535          */
typedef uint32_t  u32;    /* 32-bit 无符号: 0 .. 4294967295     */
typedef uint64_t  u64;    /* 64-bit 无符号: 0 .. 2^64-1         */

/* ================================================================
 * 有符号定长整数
 * ================================================================ */
typedef int8_t    s8;     /*  8-bit 有符号: -128 .. 127         */
typedef int16_t   s16;    /* 16-bit 有符号: -32768 .. 32767     */
typedef int32_t   s32;    /* 32-bit 有符号                       */
typedef int64_t   s64;    /* 64-bit 有符号                       */

/* ================================================================
 * 浮点类型 (条件使用 — 嵌入式尽量不用浮点)
 * ================================================================ */
typedef float     f32;
typedef double    f64;

/* ================================================================
 * 编译期断言 (用于类型大小验证)
 * ================================================================ */
#define STATIC_ASSERT(cond, msg)  _Static_assert(cond, msg)

/* 编译期检查类型大小是否正确 */
STATIC_ASSERT(sizeof(u8)  == 1, "u8 must be 1 byte");
STATIC_ASSERT(sizeof(u16) == 2, "u16 must be 2 bytes");
STATIC_ASSERT(sizeof(u32) == 4, "u32 must be 4 bytes");
STATIC_ASSERT(sizeof(u64) == 8, "u64 must be 8 bytes");

STATIC_ASSERT(sizeof(s8)  == 1, "s8 must be 1 byte");
STATIC_ASSERT(sizeof(s16) == 2, "s16 must be 2 bytes");
STATIC_ASSERT(sizeof(s32) == 4, "s32 must be 4 bytes");
STATIC_ASSERT(sizeof(s64) == 8, "s64 must be 8 bytes");

/* ================================================================
 * 常用宏
 * ================================================================ */

/** 获取数组中元素个数 */
#define ARRAY_SIZE(arr)  (sizeof(arr) / sizeof((arr)[0]))

/** 取两个数中的较小值 */
#define MIN(a, b)  ((a) < (b) ? (a) : (b))

/** 取两个数中的较大值 */
#define MAX(a, b)  ((a) > (b) ? (a) : (b))

/** 将值限制在 [lo, hi] 范围内 */
#define CLAMP(val, lo, hi)  (MIN(MAX((val), (lo)), (hi)))

/** 位掩码 — 生成从 bit0 开始的 n 位全1掩码 */
#define BIT_MASK(n)  ((1u << (n)) - 1)

/** 对齐到 size 的整数倍 (size 必须是 2 的幂) */
#define ALIGN_UP(addr, size)  (((addr) + (size) - 1) & ~((size) - 1))

/** 以字节为单位的结构体成员偏移量 */
#ifndef offsetof
#define offsetof(type, member)  ((size_t) &((type *)0)->member)
#endif

/** 从成员指针反推容器指针 */
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#endif /* COMMON_TYPES_H */
