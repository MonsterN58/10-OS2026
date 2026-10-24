/*
 * ============================================================================
 * libs/string.c —— 内存/字符串函数实现（ucore 自己的"标准库"）
 * ============================================================================
 * lab1 里真正被调用的是 memset（init.c 用它把 .bss 段清零），
 * 其余是同族工具，后续实验会大量使用。
 * 【答辩要点一】为什么要自己实现？因为没有 glibc —— 我们正在写操作系统，
 *   不能再依赖另一个操作系统的运行库（-nostdinc 也印证了这一点）。
 * 【答辩要点二】下列函数大多带 #ifdef __HAVE_ARCH_XXX 分支：
 *   这是 ucore 从 x86 版继承下来的"可替换实现"机制，
 *   如果某个架构提供了汇编优化的版本（__strcpy 等），就优先用它；
 *   RISC-V 版没有定义这些宏，所以走的都是 C 语言实现分支。
 * ============================================================================
 */

#include <string.h>

/* *
 * strlen - calculate the length of the string @s, not including
 * the terminating '\0' character.
 * @s:        the input string
 *
 * The strlen() function returns the length of string @s.
 * */
/* strlen：从字符串头一直数到 '\0'，返回字符个数（不含 '\0'）。*/
size_t
strlen(const char *s) {
    size_t cnt = 0;
    while (*s ++ != '\0') {
        cnt ++;
    }
    return cnt;
}

/* *
 * strnlen - calculate the length of the string @s, not including
 * the terminating '\0' char acter, but at most @len.
 * @s:        the input string
 * @len:    the max-length that function will scan
 *
 * Note that, this function looks only at the first @len characters
 * at @s, and never beyond @s + @len.
 *
 * The return value is strlen(s), if that is less than @len, or
 * @len if there is no '\0' character among the first @len characters
 * pointed by @s.
 * */
/* strnlen：strlen 的安全版，最多只数 len 个字符。
 *   用在 printfmt 的 %s 里：即使字符串没有结束符也不会越界扫内存。*/
size_t
strnlen(const char *s, size_t len) {
    size_t cnt = 0;
/* 注意条件顺序：先比较 cnt < len，再解引用 s。
 *   如果写反了，缓冲区没有 '\0' 时就会越界读。*/
    while (cnt < len && *s ++ != '\0') {
        cnt ++;
    }
    return cnt;
}

/* *
 * strcpy - copies the string pointed by @src into the array pointed by @dst,
 * including the terminating null character.
 * @dst:    pointer to the destination array where the content is to be copied
 * @src:    string to be copied
 *
 * The return value is @dst.
 *
 * To avoid overflows, the size of array pointed by @dst should be long enough to
 * contain the same string as @src (including the terminating null character), and
 * should not overlap in memory with @src.
 * */
/* strcpy：把 src（含结尾 '\0'）整体拷到 dst，返回 dst。
 *   这一行 while 的写法很经典：赋值表达式的值就是刚写入的字符，
 *   所以"边拷贝边判断有没有到结尾"，一次循环干两件事。*/
char *
strcpy(char *dst, const char *src) {
/* 架构优化分支：如果提供了 __strcpy（如汇编版），就用它。*/
#ifdef __HAVE_ARCH_STRCPY
    return __strcpy(dst, src);
#else
    char *p = dst;
    while ((*p ++ = *src ++) != '\0')
        /* nothing */;
    return dst;
#endif /* __HAVE_ARCH_STRCPY */
}

/* *
 * strncpy - copies the first @len characters of @src to @dst. If the end of string @src
 * if found before @len characters have been copied, @dst is padded with '\0' until a
 * total of @len characters have been written to it.
 * @dst:    pointer to the destination array where the content is to be copied
 * @src:    string to be copied
 * @len:    maximum number of characters to be copied from @src
 *
 * The return value is @dst
 * */
/* strncpy：最多拷贝 len 个字节。
 *   与标准 strncpy 一致：源串提前结束时，剩下的字节用 '\0' 补满。*/
char *
strncpy(char *dst, const char *src, size_t len) {
    char *p = dst;
/* 循环恰好 len 次，所以无论源串多短，都会写满 len 个字节。*/
    while (len > 0) {
/* 只有当源字符不是 '\0' 时才推进 src；这是"补 '\0'"的关键。*/
        if ((*p = *src) != '\0') {
            src ++;
        }
        p ++, len --;
    }
    return dst;
}

/* *
 * strcmp - compares the string @s1 and @s2
 * @s1:        string to be compared
 * @s2:        string to be compared
 *
 * This function starts comparing the first character of each string. If
 * they are equal to each other, it continues with the following pairs until
 * the characters differ or until a terminanting null-character is reached.
 *
 * Returns an integral value indicating the relationship between the strings:
 * - A zero value indicates that both strings are equal;
 * - A value greater than zero indicates that the first character that does
 *   not match has a greater value in @s1 than in @s2;
 * - And a value less than zero indicates the opposite.
 * */
/* strcmp：比较两个字符串。
 *   返回 0 表示相等；正数表示 s1 大；负数表示 s2 大。*/
int
strcmp(const char *s1, const char *s2) {
#ifdef __HAVE_ARCH_STRCMP
    return __strcmp(s1, s2);
#else
/* 相等就一起后移，直到发现不同字符或者 s1 结束。*/
    while (*s1 != '\0' && *s1 == *s2) {
        s1 ++, s2 ++;
    }
/* 强制转成 unsigned char 再相减，避免 char 为有符号时
 *   （比如 0x80 和 0x01 比较）得到错误的符号结果 —— 经典坑，答辩常问。*/
    return (int)((unsigned char)*s1 - (unsigned char)*s2);
#endif /* __HAVE_ARCH_STRCMP */
}

/* *
 * strncmp - compares up to @n characters of the string @s1 to those of the string @s2
 * @s1:        string to be compared
 * @s2:        string to be compared
 * @n:        maximum number of characters to compare
 *
 * This function starts comparing the first character of each string. If
 * they are equal to each other, it continues with the following pairs until
 * the characters differ, until a terminating null-character is reached, or
 * until @n characters match in both strings, whichever happens first.
 * */
/* strncmp：只比较前 n 个字符，比较逻辑同 strcmp。*/
int
strncmp(const char *s1, const char *s2, size_t n) {
    while (n > 0 && *s1 != '\0' && *s1 == *s2) {
        n --, s1 ++, s2 ++;
    }
/* 已经比较了 n 个字符就返回 0（相等）；
 *   否则返回第一处不同的差值。*/
    return (n == 0) ? 0 : (int)((unsigned char)*s1 - (unsigned char)*s2);
}

/* *
 * strchr - locates first occurrence of character in string
 * @s:        the input string
 * @c:        character to be located
 *
 * The strchr() function returns a pointer to the first occurrence of
 * character in @s. If the value is not found, the function returns 'NULL'.
 * */
/* strchr：找字符 c 第一次出现的位置，找不到返回 NULL。*/
char *
strchr(const char *s, char c) {
    while (*s != '\0') {
        if (*s == c) {
            return (char *)s;
        }
        s ++;
    }
    return NULL;
}

/* *
 * strfind - locates first occurrence of character in string
 * @s:        the input string
 * @c:        character to be located
 *
 * The strfind() function is like strchr() except that if @c is
 * not found in @s, then it returns a pointer to the null byte at the
 * end of @s, rather than 'NULL'.
 * */
/* strfind：与 strchr 的唯一区别是"找不到时返回指向结尾 '\0' 的指针"
 *   而不是 NULL —— 这样调用者永远拿到一个合法指针，省去判空。*/
char *
strfind(const char *s, char c) {
    while (*s != '\0') {
        if (*s == c) {
            break;
        }
        s ++;
    }
    return (char *)s;
}

/* *
 * strtol - converts string to long integer
 * @s:        the input string that contains the representation of an integer number
 * @endptr:    reference to an object of type char *, whose value is set by the
 *             function to the next character in @s after the numerical value. This
 *             parameter can also be a null pointer, in which case it is not used.
 * @base:    x
 *
 * The function first discards as many whitespace characters as necessary until
 * the first non-whitespace character is found. Then, starting from this character,
 * takes as many characters as possible that are valid following a syntax that
 * depends on the base parameter, and interprets them as a numerical value. Finally,
 * a pointer to the first character following the integer representation in @s
 * is stored in the object pointed by @endptr.
 *
 * If the value of base is zero, the syntax expected is similar to that of
 * integer constants, which is formed by a succession of:
 * - An optional plus or minus sign;
 * - An optional prefix indicating octal or hexadecimal base ("0" or "0x" respectively)
 * - A sequence of decimal digits (if no base prefix was specified) or either octal
 *   or hexadecimal digits if a specific prefix is present
 *
 * If the base value is between 2 and 36, the format expected for the integral number
 * is a succession of the valid digits and/or letters needed to represent integers of
 * the specified radix (starting from '0' and up to 'z'/'Z' for radix 36). The
 * sequence may optionally be preceded by a plus or minus sign and, if base is 16,
 * an optional "0x" or "0X" prefix.
 *
 * The strtol() function returns the converted integral number as a long int value.
 * */
/* strtol：把字符串按 base 进制解析成 long（"123" -> 123，支持 0x/0 前缀）。
 *   手写实现，标准库的 atoi/strtol 在这里不可用。*/
long
/* neg 记录符号，val 是累加结果（注意用 long 而不是无符号）。*/
strtol(const char *s, char **endptr, int base) {
    int neg = 0;
    long val = 0;

    // gobble initial whitespace
/* 第一步：跳过开头的空格和制表符。*/
    while (*s == ' ' || *s == '\t') {
        s ++;
    }

    // plus/minus sign
/* 第二步：处理正负号。*/
    if (*s == '+') {
        s ++;
    }
    else if (*s == '-') {
        s ++, neg = 1;
    }

    // hex or octal base prefix
/* 第三步：确定进制。
 *   base==0 表示"按字面量风格自动判断"：
 *     0x 开头 -> 16 进制；0 开头 -> 8 进制；否则 10 进制。*/
    if ((base == 0 || base == 16) && (s[0] == '0' && s[1] == 'x')) {
        s += 2, base = 16;
    }
/* 0 开头但没有 x：八进制，并且跳过这个前导 0。*/
    else if (base == 0 && s[0] == '0') {
        s ++, base = 8;
    }
/* 都不符合：十进制。*/
    else if (base == 0) {
        base = 10;
    }

    // digits
/* 第四步：逐位累加：val = val * base + dig。*/
    while (1) {
        int dig;

/* 数字字符 '0'-'9' 转成 0-9。*/
        if (*s >= '0' && *s <= '9') {
            dig = *s - '0';
        }
/* 小写字母 'a'-'z' 转成 10-35。*/
        else if (*s >= 'a' && *s <= 'z') {
            dig = *s - 'a' + 10;
        }
/* 大写字母同理（进制最大 36，所以支持到 'z'/'Z'）。*/
        else if (*s >= 'A' && *s <= 'Z') {
            dig = *s - 'A' + 10;
        }
/* 不是合法字符就结束解析。*/
        else {
            break;
        }
/* 数字值超过进制范围（例如 9 出现在八进制里）也结束。*/
        if (dig >= base) {
            break;
        }
        s ++, val = (val * base) + dig;
/* 注释是实话：这里没有做溢出检测，超出 long 范围会静默出错。*/
        // we don't properly detect overflow!
    }

/* 把"解析停止的位置"写回给调用者，方便继续解析后面的内容。*/
    if (endptr) {
        *endptr = (char *) s;
    }
/* 最后按符号取负。*/
    return (neg ? -val : val);
}

/* *
 * memset - sets the first @n bytes of the memory area pointed by @s
 * to the specified value @c.
 * @s:        pointer the the memory area to fill
 * @c:        value to set
 * @n:        number of bytes to be set to the value
 *
 * The memset() function returns @s.
 * */
/* memset —— 【lab1 唯一被调用的函数】把从 s 开始的 n 个字节都写成 c。
 *   init.c 里 memset(edata, 0, end - edata) 就是靠它清空 .bss 段。*/
void *
memset(void *s, char c, size_t n) {
#ifdef __HAVE_ARCH_MEMSET
    return __memset(s, c, n);
#else
    char *p = s;
/* n-- > 0 的写法：先用 n 的原值判断，再自减，于是恰好执行 n 次。*/
    while (n -- > 0) {
        *p ++ = c;
    }
    return s;
#endif /* __HAVE_ARCH_MEMSET */
}

/* *
 * memmove - copies the values of @n bytes from the location pointed by @src to
 * the memory area pointed by @dst. @src and @dst are allowed to overlap.
 * @dst        pointer to the destination array where the content is to be copied
 * @src        pointer to the source of data to by copied
 * @n:        number of bytes to copy
 *
 * The memmove() function returns @dst.
 * */
/* memmove：拷贝 n 个字节，允许源和目的区域重叠（这是它与 memcpy 的区别）。*/
void *
memmove(void *dst, const void *src, size_t n) {
#ifdef __HAVE_ARCH_MEMMOVE
    return __memmove(dst, src, n);
#else
    const char *s = src;
    char *d = dst;
/* 重叠检测：源在目的之前，且两区域相交时，
 *   必须"从后往前"拷贝，否则前面的字节会被覆盖掉。
 *   这是经典面试题，也是内核里做内存搬移时最容易出的错。*/
    if (s < d && s + n > d) {
        s += n, d += n;
/* 从末尾开始反向拷贝。*/
        while (n -- > 0) {
            *-- d = *-- s;
        }
/* 不重叠（或源在目的之后）：从前往后拷贝即可。*/
    } else {
        while (n -- > 0) {
            *d ++ = *s ++;
        }
    }
    return dst;
#endif /* __HAVE_ARCH_MEMMOVE */
}

/* *
 * memcpy - copies the value of @n bytes from the location pointed by @src to
 * the memory area pointed by @dst.
 * @dst        pointer to the destination array where the content is to be copied
 * @src        pointer to the source of data to by copied
 * @n:        number of bytes to copy
 *
 * The memcpy() returns @dst.
 *
 * Note that, the function does not check any terminating null character in @src,
 * it always copies exactly @n bytes. To avoid overflows, the size of arrays pointed
 * by both @src and @dst, should be at least @n bytes, and should not overlap
 * (for overlapping memory area, memmove is a safer approach).
 * */
/* memcpy：只做从前往后的拷贝，速度更快但要求两块区域不重叠。
 *   注释里特意提醒：要处理重叠请用 memmove。*/
void *
memcpy(void *dst, const void *src, size_t n) {
#ifdef __HAVE_ARCH_MEMCPY
    return __memcpy(dst, src, n);
#else
    const char *s = src;
    char *d = dst;
    while (n -- > 0) {
        *d ++ = *s ++;
    }
    return dst;
#endif /* __HAVE_ARCH_MEMCPY */
}

/* *
 * memcmp - compares two blocks of memory
 * @v1:        pointer to block of memory
 * @v2:        pointer to block of memory
 * @n:        number of bytes to compare
 *
 * The memcmp() functions returns an integral value indicating the
 * relationship between the content of the memory blocks:
 * - A zero value indicates that the contents of both memory blocks are equal;
 * - A value greater than zero indicates that the first byte that does not
 *   match in both memory blocks has a greater value in @v1 than in @v2
 *   as if evaluated as unsigned char values;
 * - And a value less than zero indicates the opposite.
 * */
/* memcmp：逐字节比较两块内存，返回第一个不同字节的差值（同 strcmp 的约定）。*/
int
memcmp(const void *v1, const void *v2, size_t n) {
    const char *s1 = (const char *)v1;
    const char *s2 = (const char *)v2;
/* 发现不同立刻返回；注意同样要转成 unsigned char 来比较。*/
    while (n -- > 0) {
        if (*s1 != *s2) {
            return (int)((unsigned char)*s1 - (unsigned char)*s2);
        }
        s1 ++, s2 ++;
    }
/* n 个字节全部相同则返回 0。*/
    return 0;
}

