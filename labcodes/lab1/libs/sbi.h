/*
 * ============================================================================
 * libs/sbi.h —— OpenSBI（M 态固件）服务接口的声明
 * ============================================================================
 * 背景：我们的内核运行在 S 态（监管态），而控制台、定时器等最底层服务
 * 由运行在 M 态（机器态）的 OpenSBI 提供。跨特权级不能直接 call，
 * 必须用 ecall 指令"请求"，这套请求-应答的规则就叫 SBI。
 * 头文件只做声明，真正的实现（内联汇编发 ecall）在 sbi.c 里。
 * 说明：本文件里声明的大部分接口是历史遗留（ucore 的 x86 时代），
 * lab1 真正用到的只有 sbi_console_putchar（以及 getchar）。
 * ============================================================================
 */

#ifndef _ASM_RISCV_SBI_H
#define _ASM_RISCV_SBI_H

/* 描述一块物理内存的起始地址、大小和 NUMA 节点号。
 *   纯属历史遗留：RISC-V 下内存信息由设备树（DTB）提供，不用这个结构。*/
typedef struct {
  unsigned long base;
  unsigned long size;
  unsigned long node_id;
} memory_block_info;

/* 查询内存块信息。历史遗留，lab1 未使用。*/
unsigned long sbi_query_memory(unsigned long id, memory_block_info *p);

/* 设置下一次时钟中断的时间点（stime_value）。lab3 的时钟中断会用到。*/
void sbi_set_timer(unsigned long long stime_value);
/* 向另一个 hart（核心）发送处理器间中断 IPI。多核实验才用。*/
void sbi_send_ipi(unsigned long hart_id);
/* 清除挂起的 IPI。*/
unsigned long sbi_clear_ipi(void);
/* 关机：让 QEMU 退出（SBI_SHUTDOWN 功能号 8）。*/
void sbi_shutdown(void);

/* 【lab1 的核心出口】输出一个字符到控制台。*/
void sbi_console_putchar(unsigned char ch);
/* 从控制台读一个字符，无输入返回 0。*/
int sbi_console_getchar(void);

#endif
