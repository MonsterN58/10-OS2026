/*
 * ============================================================================
 * kern/init/entry.S  —— 内核的第一条指令所在处（答辩必讲）
 * ============================================================================
 * 【它在整个启动流程中的位置】
 *   加电复位 -> CPU 从 0x1000 的 MROM 起步 -> 跳到 0x80000000 的 OpenSBI
 *   -> OpenSBI 把内核镜像加载到 0x80200000 -> 跳到 0x80200000
 *   -> 【本文件 kern_entry】设置好栈 -> tail kern_init -> C 语言世界开始
 *
 * 【关键结论（练习1、练习2 的答案都藏在这里）】
 *   1. 为什么 0x80200000 上放的就是本文件？
 *      因为 tools/kernel.ld 把内核的加载地址定在 0x80200000，并且把
 *      .text.kern_entry 放在 .text 段的最前面，于是 kern_entry 恰好落在
 *      0x80200000。练习2 里的 "b *0x80200000" 就是在这条指令上打断点。
 *   2. 为什么用 .S 而不是 .c？
 *      这里要在 C 运行环境（栈）都还不存在的时候操作寄存器，只能是汇编；
 *      后缀是 .S 而不是 .s，表示需要先经过 C 预处理器（#include 就是证据）。
 * ============================================================================
 */

#include <mmu.h>
#include <memlayout.h>

/* 声明本文件的内容放进 .text 段。
 *   "ax" = allocatable + executable，即"可加载、可执行"，这是代码段的标配；
 *   %progbits 表示段内是程序数据（相对的是 @nobits，用于 .bss）。*/
    .section .text,"ax",%progbits
/* 把符号 kern_entry 导出为全局符号，供链接器和 gdb 看见。*/
    .globl kern_entry
/* 链接脚本把这段代码放在内核镜像最前面，所以这个标号就是 0x80200000。*/
kern_entry:
/* 【练习1 第一问】la sp, bootstacktop
 *   la = load address，伪指令，通常展开为 auipc + addi 两条指令；
 *   它把 bootstacktop 这个标号（内核栈的"顶"）的地址装入 sp 寄存器。
 *   - 为什么要算地址而不是写死某个数？因为编译时用 -mcmodel=medany，
 *     生成与位置无关的 PC 相对寻址，这样镜像被加载到哪里都能正确运行。
 *   - 为什么栈顶是 bootstacktop 而不是 bootstack？
 *     RISC-V 的栈是"从高地址向低地址生长"的，sp 必须指向栈区最高地址，
 *     压栈时 sp 减小。bootstack 是低地址起始处，bootstacktop 是高地址末尾。
 *   - 目的是什么？为 C 语言函数调用准备运行环境：没有合法 sp，
 *     后面的 kern_init() 里任何一条压栈/取参指令都会写坏内存。*/
    la sp, bootstacktop

/* 【练习1 第二问】tail kern_init
 *   tail 也是伪指令，展开为 "auipc t1, 偏移; jr t1"（或者直接 j 过去），
 *   语义是"尾调用"：跳转过去并且不保留返回地址。
 *   - 为什么不是 call/jal？因为 kern_init() 声明了 noreturn，永不返回，
 *     保存 ra 毫无意义；而且此时也没有人来接收它的返回值。
 *   - 目的：把控制权交给 C 语言写的内核初始化函数 kern_init()，
 *     从此进入 C 语言世界（栈已经准备好了）。
 *   - tail 而不是普通 j 的好处：复用了当前栈帧，不额外消耗栈空间。*/
    tail kern_init

/* 下面这块空间放在 .data 段中（不是 .bss），
 *   因为 .bss 需要在 C 代码里显式清零，而栈在使用前就会被写入，不必清零。*/
.section .data
    # .align 2^12
/* 把当前地址按 PGSHIFT（=12）字节对齐，也就是按 4KB 页面边界对齐。
 *   PGSHIFT 来自 kern/mm/mmu.h，值 12 相当于 2^12 = 4096 = 一页。
 *   对齐栈的目的：一是让栈起点落在页边界上，二是 lab2 之后要为栈设置
 *   页表映射时，按页对齐才方便。*/
    .align PGSHIFT
    .global bootstack
/* bootstack 是内核栈的起始（最低）地址标号，
 *   预留 KSTACKSIZE 字节的空间。KSTACKSIZE 来自 kern/mm/memlayout.h，
 *   = KSTACKPAGE(2) * PGSIZE(4096) = 8KB，也就是连续两页。*/
bootstack:
    .space KSTACKSIZE
/* bootstacktop 是内核栈的末尾（最高）地址标号，紧贴在预留空间的后面。
 *   注意：它本身不占空间，只是个位置标记，sp 将指向它。
 *   顺序也很重要：先 .space 再 bootstacktop，才能保证 top 高于 stack。*/
    .global bootstacktop
bootstacktop: