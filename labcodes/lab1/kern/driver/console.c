/*
 * ============================================================================
 * kern/driver/console.c —— 控制台设备层实现（一个"薄封装"）
 * ============================================================================
 * 本文件只有 cons_putc/cons_getc 有实际内容，其余是为了保持接口完整而
 * 留下的空实现。答辩要点：为什么这些函数是空的？
 *   因为 lab1 的运行环境（QEMU + OpenSBI）已经提供了字符级输入输出，
 *   真正的"驱动"工作（时钟、中断、键盘）属于后面的实验。
 * ============================================================================
 */
/* 包含 sbi.h 才能调用 sbi_console_putchar / sbi_console_getchar。*/
#include <sbi.h>
/* 包含自己的接口声明，保证函数定义与声明一致（编译器会检查）。*/
#include <console.h>

/* kbd_intr - try to feed input characters from keyboard */
/* 键盘中断：空实现。占位，lab3 讲中断时再实现。*/
void kbd_intr(void) {}

/* serial_intr - try to feed input characters from serial port */
/* 串口中断：空实现。早期 ucore 用 8250 串口中断喂字符，
 *   现在字符直接从 SBI 取，不需要中断。*/
void serial_intr(void) {}

/* cons_init - initializes the console devices */
/* 控制台初始化：空实现。
 *   如果以后要设置串口波特率、注册设备等，就在这里做。*/
void cons_init(void) {}

/* cons_putc - print a single character @c to console devices */
/* 【lab1 打印链的倒数第二环】cons_putc：
 *   把 int 截成 unsigned char（SBI 只认一个字节），转交给 sbi_console_putchar。
 *   注意形参是 int 而不是 char：这是 C 语言 I/O 接口的传统约定
 *   （传字符也用 int，便于将来用 EOF 等特殊值表示错误）。*/
void cons_putc(int c) { sbi_console_putchar((unsigned char)c); }

/* *
 * cons_getc - return the next input character from console,
 * or 0 if none waiting.
 * */
/* cons_getc：从控制台取一个字符，没有输入时 SBI 返回 0。
 *   这里的实现是"轮询"（polling），不是中断驱动，所以 getchar() 会忙等。*/
int cons_getc(void) {
    int c = 0;
/* 真正向 OpenSBI 要字符；lab1 里没用到，但 readline() 依赖它。*/
    c = sbi_console_getchar();
    return c;
}
