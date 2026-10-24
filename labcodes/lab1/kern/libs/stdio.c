/*
 * ============================================================================
 * kern/libs/stdio.c —— ucore 版的"标准输出"（lab1 打印链的中层，答辩重点）
 * ============================================================================
 * 【为什么要自己写 printf？】
 *   因为我们正在写操作系统本身，不能依赖 glibc（那是"寄生"在别人操作
 *   系统上的库）。所以从 cputchar、cputs 到 cprintf，全部自己实现。
 * 【完整调用链（背下来，答辩必考）】
 *   cprintf()  -> vcprintf() -> vprintfmt()   [格式化，libs/printfmt.c]
 *               -> cputch()   -> cons_putc()  [kern/driver/console.c]
 *               -> sbi_console_putchar()      [libs/sbi.c，内联汇编]
 *               -> ecall -> OpenSBI（M 态） -> QEMU 屏幕
 * ============================================================================
 */
/* console.h 提供 cons_putc / cons_getc（设备层）。*/
#include <console.h>
/* defs.h 提供 va_list 之外的定长类型、宏等。*/
#include <defs.h>
/* stdio.h 是 ucore 自己的头文件，声明了本文件里所有函数。*/
#include <stdio.h>

/* 以下属于"高层"控制台 I/O：负责字符计数、行缓冲、格式化，不管设备细节。*/
/* HIGH level console I/O */

/*
 * cputch —— 输出一个字符并累加已输出字符数。
 * 注意它的两个"特殊之处"，这是 printfmt.c 能和它协作的原因：
 *   1. 形参类型必须是 (int, void*)，因为 vprintfmt 收的是函数指针
 *      void (*putch)(int, void *)，这里的 void * 就是 &cnt；
 *   2. 每输出一个字符就 (*cnt)++，于是 printfmt 能返回"输出了多少字符"。
 * 前面加 static：只在本文件内可见，这是内部辅助函数的惯例。
 */
/* *
 * cputch - writes a single character @c to stdout, and it will
 * increace the value of counter pointed by @cnt.
 * */
static void cputch(int c, int *cnt) {
    cons_putc(c);
    (*cnt)++;
}

/*
 * vcprintf —— 用已经取好的 va_list 做格式化输出，返回输出字符数。
 * 为什么要把 cprintf / vcprintf 拆成两个函数？
 *   因为变参函数（...）不能把可变参数继续往下一层传，
 *   必须先用 va_start 把参数"打包"成 va_list，才能传给 vprintfmt。
 *   这是 C 语言变参机制的经典写法，答辩常被问到。
 */
/* *
 * vcprintf - format a string and writes it to stdout
 *
 * The return value is the number of characters which would be
 * written to stdout.
 *
 * Call this function if you are already dealing with a va_list.
 * Or you probably want cprintf() instead.
 * */
/* 计数器从 0 开始，cputch 会不断把它 +1。*/
int vcprintf(const char *fmt, va_list ap) {
    int cnt = 0;
/* 把"输出一个字符"的动作(cputch)和它的上下文(&cnt)一起交给格式化器；
 *   vprintfmt 负责解析 %d/%s 等格式，遇到普通字符就回调 cputch。
 *   这里把函数名强制转成 (void *) 是为了匹配接口的声明形式；
 *   更严谨的写法是 (void (*)(int, void *))cputch，两种在 ucore 里都出现。*/
    vprintfmt((void *)cputch, &cnt, fmt, ap);
    return cnt;
}

/*
 * cprintf —— 我们最常用的"内核 printf"，返回输出的字符个数。
 * 形参 ... 是可变参数，用 va_start/va_end 取出后交给 vcprintf。
 */
/* *
 * cprintf - formats a string and writes it to stdout
 *
 * The return value is the number of characters which would be
 * written to stdout.
 * */
int cprintf(const char *fmt, ...) {
/* va_list 是"参数游标"，初始指向 fmt 后面的第一个可变参数。*/
    va_list ap;
    int cnt;
/* va_start(ap, fmt)：把 ap 定位到 fmt 之后；最后一个具名参数必须是 fmt。*/
    va_start(ap, fmt);
    cnt = vcprintf(fmt, ap);
/* va_end 在 GCC 里是空宏，但写上它是为了可移植性和规范。*/
    va_end(ap);
    return cnt;
}

/* cputchar - writes a single character to stdout */
/* cputchar —— 只输出一个字符，不计数，供 readline 回显等场景使用。*/
void cputchar(int c) { cons_putc(c); }

/*
 * cputs —— 输出字符串并自动补一个换行（注意它的名字是"c + puts"）。
 * 它自己用循环调用 cputch 来统计长度，所以它不依赖 printfmt。
 */
/* *
 * cputs- writes the string pointed by @str to stdout and
 * appends a newline character.
 * */
int cputs(const char *str) {
    int cnt = 0;
    char c;
/* 注意是 *str++：先取当前字符，再把指针后移。*/
    while ((c = *str++) != '\0') {
        cputch(c, &cnt);
    }
/* 字符串输出完补一个 '\n'，这是 cputs 与普通 puts 相同的约定。*/
    cputch('\n', &cnt);
    return cnt;
}

/*
 * getchar —— 忙等（轮询）读取一个字符。
 * while 循环不停调用 cons_getc()，返回 0 表示"暂时没有输入"，
 * 于是它一直空转直到真拿到字符。这种"轮询"很浪费 CPU，
 * 后面实验用中断 + 等待队列（sleep）来改进，这是一个很好的答辩对比点。
 */
/* getchar - reads a single non-zero character from stdin */
int getchar(void) {
    int c;
    while ((c = cons_getc()) == 0) /* do nothing */;
    return c;
}
