/*
 * ============================================================================
 * libs/string.h —— 内存/字符串函数的声明（实现在 libs/string.c）
 * ============================================================================
 * 由于 -nostdinc，标准库的 string.h 用不了，这些函数只能自己实现。
 * lab1 真正用到的只有 memset（init.c 清 BSS），
 * 其余是给后续实验准备的常用工具，答辩时能说清每个函数的作用即可。
 * ============================================================================
 */

#ifndef __LIBS_STRING_H__
#define __LIBS_STRING_H__

#include <defs.h>

/* strlen：算字符串长度（不含结尾的 '\0'）。*/
size_t strlen(const char *s);
/* strnlen：最多只看 len 个字符的长度，防止越过缓冲区边界。*/
size_t strnlen(const char *s, size_t len);

/* strcpy/strncpy：字符串复制（strncpy 会把不够的字节补 '\0'）。*/
char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, size_t len);

/* strcmp/strncmp：字符串比较，返回 0 表示相等。*/
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);

/* strchr：查找字符第一次出现的位置，找不到返回 NULL；
 *   strfind：功能类似，但找不到时返回指向结尾 '\0' 的指针而不是 NULL。*/
char *strchr(const char *s, char c);
char *strfind(const char *s, char c);
/* strtol：把字符串按给定进制解析成整数（"123" -> 123）。*/
long strtol(const char *s, char **endptr, int base);

/* memset：把一段内存填成同一个字节 —— lab1 清 BSS 用的就是它。*/
void *memset(void *s, char c, size_t n);
/* memmove：内存拷贝，允许源和目的区域重叠（内部会判断方向）。*/
void *memmove(void *dst, const void *src, size_t n);
/* memcpy：内存拷贝，不允许重叠（更快，但没有保护）。*/
void *memcpy(void *dst, const void *src, size_t n);
/* memcmp：按字节比较两块内存。*/
int memcmp(const void *v1, const void *v2, size_t n);

#endif /* !__LIBS_STRING_H__ */

