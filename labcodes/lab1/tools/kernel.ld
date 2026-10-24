/*
 * ============================================================================
 * tools/kernel.ld —— 内核链接脚本（lab1 答辩必讲：内核"为什么在 0x80200000"）
 * ============================================================================
 * 【链接脚本是干什么的】
 *   编译只把每个 .c/.S 变成独立的 .o；.o 里的段（如 .text）地址都还没定。
 *   链接脚本（linker script）的任务就是告诉链接器 ld：
 *     1) 把各输入文件的同名段拼成输出文件的段（.text/.rodata/.data/.bss ...）；
 *     2) 每个输出段放在什么地址上（内存布局）；
 *     3) 程序的入口符号是谁。
 *   ld 默认的脚本是给"跑在 Linux 上的应用程序"用的，不适合内核，
 *   所以我们要自己写一个（这也是本文件存在的原因）。
 * 【本实验最关键的一句话】
 *   BASE_ADDRESS = 0x80200000，且 .text 段以 .text.kern_entry 打头。
 *   于是 entry.S 里的 kern_entry 恰好落在物理地址 0x80200000 ——
 *   这正是 QEMU 里 OpenSBI 把内核镜像加载到的地址（见 Makefile 的
 *   -device loader,file=bin/ucore.img,addr=0x80200000）。
 *   地址必须严丝合缝：OpenSBI 加载完就跳到 0x80200000，这里必须是第一条指令。
 * 【完整启动链条（答辩时可以顺着讲）】
 *   加电 → PC=0x1000（MROM）→ 0x80000000（OpenSBI）→ 把内核放到 0x80200000
 *   → 跳到 0x80200000 执行 kern_entry → 建立栈 → tail kern_init
 * ============================================================================
 */
/* Simple linker script for the ucore kernel.
   See the GNU ld 'info' manual ("info ld") to learn the syntax. */

/* 指定输出文件的指令集架构：RISC-V。*/
OUTPUT_ARCH(riscv)
/* ENTRY(kern_entry)：把 kern_entry 记为 ELF 的入口点。
 *   注意：这只是"告诉链接器/调试器"入口在哪（显示在 ELF header 里），
 *   真正让 CPU 跑到这里的其实是 OpenSBI 直接跳到 0x80200000。
 *   kern_entry 这个符号由 kern/init/entry.S 定义。*/
ENTRY(kern_entry)

/* 【答辩重点】内核的链接（加载）基地址 = 0x80200000。
 *   为什么不是 0x80000000？因为 0x80000000 被 OpenSBI 固件自己占了，
 *   内核必须紧跟在固件之后，这是 QEMU 与 OpenSBI 约定的布局。*/
BASE_ADDRESS = 0x80200000;

/* SECTIONS：后面一整块都在描述"输出文件各段的内存布局"。*/
SECTIONS
{
    /* Load the kernel at this address: "." means the current address */
/* "." 是链接脚本里的"位置计数器"，代表当前地址。
 *   这一句把计数器设成 BASE_ADDRESS，后面所有段就从 0x80200000 开始排。*/
    . = BASE_ADDRESS;

/* .text：代码段。注意花括号里各输入段是有顺序的 —— 谁写在前面谁先放。*/
    .text : {
/* 第一个是 .text.kern_entry：entry.S 里用 .section .text.kern_entry 指定，
 *   专门保证 kern_entry 是整个内核的第一条指令（正好落在 0x80200000）。
 *   后面的 .text / .stub / .text.xxx / .gnu.linkonce.t.xxx 就是普通代码段和编译器
 *   为"每个函数一个段"（-ffunction-sections）生成的众多小段。*/
        *(.text.kern_entry .text .stub .text.* .gnu.linkonce.t.*)
    }

/* PROVIDE(etext = .)：定义符号 etext = 代码段结束地址。
 *   PROVIDE 的含义：只有别处没定义过才定义它。内核 C 代码可以像用变量
 *   一样用这个符号（例如调试/统计用）。*/
    PROVIDE(etext = .); /* Define the 'etext' symbol to this value */

/* .rodata：只读数据段，放常量（字符串、const 数组等）。*/
    .rodata : {
        *(.rodata .rodata.* .gnu.linkonce.r.*)
    }

/* 把位置计数器对齐到 0x1000（4KiB，一页）的整数倍。
 *   为什么在这对齐？让数据段从页边界开始，后面 lab2 做分页（按页映射）时
 *   可以直接整页映射，不用拆开一个页。*/
    /* Adjust the address for the data segment to the next page */
    . = ALIGN(0x1000);

/* .data：已初始化的可读写全局变量。*/
    /* The data segment */
/* 注意这里是 .data 和 .sdata 两个段分开写，合起来才是数据段。*/
    .data : {
        *(.data)
        *(.data.*)
    }

/* .sdata：small data 段，存放较小的全局变量（如 char/short/指针），
 *   编译器生成它是因为它可以用一条指令直接寻址（gp 相对寻址）。*/
    .sdata : {
        *(.sdata)
        *(.sdata.*)
    }

/* PROVIDE(edata = .)：数据段结束符号。
 *   ★【重要】kern/init/init.c 清 BSS 时用的就是 edata 和 end：
 *     memset(edata, 0, end - edata)
 *   即"从数据段结束处一直清到镜像结束"，正好覆盖整个 .bss。*/
    PROVIDE(edata = .);

/* .bss：未初始化（或初始化为 0）的全局变量段。
 *   它在可执行文件里不占空间（只记录大小），由启动代码负责清零（见上）。*/
    .bss : {
        *(.bss)
        *(.bss.*)
        *(.sbss*)
    }

/* PROVIDE(end = .)：整个内核镜像的结束地址。
 *   它同时被 init.c 用来清 BSS，也被 lab2 之后用来算"内核占了多少内存"。*/
    PROVIDE(end = .);

/* /DISCARD/：这些段直接丢弃，不写进最终内核。
 *   .eh_frame 是 C++/异常处理用的、.note.GNU-stack 只是标记位，
 *   对内核毫无用处，丢掉可以让镜像更小。*/
    /DISCARD/ : {
        *(.eh_frame .note.GNU-stack)
    }
}
