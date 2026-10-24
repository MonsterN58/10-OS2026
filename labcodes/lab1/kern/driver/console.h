/*
 * ============================================================================
 * kern/driver/console.h —— 控制台（console）抽象的接口声明
 * ============================================================================
 * 设计意图：上层（stdio.c、readline.c、printfmt.c）只依赖这一组接口，
 * 不关心字符到底是怎么到屏幕上的。lab1 里真正的实现只有一条：
 * cons_putc -> sbi_console_putchar -> ecall 进 OpenSBI。
 * 这种"层"的划分是答辩常问的问题：为什么要多一层 cons_putc？
 *   因为它把"设备驱动"和"格式化输出"解耦，lab2 以后换成真正的
 *   串口/键盘驱动时，上层代码一行都不用改。
 * ============================================================================
 */

#ifndef __KERN_DRIVER_CONSOLE_H__
#define __KERN_DRIVER_CONSOLE_H__

/* 初始化控制台设备。lab1 里是空函数：SBI 固件已经替我们初始化好了。*/
void cons_init(void);
/* 输出一个字符到控制台 —— lab1 唯一真正干活的接口。*/
void cons_putc(int c);
/* 从控制台读一个字符，没有输入时返回 0（不是 EOF）。*/
int cons_getc(void);
/* 串口中断处理：lab1 是空函数，中断相关实验（lab3）再填。*/
void serial_intr(void);
/* 键盘中断处理：lab1 是空函数。*/
void kbd_intr(void);

#endif /* !__KERN_DRIVER_CONSOLE_H__ */

