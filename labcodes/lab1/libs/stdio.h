/*
 * ============================================================================
 * libs/stdio.h —— ucore 标准 I/O 的接口声明
 * ============================================================================
 * 注意三个函数来自三个不同的源文件（下面注释里标得很清楚），
 * 这正是 lab1 "分层"设计的体现：格式化（libs/printfmt.c）与
 * 设备输出（kern/driver/console.c）被 stdio.c 的中间层粘在一起。
 * ============================================================================
 */

#ifndef __LIBS_STDIO_H__
#define __LIBS_STDIO_H__

#include <defs.h>
#include <stdarg.h>

/* 以下 5 个函数定义在 kern/libs/stdio.c：内核自己用的"标准"输出。*/
/* kern/libs/stdio.c */
/* cprintf：我们的 printf，支持 %d/%s/%x/%p/%e 等格式。*/
int cprintf(const char *fmt, ...);
/* vcprintf：当你手上已经有 va_list 时用它。*/
int vcprintf(const char *fmt, va_list ap);
/* cputchar：输出单个字符。*/
void cputchar(int c);
/* cputs：输出字符串并自动换行。*/
int cputs(const char *str);
/* getchar：轮询读一个字符。*/
int getchar(void);

/* readline 定义在 libs/readline.c：读一整行（带提示符和回显）。*/
/* libs/readline.c */
char *readline(const char *prompt);

/* 以下 4 个定义在 libs/printfmt.c：真正干格式化活的核心。*/
/* libs/printfmt.c */
/* printfmt：格式化输出，输出动作由调用者提供的 putch 决定
 *   （因此可以输出到屏幕，也可以输出到缓冲区 snprintf）。*/
void printfmt(void (*putch)(int, void *), void *putdat, const char *fmt, ...);
/* vprintfmt：同上，但接收 va_list。整个 lab1 的"大脑"就是它的实现。*/
void vprintfmt(void (*putch)(int, void *), void *putdat, const char *fmt, va_list ap);
/* snprintf/vsnprintf：把格式化结果写进字符缓冲区（读入实验会用到）。*/
int snprintf(char *str, size_t size, const char *fmt, ...);
int vsnprintf(char *str, size_t size, const char *fmt, va_list ap);

#endif /* !__LIBS_STDIO_H__ */

