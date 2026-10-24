/*
 * ============================================================================
 * libs/sbi.c —— 用内联汇编实现 SBI 调用（lab1 打印链的最底层，答辩重点）
 * ============================================================================
 * 【调用约定（背下来）】
 *   a7(x17) = 功能号（function ID）
 *   a0(x10) a1(x11) a2(x12) = 三个参数
 *   ecall 指令触发陷入 M 态，由 OpenSBI 处理，返回值放回 a0(x10)
 * 【为什么要内联汇编】
 *   C 语言无法直接执行 ecall，也无法指定"哪个变量放哪个寄存器"，
 *   只能手写汇编，并用约束（'r'、'=r'）告诉编译器怎么分配寄存器。
 * ============================================================================
 */
/* 包含 defs.h 是为了 uint64_t 这样的定长类型。*/
// libs/sbi.c
#include <sbi.h>
#include <defs.h>


/* 下面是 SBI 服务的功能号清单（历史遗留风格：用变量而不是宏）。
 *   值来自 OpenSBI/SBI 规范：0=设置定时器, 1=输出字符, 2=读入字符, ...*/
uint64_t SBI_SET_TIMER = 0;
/* 1 = 控制台输出一个字符，这是 lab1 唯一的"硬件出口"。*/
uint64_t SBI_CONSOLE_PUTCHAR = 1; 
/* 2 = 控制台读入一个字符。*/
uint64_t SBI_CONSOLE_GETCHAR = 2;
uint64_t SBI_CLEAR_IPI = 3;
uint64_t SBI_SEND_IPI = 4;
uint64_t SBI_REMOTE_FENCE_I = 5;
uint64_t SBI_REMOTE_SFENCE_VMA = 6;
uint64_t SBI_REMOTE_SFENCE_VMA_ASID = 7;
/* 8 = 关机。注意：lab1 里 kern_init 是死循环，不是调用关机，
 *   所以我们只能用 Ctrl-A X 强制结束 QEMU。*/
uint64_t SBI_SHUTDOWN = 8;

/* sbi_call：所有 SBI 服务的通用入口。
 *   @sbi_type：功能号（放进 a7）
 *   @arg0/1/2：三个参数（放进 a0/a1/a2）
 *   返回 OpenSBI 放在 a0 里的结果。*/
uint64_t sbi_call(uint64_t sbi_type, uint64_t arg0, uint64_t arg1, uint64_t arg2) {
    uint64_t ret_val;
/* __asm__ volatile 内联汇编；volatile 表示"这条汇编有副作用，
 *   不许编译器优化掉或挪动它"，对 ecall 这种有可观测行为的指令必须有。
 *   后面没有 ":" 的部分（第 18~24 行）都是汇编指令字符串，结尾的 \n
 *   用来分隔多条汇编语句。*/
    __asm__ volatile (
/* 把功能号送进 x17（也就是 a7）。*/
        "mv x17, %[sbi_type]\n"
/* 把第一个参数送进 x10（a0）。*/
        "mv x10, %[arg0]\n"
/* 第二个参数 -> x11（a1）。*/
        "mv x11, %[arg1]\n"
/* 第三个参数 -> x12（a2）。*/
        "mv x12, %[arg2]\n"
/* 【关键一条】ecall：发起环境调用，CPU 陷入 M 态，
 *   OpenSBI 按 a7 的功能号分发处理，处理完返回 S 态继续执行下一条。*/
        "ecall\n"
/* OpenSBI 的返回值在 x10（a0）里，搬到 C 变量 ret_val。
 *   注意这条必须写在 ecall 之后，且与 ecall 之间不能有别的指令覆盖 a0。*/
        "mv %[ret_val], x10"
/* 输出操作数：'=r' 表示只写、用通用寄存器存放，
 *   编译后 ret_val 的地址就是 %[ret_val] 的替换位置。*/
        : [ret_val] "=r" (ret_val)
/* 输入操作数：'r' 表示只读、放通用寄存器，
 *   把四个 C 变量分别绑定到汇编里的 [名字]。*/
        : [sbi_type] "r" (sbi_type), [arg0] "r" (arg0), [arg1] "r" (arg1), [arg2] "r" (arg2)
/* 破坏列表（clobber list）："memory" 告诉编译器这段汇编
 *   可能读写内存（还可能改变全局状态），别把内存访问优化到它外面。*/
        : "memory"
    );
    return ret_val;
}

/* 【打印链的最后一环】把一个字符交给 SBI 输出。
 *   功能号 1 = SBI_CONSOLE_PUTCHAR，后两个参数不用所以填 0。
 *   至此：cprintf -> ... -> cons_putc -> 这里 -> ecall -> 屏幕上出现字符。*/
void sbi_console_putchar(unsigned char ch) {
    sbi_call(SBI_CONSOLE_PUTCHAR, ch, 0, 0);
}

/* 设置下一次定时器中断时刻，lab3 会用到；lab1 未使用。*/
void sbi_set_timer(unsigned long long stime_value) {
    sbi_call(SBI_SET_TIMER, stime_value, 0, 0);
}
