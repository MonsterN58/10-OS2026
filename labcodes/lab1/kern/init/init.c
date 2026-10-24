/*
 * ============================================================================
 * kern/init/init.c —— C 语言的内核入口（答辩必讲）
 * ============================================================================
 * 执行流：entry.S 的 kern_entry 设置好栈后 tail 到这里，
 *         kern_init() 做两件事：清空 BSS 段 + 打印一行启动信息，然后死循环。
 * 输出的那句 "(THU.CST) os is loading ..." 就是 lab1 成功运行的标志。
 * ============================================================================
 */
/* stdio.h：这里的 cprintf 不是 glibc 的 printf，而是 ucore 自己实现的
 *   （libs/printfmt.c + kern/libs/stdio.c），声明在本项目中。*/

#include <stdio.h>
/* string.h：同样是自己实现的 string.c，提供 memset。*/
#include <string.h>
/* sbi.h：OpenSBI 服务接口，作为打印字符的"最底层出口"。*/
#include <sbi.h>
/* 把 kern_init 声明为 noreturn（永不返回）。
 *   这解释了入口汇编为什么敢用 tail 而不是 call：函数不回来，不必存 ra。
 *   属性写在声明上，编译器就不会抱怨 while(1) 之后没有 return。*/
int kern_init(void) __attribute__((noreturn));

/* kern_init 是 C 语言意义上的"内核入口点"。
 *   注意它没有参数，因为它不接收任何来自固件的信息（不符合规范但足够 lab1 用）。*/
int kern_init(void) {
/* 这两个符号不是变量，而是链接脚本用 PROVIDE 定义的"位置标号"：
 *   edata = 已初始化数据段的结束地址（.data/.sdata 之后），
 *   end   = 整个内核镜像数据部分的结束地址（.bss 之后），
 *   所以 edata ~ end 这一段正好就是 .bss 段的范围。
 *   声明成 extern char[] 是为了能直接做指针减法，而且不为它分配空间。*/
    extern char edata[], end[];
/* 【本文件最重要的两行之一】把 BSS 段清零。
 *   为什么要清？C 语言规定未初始化的全局/静态变量初值为 0，
 *   但 .bss 段为了减小镜像体积，在文件里并不保存实际内容
 *   （objcopy 成 ucore.img 时更明显）。内核镜像由 OpenSBI 直接搬到内存，
 *   谁也不会替我们清零，所以必须自己动手：memset(起点, 0, 长度)。
 *   end - edata 的计算依赖链接脚本里各段的顺序；如果顺序错了，
 *   这里就会清掉别的数据（答辩常问的"为什么不能乱改链接脚本"）。*/
    memset(edata, 0, end - edata);

/* 内核启动的打印信息，字符串放在 .rodata 段。*/
    const char *message = "(THU.CST) os is loading ...\n";
/* 调用 ucore 自己实现的格式化输出：cprintf -> vcprintf -> vprintfmt
 *   -> cputch -> cons_putc -> sbi_console_putchar -> ecall，
 *   这条调用链就是 lab1 的核心，答辩时最好背下来。*/
    cprintf("%s\n\n", message);
/* 内核绝不退出：既没有"返回上一层"可去，也没有操作系统可以退出。
 *   QEMU 里想停下就按 Ctrl-A 再按 X（或用 make debug + gdb 的 quit）。*/
   while (1)
        ;
}
