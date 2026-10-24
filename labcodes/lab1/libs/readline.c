/*
 * ============================================================================
 * libs/readline.c —— 读取一整行输入（lab1 编译进来但未调用）
 * ============================================================================
 * 它的存在让"内核可以和人对话"：打印提示符 -> 读字符 -> 回显 -> 按行返回。
 * 依赖 cprintf（提示符）、getchar（取字符）、cputchar（回显），
 * 所以它同时也检验了 lab1 的输出链是通的。
 * ============================================================================
 */

#include <stdio.h>

/* 行缓冲区大小 1KB。*/
#define BUFSIZE 1024
/* static：只有本文件可见；又因为是全局数组，它会被放在 .bss 段，
 *   正好由 init.c 的 memset(edata, 0, end - edata) 负责清零。*/
static char buf[BUFSIZE];

/*
 * readline —— 从控制台读一行，返回内部的静态缓冲区指针。
 * 返回值是 static 缓冲区的地址，所以"下次调用会覆盖上次的结果"，
 * 需要保存时得自己拷贝一份（注释里特意提醒了这一点）。
 */
/* *
 * readline - get a line from stdin
 * @prompt:        the string to be written to stdout
 *
 * The readline() function will write the input string @prompt to
 * stdout first. If the @prompt is NULL or the empty string,
 * no prompt is issued.
 *
 * This function will keep on reading characters and saving them to buffer
 * 'buf' until '\n' or '\r' is encountered.
 *
 * Note that, if the length of string that will be read is longer than
 * buffer size, the end of string will be discarded.
 *
 * The readline() function returns the text of the line read. If some errors
 * are happened, NULL is returned. The return value is a global variable,
 * thus it should be copied before it is used.
 * */
char *readline(const char *prompt) {
/* prompt 为 NULL 或空串时不打印提示符。*/
    if (prompt != NULL) {
        cprintf("%s", prompt);
    }
/* i 是当前写入位置，c 是刚读到的字符。*/
    int i = 0, c;
    while (1) {
/* getchar 会一直等到有字符回来（轮询）。*/
        c = getchar();
/* 读到错误/EOF（负数）就返回 NULL。*/
        if (c < 0) {
            return NULL;
/* 可打印字符：先回显到屏幕，再存进缓冲区（i < BUFSIZE-1 留出 '\0'）。*/
        } else if (c >= ' ' && i < BUFSIZE - 1) {
            cputchar(c);
            buf[i++] = c;
/* 退格键：回显一个 '\b' 并把写入位置回退一格。*/
        } else if (c == '\b' && i > 0) {
            cputchar(c);
            i--;
/* 遇到换行或回车：补上字符串结束符并返回整行内容。*/
        } else if (c == '\n' || c == '\r') {
            cputchar(c);
            buf[i] = '\0';
            return buf;
        }
    }
}
