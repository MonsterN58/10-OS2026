/*
 * ============================================================================
 * kern/mm/memlayout.h —— 内存布局相关常量
 * ============================================================================
 * 本文件目前只描述内核栈的大小；后续实验会加入内核虚地址空间、
 * 物理内存起止地址等定义。lab1 里它和 mmu.h 一起，只为 entry.S 服务。
 * ============================================================================
 */

#ifndef __KERN_MM_MEMLAYOUT_H__
#define __KERN_MM_MEMLAYOUT_H__

/* 内核栈占 2 个页面。
 *   为什么不是 1 页？内核栈后面跟着"保护页"（guard page）的设计在
 *   后续实验（lab2 页表）里才实现，那时需要栈页后面留出未映射的页，
 *   栈溢出时触发缺页异常而不是默默踩坏别的数据。*/
#define KSTACKPAGE          2                           // # of pages in kernel stack
/* 内核栈总字节数 = KSTACKPAGE * PGSIZE = 2 * 4096 = 8192 = 8KB。
 *   entry.S 里 ".space KSTACKSIZE" 用的就是这个值。*/
#define KSTACKSIZE          (KSTACKPAGE * PGSIZE)       // sizeof kernel stack

#endif /* !__KERN_MM_MEMLAYOUT_H__ */

