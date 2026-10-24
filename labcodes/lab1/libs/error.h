/*
 * ============================================================================
 * libs/error.h —— 内核错误码
 * ============================================================================
 * printfmt.c 支持一种特殊的格式符 %e：把整数错误码翻译成字符串，
 * 它用的正是本文件里的这些宏（例如 E_NO_MEM -> "out of memory"）。
 * 内核函数习惯"返回 0 表示成功，返回负的错误码表示失败"。
 * ============================================================================
 */

#ifndef __LIBS_ERROR_H__
#define __LIBS_ERROR_H__

/* 注释提醒：这里的顺序要和 printfmt.c 里的 error_string[] 表保持一致。*/
/* kernel error codes -- keep in sync with list in lib/printfmt.c */
/* 1：未指明/未知的错误。*/
#define E_UNSPECIFIED        1    // Unspecified or unknown problem
/* 2：进程不存在（进程管理实验才用）。*/
#define E_BAD_PROC            2    // Process doesn't exist or otherwise
/* 3：参数非法，最常见的一个。*/
#define E_INVAL                3    // Invalid parameter
/* 4：内存不足（malloc 失败等）。*/
#define E_NO_MEM            4    // Request failed due to memory shortage
/* 5：进程数超限。*/
#define E_NO_FREE_PROC        5    // Attempt to create a new process beyond
/* 6：内存访问错误（段错误）。*/
#define E_FAULT                6    // Memory fault

/* the maximum allowed */
/* 最大错误码，用来界定 error_string[] 数组的大小。*/
#define MAXERROR            6

#endif /* !__LIBS_ERROR_H__ */

