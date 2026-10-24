/*
 * ============================================================================
 * libs/defs.h —— ucore 的"基础定义"头文件（没有它就编不过）
 * ============================================================================
 * 为什么需要它？因为编译内核时用了 -nostdinc，不能用宿主机的标准头文件，
 * 所以 NULL、整数类型、size_t、常用宏……全都要自己定义。
 * 手册 [22] 节专门摘录了这个文件，说明它属于 lab1 要掌握的内容。
 * ============================================================================
 */

#ifndef __LIBS_DEFS_H__
#define __LIBS_DEFS_H__

/* NULL 指针的定义；用 #ifndef 保护，避免和别的头文件重复定义。*/
#ifndef NULL
#define NULL ((void *)0)
#endif

/* 几个编译器属性宏的封装，让代码更可读：
 *   always_inline：强制内联，用于极短的小函数；
 *   noinline：禁止内联（调试时想让函数单独存在就用它）；
 *   noreturn：告诉编译器"此函数不返回"，init.c 的 kern_init 就用了它。*/
#define __always_inline inline __attribute__((always_inline))
#define __noinline __attribute__((noinline))
#define __noreturn __attribute__((noreturn))

/* Represents true-or-false values */
/* 自己定义 bool（C99 要 <stdbool.h>，我们不用标准头）。
 *   注意它是 int 而不是 _Bool，值 0/1 表示假/真。*/
typedef int bool;

/* 定长整数类型：*_t 系列。内核里到处要精确控制宽度
 *   （比如 sbiret 的返回值必须 64 位），所以自己定义一遍。
 *   注意 int64_t 用 long long、uint64_t 用 unsigned long long：
 *   在 64 位 RISC-V 上它们都是 8 字节。*/
/* Explicitly-sized versions of integer types */
typedef char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;

/* 定长"最快"类型，属于历史遗留，lab1 未使用。*/
/* Add fast types */
typedef signed char int_fast8_t;
typedef short int_fast16_t;
typedef long int_fast32_t;
typedef long long int_fast64_t;

typedef unsigned char uint_fast8_t;
typedef unsigned short uint_fast16_t;
typedef unsigned long uint_fast32_t;
typedef unsigned long long uint_fast64_t;

/* 各整数类型的最小值/最大值。
 *   写法 (-0x7f - 1) 而不是直接 -0x80，是为了避免"负号加字面量"
 *   被当成无符号数计算的坑。*/
#define INT8_MIN    (-0x7f - 1)
#define INT16_MIN   (-0x7fff - 1)
#define INT32_MIN   (-0x7fffffff - 1)
#define INT64_MIN   (-0x7fffffffffffffff - 1)

#define INT8_MAX    (0x7f)
#define INT16_MAX   (0x7fff)
#define INT32_MAX   (0x7fffffff)
#define INT64_MAX   (0x7fffffffffffffff)

#define UINT8_MAX   (0xff)
#define UINT16_MAX  (0xffff)
#define UINT32_MAX  (0xffffffff)
#define UINT64_MAX  (0xffffffffffffffff)

/*
 * 指针与地址：在 64 位 RISC-V 上，地址就是 64 位整数。
 * ucore 的约定是——用指针类型表示"地址"，用 uintptr_t 表示"地址的数值"，
 * 这样指针运算和整数运算的意图一目了然。
 */
/* *
 * Pointers and addresses are 32 bits long.
 * We use pointer types to represent addresses,
 * uintptr_t to represent the numerical values of addresses.
 * */
typedef int64_t intptr_t;
typedef uint64_t uintptr_t;

/* size_t：表示对象大小/长度，内核里用无符号 64 位。
 *   注意它就是我们自己定义的 uintptr_t，标准库的 <stddef.h> 不可用。*/
/* size_t is used for memory object sizes */
typedef uintptr_t size_t;

/* ppn_t：physical page number（物理页号），lab2 页表实验会大量使用。*/
/* used for page numbers */
typedef size_t ppn_t;

/*
 * 取整宏（答辩常问）：
 *   ({ ... }) 是 GCC 的"语句表达式"，让宏能像函数一样返回最后一个表达式的值；
 *   typeof(a) 保证返回值类型和参数一致；
 *   要求 n 是 2 的幂时效率最高（编译器会把 % 和 / 优化成位运算）。
 * ROUNDDOWN(7,4)=4，ROUNDUP(7,4)=8，将来对齐页边界全靠它们。
 */
/* *
 * Rounding operations (efficient when n is a power of 2)
 * Round down to the nearest multiple of n
 * */
#define ROUNDDOWN(a, n) ({                                          \
            size_t __a = (size_t)(a);                               \
            (typeof(a))(__a - __a % (n));                           \
        })

/* 向上取整到 n 的倍数：先加 n-1 再向下取整。*/
/* Round up to the nearest multiple of n */
#define ROUNDUP(a, n) ({                                            \
            size_t __n = (size_t)(n);                               \
            (typeof(a))(ROUNDDOWN((size_t)(a) + __n - 1, __n));     \
        })

/*
 * offsetof：求结构体成员 relative 于结构体首地址的字节偏移。
 * 技巧在于把 0 强制转成 type *，于是成员地址的数值就等于偏移量。
 */
/* Return the offset of 'member' relative to the beginning of a struct type */
#define offsetof(type, member)                                      \
    ((size_t)(&((type *)0)->member))

/*
 * to_struct：从"成员的指针"反推出"整个结构体的指针"。
 * 用途：把成员节点挂进链表，需要时再倒推回宿主结构体。
 * 这是 lab3 之后的进程管理、lab4 之后的链表操作的核心技巧，
 * lab1 只是把它定义好（属于"未来会被用到"的基础设施）。
 */
/* *
 * to_struct - get the struct from a ptr
 * @ptr:    a struct pointer of member
 * @type:   the type of the struct this is embedded in
 * @member: the name of the member within the struct
 * */
#define to_struct(ptr, type, member)                               \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#endif /* !__LIBS_DEFS_H__ */

