/*
 * ============================================================================
 * libs/stdarg.h —— 变参（可变参数）支持（答辩高频考点）
 * ============================================================================
 * printf 系列的函数都带 "..."，那么它怎么知道后面有几个参数、各是什么
 * 类型？答案是：靠格式字符串推断类型，靠编译器提供的 va_list 机制
 * 在参数区里"游走"。这个头文件就是把编译器的内建能力包装成标准名字。
 * 【RISC-V 调用约定补充】前 8 个整型/指针参数放在 a0~a7 寄存器，
 * 多出来的参数放在栈上；所以 va_arg 取参数时要小心寄存器溢出区。
 * 这些细节由编译器实现，我们只需正确使用 va_start/va_arg/va_end。
 * ============================================================================
 */

#ifndef __LIBS_STDARG_H__
#define __LIBS_STDARG_H__

/* compiler provides size of save area */
/* 直接使用编译器内建类型，不需要自己写结构体。
 *   这也解释了 init.c/stdio.c 里为什么不能 #include <stdarg.h>：
 *   主机上的那个 stdarg.h 是给 glibc 用的，我们只能用自己的。*/
typedef __builtin_va_list va_list;

/* va_start：把 ap 定位到"最后一个具名参数"之后的可变参数起点，
 *   所以调用时第二个参数必须传最后一个具名参数的名字（如 fmt）。*/
#define va_start(ap, last)              (__builtin_va_start(ap, last))
/* va_arg：取出一个 type 类型的参数，并把游标往后移动 sizeof(type)
 *   （还要满足对齐要求）。类型必须传对，否则会读到错位的数据。*/
#define va_arg(ap, type)                (__builtin_va_arg(ap, type))
/* va_end：清理游标。GCC 下是空宏，但规范要求成对出现。*/
#define va_end(ap)                      /*nothing*/

#endif /* !__LIBS_STDARG_H__ */

