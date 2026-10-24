/*
 * ============================================================================
 * libs/riscv.h —— RISC-V 架构的"字典"（答辩必问：这个文件到底是什么）
 * ============================================================================
 * 【来源与结构】本文件是从 RISC-V 官方 / OpenSBI 配套的 riscv.h 裁剪而来的。
 *   第 1  ~ 243 行：人工整理、与操作系统紧密相关的定义（要重点理解）；
 *   第 244 行往后：由 parse-opcodes 工具【自动生成】的编码表
 *                  （指令 MATCH_/MASK_、CSR 编号、异常号 CAUSE_），
 *                  属于"查表用"的资料，看懂格式即可，不必逐条记忆。
 *【五大块内容】
 *   一、按 32/64 位区分的基础宏（LOAD/STORE/REGBYTES…）
 *   二、CSR（控制状态寄存器）的位域定义：mstatus/sstatus/mip/mie 的每个
 *       位叫什么、在第几位（写内核时"置位/清位"全靠这些宏）
 *   三、页表项 PTE 的各个标志位（lab2 物理内存和页表实验的主角）
 *   四、本项目真正要用起来的一套封装：
 *       read_csr / write_csr / set_csr / clear_csr / swap_csr（读写 CSR 的内联汇编）
 *       do_div（除法取余）、barrier（内存屏障）、lcr3（写页表基址寄存器）
 *   五、自动生成的编码表：MATCH_xxx / MASK_xxx（指令编码）、CSR_*（CSR 编号）、
 *       CAUSE_*（异常/中断原因号）
 *【lab1 里的实际使用情况（答辩要点）】
 *   lab1 编译时本文件被 printfmt.c 包含，但真正"用到"的只有 do_div 这一个宏
 *   （printnum 打印数字时用它做除法）。
 *   其他的 read_csr/write_csr/PTE/lcr3 等等，是给后续实验铺路的：
 *     lab2 用 PTE_* 和 lcr3（sptbr/satp）建立页表；
 *     lab3 用 read_csr/write_csr 操作 sstatus/stvec/sepc，用 IRQ_STIP 处理时钟中断。
 *   这也正是实验手册所说"printfmt.c 依赖 riscv.h，但我们还没用到它的强大功能"。
 *【为什么加进来不会让内核变大】
 *   本文件全是头文件里的宏和少数 static inline 函数：宏只在被使用处展开，
 *   没被使用的定义不产生任何代码（Makefile 里的 --gc-sections 还会再丢掉
 *   没人引用的函数段）。
 * ============================================================================
 */

// See LICENSE for license details.

#ifndef __LIBS_RISCV_H__
#define __LIBS_RISCV_H__

/* 下面按"机器字长"区分：64 位（rv64）和 32 位（rv32）要用不同的指令。
 *   我们的实验是 64 位（riscv64-unknown-elf-gcc，-m elf64lriscv），
 *   所以走的是上面这一支；下面的 #else 分支是为 32 位准备的，读的时候可以跳过。*/
#if __riscv_xlen == 64
# define SLL32    sllw   // 64 位：32 位移位指令加 w 后缀
# define STORE    sd   // 64 位：store doubleword（8 字节存）
# define LOAD     ld   // 64 位：load doubleword（8 字节取）
# define LWU      lwu   // 64 位：load word unsigned（32 位无符号取）
/* LOG_REGBYTES：一个寄存器占多少字节的"对数"，8 字节 -> 3（因为 2^3 = 8）。
 *   之所以存对数，是为了后面用 1 << 3 直接算出字节数，也便于做移位运算。*/
# define LOG_REGBYTES 3   // 寄存器字节数的对数：2^3 = 8 字节
#else
# define SLL32    sll   // 32 位版本
# define STORE    sw   // 32 位：4 字节存
# define LOAD     lw   // 32 位：4 字节取
# define LWU      lw   // 32 位没有 lwu，只能用 lw
# define LOG_REGBYTES 2   // 32 位：2^2 = 4 字节
#endif
/* REGBYTES：一个寄存器/指针的字节数，64 位下是 8。
 *   保存/恢复上下文（lab3 的 trapentry.S）时要按这个数值移动栈指针，
 *   写死了 8 就无法移植，所以用宏算出来。*/
#define REGBYTES (1 << LOG_REGBYTES)   // 一个寄存器/指针的字节数（64 位下为 8）

/* ------------------------------------------------------------------
 * 一、mstatus（机器态状态寄存器）的各个位。
 *    mstatus 是 M 态最重要的状态寄存器，记录"中断是否使能、上一次陷入
 *    是从哪个特权级来的、浮点单元是否被使用"等信息。
 *    OpenSBI 用它维护运行环境；我们的内核用不到 mstatus 本身，
 *    但 sstatus 与它一一对应（S 态可见的那部分），所以对照着看很有价值。
 *    ------------------------------------------------------------------ */
#define MSTATUS_UIE         0x00000001  // 用户态中断使能
#define MSTATUS_SIE         0x00000002  // 管理态（S）中断使能：内核的总中断开关之一
#define MSTATUS_HIE         0x00000004  // 虚拟态（H）中断使能
#define MSTATUS_MIE         0x00000008  // 机器态（M）中断使能：M 态总开关（属于 OpenSBI）
#define MSTATUS_UPIE        0x00000010  // 陷入 U 态前的中断使能快照
#define MSTATUS_SPIE        0x00000020  // 陷入 S 态前的中断使能快照（sret 后恢复）
#define MSTATUS_HPIE        0x00000040  // 陷入 H 态前的中断使能快照
#define MSTATUS_MPIE        0x00000080  // 陷入 M 态前的中断使能快照（mret 后恢复）
#define MSTATUS_SPP         0x00000100  // 上一次陷入来自哪个特权级（sret 返回的去向）
#define MSTATUS_HPP         0x00000600  // H 态的对应字段（2 位）
#define MSTATUS_MPP         0x00001800  // 上一次陷入来自哪个特权级（mret 返回的去向，2 位）
#define MSTATUS_FS          0x00006000  // 浮点单元状态（0=未使用，3=已使用）
#define MSTATUS_XS          0x00018000  // 其它扩展单元状态
#define MSTATUS_MPRV        0x00020000  // 让 mret 之后按 MPP 指定的特权级访存
#define MSTATUS_PUM         0x00040000  // 禁止 S 态访问 U 态内存（安全加固）
#define MSTATUS_MXR         0x00080000  // 允许把"可执行"页当数据读（executable page readable）
#define MSTATUS_VM          0x1F000000  // 虚拟化模式（H 扩展，本实验不用）
#define MSTATUS32_SD        0x80000000  // 32 位版本的"有脏状态"汇总位
#define MSTATUS64_SD        0x8000000000000000  // 64 位版本的"有脏状态"汇总位

/* ------------------------------------------------------------------
 * 二、sstatus（管理态状态寄存器）：mstatus 在 S 态的"视图"。
 *    这是我们的内核真正会读写的寄存器：
 *      SIE 位     -> 总中断开关（lab3 打开时钟中断时置位）
 *      SPIE/SPP   -> 陷入发生前的使能状态与特权级（用来正确返回）
 *    ------------------------------------------------------------------ */
#define SSTATUS_UIE         0x00000001  // 用户态中断使能
#define SSTATUS_SIE         0x00000002  // S 态中断使能：lab3 打开时钟中断时用的就是这一位
#define SSTATUS_UPIE        0x00000010  // 陷入 U 态前的使能快照
#define SSTATUS_SPIE        0x00000020  // 陷入 S 态前的使能快照（sret 之后恢复到 SIE）
#define SSTATUS_SPP         0x00000100  // 上一次陷入来自哪个特权级（sret 的返回去向）
#define SSTATUS_FS          0x00006000  // 浮点单元状态
#define SSTATUS_XS          0x00018000  // 扩展单元状态
#define SSTATUS_PUM         0x00040000  // 禁止 S 态访问 U 态内存
#define SSTATUS32_SD        0x80000000  // 32 位"有脏状态"汇总位
#define SSTATUS64_SD        0x8000000000000000  // 64 位"有脏状态"汇总位

/* ------------------------------------------------------------------
 * 三、DCSR（调试控制状态寄存器）与 MCONTROL（硬件断点控制寄存器）。
 *    这是"调试"这个功能专用的寄存器：gdb 单步（STEP）、硬件断点（EBREAKM
 *    表示在 M 态遇到 ebreak 时进入调试模式）等等。
 *    lab1 的练习2 要用 gdb 单步跟踪加电启动过程，靠的正是 QEMU 实现的这套
 *    调试机制；但我们自己写内核时不需要碰这些寄存器。
 *    ------------------------------------------------------------------ */
#define DCSR_XDEBUGVER      (3U<<30)
#define DCSR_NDRESET        (1<<29)  // 调试器版本号
#define DCSR_FULLRESET      (1<<28)  // 未见过的复位（ndmreset 触发）
#define DCSR_EBREAKM        (1<<15)  // 完全复位
#define DCSR_EBREAKH        (1<<14)  // 在 M 态遇到 ebreak 进入调试模式
#define DCSR_EBREAKS        (1<<13)  // 在 H 态遇到 ebreak 进入调试模式
#define DCSR_EBREAKU        (1<<12)  // 在 S 态遇到 ebreak 进入调试模式
#define DCSR_STOPCYCLE      (1<<10)  // 在 U 态遇到 ebreak 进入调试模式
#define DCSR_STOPTIME       (1<<9)  // 单步时暂停周期计数
#define DCSR_CAUSE          (7<<6)  // 单步时暂停时间计数
#define DCSR_DEBUGINT       (1<<5)  // 进入调试模式的原因
#define DCSR_HALT           (1<<3)  // 调试中断使能
#define DCSR_STEP           (1<<2)  // 使 hart 停住（halt）
#define DCSR_PRV            (3<<0)  // 单步标志
  // 当前特权级
#define DCSR_CAUSE_NONE     0
#define DCSR_CAUSE_SWBP     1  // 原因：无
#define DCSR_CAUSE_HWBP     2  // 原因：软件断点
#define DCSR_CAUSE_DEBUGINT 3  // 原因：硬件断点
#define DCSR_CAUSE_STEP     4  // 原因：调试中断
#define DCSR_CAUSE_HALT     5  // 原因：单步
  // 原因：halt 请求
/* MCONTROL：硬件断点（watchpoint/breakpoint）的配置位。
 *   MCONTROL_TYPE(xlen) 这样的写法是为兼容 32/64 位：断点类型字段的位置
 *   与字长有关，所以用 xlen 当参数算出来。
 *   练习2 里用 watch *0x80200000 观察内核被加载的瞬间，底层就是这类机制。*/
#define MCONTROL_TYPE(xlen)    (0xfULL<<((xlen)-4))
#define MCONTROL_DMODE(xlen)   (1ULL<<((xlen)-5))  // 断点类型字段（位置与 xlen 相关）
#define MCONTROL_MASKMAX(xlen) (0x3fULL<<((xlen)-11))  // DMODE：该断点由调试模式使用
  // 掩码最大长度
#define MCONTROL_SELECT     (1<<19)  // 选择：不用地址匹配，改用下一位的 M/S/U 选择
#define MCONTROL_TIMING     (1<<18)  // 匹配时机（执行前/后）
#define MCONTROL_ACTION     (0x3f<<12)  // 命中后的动作
#define MCONTROL_CHAIN      (1<<11)  // 与下一个断点组成链
#define MCONTROL_MATCH      (0xf<<7)  // 地址匹配方式
#define MCONTROL_M          (1<<6)  // 在 M 态生效
#define MCONTROL_H          (1<<5)  // 在 H 态生效
#define MCONTROL_S          (1<<4)  // 在 S 态生效
#define MCONTROL_U          (1<<3)  // 在 U 态生效
#define MCONTROL_EXECUTE    (1<<2)  // 触发条件：取指
#define MCONTROL_STORE      (1<<1)  // 触发条件：写
#define MCONTROL_LOAD       (1<<0)  // 触发条件：读

#define MCONTROL_TYPE_NONE      0  // 类型：无
#define MCONTROL_TYPE_MATCH     2  // 类型：地址匹配

#define MCONTROL_ACTION_DEBUG_EXCEPTION   0  // 动作：产生调试异常
#define MCONTROL_ACTION_DEBUG_MODE        1  // 动作：进入调试模式
#define MCONTROL_ACTION_TRACE_START       2  // 动作：开始跟踪
#define MCONTROL_ACTION_TRACE_STOP        3  // 动作：停止跟踪
#define MCONTROL_ACTION_TRACE_EMIT        4  // 动作：输出跟踪数据

#define MCONTROL_MATCH_EQUAL     0  // 匹配：相等
#define MCONTROL_MATCH_NAPOT     1  // 匹配：NAPOT（自然对齐的 2 的幂范围）
#define MCONTROL_MATCH_GE        2  // 匹配：大于等于
#define MCONTROL_MATCH_LT        3  // 匹配：小于
#define MCONTROL_MATCH_MASK_LOW  4  // 匹配：低位掩码
#define MCONTROL_MATCH_MASK_HIGH 5  // 匹配：高位掩码

/* ------------------------------------------------------------------
 * 四、中断挂起位（MIP = M 态中断挂起；SIP = S 态可见的那部分）。
 *    每一位对应一种中断源："置位 = 有中断在等待处理"。
 *    命名规律：S/H/M = 目标特权级，S/T/E = 软件/时钟/外部三种类型。
 *      MIP_STIP  S 态时钟中断挂起 -> lab3 定时器中断
 *      MIP_SSIP  S 态软件中断挂起 -> 处理器间中断（多核实验）
 *      MIP_SEIP  S 态外部中断挂起 -> 设备（键盘等）
 *    另外注意下面 SIP_xxx 是 MIP_xxx 的别名：同一批位，S 态只能看到自己那份。
 *    ------------------------------------------------------------------ */
#define MIP_SSIP            (1 << IRQ_S_SOFT)  // S 态软件中断挂起（处理器间中断）
#define MIP_HSIP            (1 << IRQ_H_SOFT)  // H 态软件中断挂起
#define MIP_MSIP            (1 << IRQ_M_SOFT)  // M 态软件中断挂起
#define MIP_STIP            (1 << IRQ_S_TIMER)  // S 态时钟中断挂起：lab3 定时器中断看的就是这一位
#define MIP_HTIP            (1 << IRQ_H_TIMER)  // H 态时钟中断挂起
#define MIP_MTIP            (1 << IRQ_M_TIMER)  // M 态时钟中断挂起
#define MIP_SEIP            (1 << IRQ_S_EXT)  // S 态外部中断挂起（键盘等设备）
#define MIP_HEIP            (1 << IRQ_H_EXT)  // H 态外部中断挂起
#define MIP_MEIP            (1 << IRQ_M_EXT)  // M 态外部中断挂起

/* S 态可见的别名。写 S 模式代码时用 SIP_xxx 更符合直觉。*/
#define SIP_SSIP MIP_SSIP  // 与 MIP_SSIP 同一位，S 态视角的名字
#define SIP_STIP MIP_STIP  // 与 MIP_STIP 同一位，S 态视角的名字

/* ------------------------------------------------------------------
 * 五、特权级编码（priv: privilege）。
 *    RISC-V 一共 4 个特权级，数值越大权限越高：
 *      0 = U 用户态     （应用程序）
 *      1 = S 管理态     （我们的内核运行在这里）
 *      2 = H 虚拟态     （Hypervisor，本实验不涉及）
 *      3 = M 机器态     （OpenSBI 固件，最高权限，不可修改）
 *    一处贯穿全实验的重要认知：我们写的是 S 态内核，所以想输出一个字符、
 *    想设置定时器，都必须通过 ecall 请 M 态的 OpenSBI 代劳。
 *    ------------------------------------------------------------------ */
#define PRV_U 0  // 0 = 用户态 U
#define PRV_S 1  // 1 = 管理态 S（我们的内核运行在这里）
#define PRV_H 2  // 2 = 虚拟态 H（本实验不涉及）
#define PRV_M 3  // 3 = 机器态 M（OpenSBI 固件，不可修改）

/* ------------------------------------------------------------------
 * 六、地址翻译模式（VM）与页表层级。
 *    MBARE   = Bare，不做地址翻译（"裸机"模式，取到的地址即物理地址）
 *    SV32/39/48 = 页式地址翻译，数字表示虚拟地址的有效位数：
 *               SV39 = 三级页表、39 位虚拟地址（lab2 要实现的正是它）
 *    目前（lab1）内核处于 Bare 模式：没有页表，0x80200000 既是虚拟地址
 *    也是物理地址；lab2 打开分页后，"看到的地址"和"实际地址"才分开。
 *    ------------------------------------------------------------------ */
#define VM_MBARE 0  // 0 = 不做地址翻译（裸机模式，lab1 就是这个状态）
#define VM_MBB   1  // 1 = MBB：按 M 态页表翻译
#define VM_MBBID 2  // 2 = MBBID：MBB + 地址空间标识
#define VM_SV32  8  // 8 = SV32：32 位页式翻译（两级页表）
#define VM_SV39  9  // 9 = SV39：39 位虚拟地址、三级页表（lab2 要实现的目标）
#define VM_SV48  10  // 10 = SV48：48 位虚拟地址（四级页表）

/* ------------------------------------------------------------------
 * 七、中断/异常原因号（IRQ_*，将来填入 scause/mcause 的值）。
 *    编号规律（RISC-V 规范）：
 *      0~3   软件中断      4~7   定时器中断
 *      8~11  外部中断      12~13 协处理器/主机
 *    每类里的四个数字再按 U/S/H/M 递增。说清楚"中断号是有规律的表"，
 *    比死记一个个宏更划算。
 *    ------------------------------------------------------------------ */
#define IRQ_U_SOFT   0  // 0 = U 态软件中断号
#define IRQ_S_SOFT   1  // 1 = S 态软件中断号
#define IRQ_H_SOFT   2  // 2 = H 态软件中断号
#define IRQ_M_SOFT   3  // 3 = M 态软件中断号
#define IRQ_U_TIMER  4  // 4 = U 态定时器中断号
#define IRQ_S_TIMER  5  // 5 = S 态定时器中断号（lab3 时钟中断用这个号）
#define IRQ_H_TIMER  6  // 6 = H 态定时器中断号
#define IRQ_M_TIMER  7  // 7 = M 态定时器中断号
#define IRQ_U_EXT    8  // 8 = U 态外部中断号
#define IRQ_S_EXT    9  // 9 = S 态外部中断号（设备中断用这个号）
#define IRQ_H_EXT    10  // 10 = H 态外部中断号
#define IRQ_M_EXT    11  // 11 = M 态外部中断号
#define IRQ_COP      12  // 12 = 协处理器不可用
#define IRQ_HOST     13  // 13 = 主机（hypervisor）相关

/* ------------------------------------------------------------------
 * 八、与"机器启动"直接相关的常量（【练习2 的答案就在这里】）。
 *    DEFAULT_RSTVEC = 0x1000：CPU 复位向量。RISC-V 硬件加电后，PC 被强制
 *      设为 0x1000，从这里开始执行固化在 MROM 里的最早期代码 —— 这就是
 *      "RISC-V 加电后最初执行的几条指令位于什么地址"的标准答案。
 *    DRAM_BASE = 0x80000000：DRAM 起始地址，OpenSBI 固件就被加载在这里；
 *      我们内核的加载地址 0x80200000 是它往后的 2MB 处（见 kernel.ld）。
 *    EXT_IO_BASE = 0x40000000：QEMU virt 机器外设（MMIO）的映射起始处。
 *    ------------------------------------------------------------------ */
#define DEFAULT_RSTVEC     0x00001000  // ★ 复位向量 0x1000：CPU 加电后 PC 的初值（练习2 的答案）
#define DEFAULT_NMIVEC     0x00001004  // 不可屏蔽中断入口 0x1004
#define DEFAULT_MTVEC      0x00001010  // 复位后默认的 M 态 trap 入口 0x1010
#define CONFIG_STRING_ADDR 0x0000100C  // 设备树里"配置字符串"的地址
#define EXT_IO_BASE        0x40000000  // 外设 MMIO 起始 0x40000000（QEMU virt 机器）
#define DRAM_BASE          0x80000000  // DRAM 起始 0x80000000：OpenSBI 被加载在这里

// page table entry (PTE) fields
/* ------------------------------------------------------------------
 * 九、页表项（PTE, Page Table Entry）的标志位 —— lab2 的核心。
 *    一个 PTE 是 8 字节：低 10 位是权限标志，高 44 位是物理页号。
 *      V 有效（必须为 1，否则整个 PTE 无效）
 *      R/W/X 可读/可写/可执行（三者全 0 的 PTE 表示"指向下一级页表"）
 *      U 允许用户态访问   G 全局映射（切换页表时 TLB 不必刷新）
 *      A 已访问           D 已脏（被写过）；缺这两个位硬件会触发缺页异常
 *    PTE_PPN_SHIFT = 10：物理页号从第 10 位开始存放。
 *    PTE_TABLE() 判断"这个 PTE 是否指向下一级页表"：只有 V 位被置起，
 *    没有 R/W/X，正是 RISC-V 规范规定的"非叶子 PTE"特征。
 *    ------------------------------------------------------------------ */
#define PTE_V     0x001 // Valid  // 有效位：为 0 则整个页表项无效
#define PTE_R     0x002 // Read  // 可读
#define PTE_W     0x004 // Write  // 可写
#define PTE_X     0x008 // Execute  // 可执行（R/W/X 全 0 表示该 PTE 指向下一级页表）
#define PTE_U     0x010 // User  // 允许用户态（U 态）访问
#define PTE_G     0x020 // Global  // 全局映射：切换页表时不必刷新对应的 TLB 项
#define PTE_A     0x040 // Accessed  // 已访问（硬件会在第一次访问时置位）
#define PTE_D     0x080 // Dirty  // 已脏（被写过就置位）
/* PTE_SOFT：两位保留给软件使用的位，内核可以用它记自定义信息。*/
#define PTE_SOFT  0x300 // Reserved for Software

/* PTE 里物理页号的起始位。取物理地址时用 (pte >> 10) << 12 得到页框基址。*/
#define PTE_PPN_SHIFT 10

#define PTE_TABLE(PTE) (((PTE) & (PTE_V | PTE_R | PTE_W | PTE_X)) == PTE_V)

/* ------------------------------------------------------------------
 * 十、只有定义了 __riscv 宏（即用 RISC-V 工具链编译）时才展开下面这些。
 *    也就是说：本文件是"可移植裁剪"过的，非 RISC-V 编译器只会看到上面
 *    那些常量定义。
 *    ------------------------------------------------------------------ */
#ifdef __riscv

#if __riscv_xlen == 64
# define MSTATUS_SD MSTATUS64_SD  // 64 位版本的 SD 位别名
# define SSTATUS_SD SSTATUS64_SD  // 64 位版本的 SD 位别名
# define RISCV_PGLEVEL_BITS 9  // 64 位：三级页表（每级 9 位 + 12 位页内偏移 = 39 位）
#else
# define MSTATUS_SD MSTATUS32_SD  // 32 位版本
# define SSTATUS_SD SSTATUS32_SD  // 32 位版本
# define RISCV_PGLEVEL_BITS 10  // 32 位：两级页表（10 + 10 + 12 = 32 位）
#endif
/* RISCV_PGSHIFT / RISCV_PGSIZE：本文件自带的页大小定义，和
 *   kern/mm/mmu.h 里的 PGSHIFT/PGSIZE 含义相同（12 / 4096）。
 *   两份定义来自不同年代，实际编译内核时页大小以 mmu.h 为准。*/
#define RISCV_PGSHIFT 12  // 本文件自带的页大小定义（与 mmu.h 中一致）
#define RISCV_PGSIZE (1 << RISCV_PGSHIFT)  // = 1 << 12 = 4096

#ifndef __ASSEMBLER__
/* 汇编文件（.S）经过 C 预处理器时会定义 __ASSEMBLER__，
 *   下面 read_csr 之类的 C 语法宏对汇编器毫无意义，必须排除掉，
 *   否则汇编时会报错。这是"一个头文件同时给 C 和汇编用"的标准写法。*/

/* 以下宏使用 GCC 的扩展（语句表达式、内建函数），
 *   所以再用 __GNUC__ 包一层，保证别的编译器也不会被坑。*/
#ifdef __GNUC__

/* =====================================================================
 * ★ read_csr —— 读一个 CSR 寄存器（后续实验最常用的宏之一）
 *   展开成一条 csrr 指令：把寄存器 reg 的值读到临时变量 __tmp 返回。
 *   用法：unsigned long x = read_csr(sstatus);
 *   关键字解释：
 *     "csrr %0, " #reg  —— #reg 是"字符串化"，你在调用处写 sstatus，
 *                         预处理器就把它变成字符串 "sstatus"，于是汇编
 *                         看到的完整指令是 csrr t0, sstatus。这正是我们能
 *                         用 C 名字直接访问 CSR 的原因。
 *     "=r"(__tmp)       —— 输出操作数：结果放一个通用寄存器，再存回 __tmp。
 *     ({ ... })         —— GCC 语句表达式：让宏像函数一样有"返回值"。
 *   答辩提示：CSR 名字（如 sstatus）是汇编器认识的符号，由 riscv.h 后半
 *   部分自动生成的 CSR_* 编号表 + 编译器的 -march 支持共同保证。
 * ===================================================================== */
#define read_csr(reg) ({ unsigned long __tmp; \
  asm volatile ("csrr %0, " #reg : "=r"(__tmp)); \
  __tmp; })

/* =====================================================================
 * ★ write_csr —— 写一个 CSR 寄存器（csrw 指令）
 *   那个 if/else 是性能优化而不是逻辑分支：
 *     如果值是编译期常量且小于 32，可以编码进指令的立即数位（csrw reg, imm），
 *     少占一个寄存器（用 "i" 约束请求立即数形式）；
 *     否则只能先把值放进寄存器再写（用 "r" 约束）。
 *   两个分支功能完全相同 —— 答辩时被问到"为什么写两遍"就是这个答案。
 * ===================================================================== */
#define write_csr(reg, val) ({ \
  if (__builtin_constant_p(val) && (unsigned long)(val) < 32) \
    asm volatile ("csrw " #reg ", %0" :: "i"(val)); \
  else \
    asm volatile ("csrw " #reg ", %0" :: "r"(val)); })

/* swap_csr —— 原子地"读出旧值并写入新值"（csrrw 指令）。
 *   返回值是旧值。常用于"先保存再修改"的场景，而且不会被中断打断。*/
#define swap_csr(reg, val) ({ unsigned long __tmp; \
  if (__builtin_constant_p(val) && (unsigned long)(val) < 32) \
    asm volatile ("csrrw %0, " #reg ", %1" : "=r"(__tmp) : "i"(val)); \
  else \
    asm volatile ("csrrw %0, " #reg ", %1" : "=r"(__tmp) : "r"(val)); \
  __tmp; })

/* set_csr —— 把指定的位置 1，其余位保持不变（csrrs 指令）。
 *   例子：set_csr(sstatus, SSTATUS_SIE) 打开 S 态中断总开关。
 *   注意它同样返回旧值（宏的返回值），不关心时可以忽略。*/
#define set_csr(reg, bit) ({ unsigned long __tmp; \
  if (__builtin_constant_p(bit) && (unsigned long)(bit) < 32) \
    asm volatile ("csrrs %0, " #reg ", %1" : "=r"(__tmp) : "i"(bit)); \
  else \
    asm volatile ("csrrs %0, " #reg ", %1" : "=r"(__tmp) : "r"(bit)); \
  __tmp; })

/* clear_csr —— 把指定的位清 0，其余位保持不变（csrrc 指令）。
 *   例子：clear_csr(sstatus, SSTATUS_SIE) 关闭中断。*/
#define clear_csr(reg, bit) ({ unsigned long __tmp; \
  if (__builtin_constant_p(bit) && (unsigned long)(bit) < 32) \
    asm volatile ("csrrc %0, " #reg ", %1" : "=r"(__tmp) : "i"(bit)); \
  else \
    asm volatile ("csrrc %0, " #reg ", %1" : "=r"(__tmp) : "r"(bit)); \
  __tmp; })

/* 三个只读计数器的便捷宏：时间/周期数/指令数。
 *   注意它们在汇编层就是 CSR 名（time/cycle/instret），
 *   不需要先查 CSR_TIME 这样的编号 —— 那是给 C 语言直接读写数值用的。*/
#define rdtime() read_csr(time)  // 读 time CSR：系统时间（微秒级时基）
#define rdcycle() read_csr(cycle)  // 读 cycle CSR：已执行周期数
#define rdinstret() read_csr(instret)  // 读 instret CSR：已退休指令数（性能统计用）

#endif

/* =====================================================================
 * ★ do_div(n, base) —— 【lab1 唯一被真正使用的宏】
 *   它同时给出两个结果（这是它存在的唯一理由）：
 *     宏的"返回值" = n % base（余数）
 *     副作用       = 把 n 改成 n / base（商）
 *   printfmt.c 的 printnum 正是靠它一次拿到最低位数字和剩下的高位：
 *     unsigned mod = do_div(result, base);
 *   用法上有个坑：n 必须是可以取地址的变量（不能被改成 n/base），
 *   因为宏里对它赋值。这类"必须传左值"的宏在答辩时很容易被追问。
 * ===================================================================== */
#define do_div(n, base)                              \
    ({                                               \
        int __res;                                   \
        __res = ((unsigned long)n) % (unsigned)base; \
        n = ((unsigned long)n) / (unsigned)base;     \
        __res;                                       \
    })

/* barrier() —— 内存屏障（fence 指令）。
 *   告诉 CPU 和编译器：这里之前的访存必须全部完成，之后的访存才能开始。
 *   单核 lab1 用不到；多核或多设备实验里，编译器优化与乱序执行会让
 *   "看起来没问题的代码"出错，这时就必须插 fence。*/
#define barrier() __asm__ __volatile__ ("fence" ::: "memory")

/* lcr3 —— 在 x86 时代它负责把页目录基址写进 CR3 寄存器；
 *   RISC-V 版把它等价地实现为"写 sptbr（页表基址寄存器）"（新版规范叫 satp）。
 *   lab1 用不到；lab2 开启分页时，一行 lcr3(页表物理地址) 就完成地址空间切换。*/
static inline void
lcr3(unsigned int cr3) {
    write_csr(sptbr, cr3 >> RISCV_PGSHIFT);
}

#endif

#endif

#endif
/* ============================================================================
 * ★ 从这里开始是【自动生成】的部分（parse-opcodes 工具生成，不要手改）
 * ============================================================================
 * 结构（三张表 + 一批"声明回调"）：
 *   1) MATCH_xxx / MASK_xxx：每条指令的编码模板与掩码。判断一条 32 位指令
 *      是否为某指令： (inst & MASK_xxx) == MATCH_xxx。
 *      例如 MATCH_ECALL = 0x00000073 —— 这正是 sbi.c 里那条 ecall 的机器码。
 *   2) CSR_xxx：控制状态寄存器的 12 位编号。
 *      例如 CSR_MSTATUS = 0x300 —— ecall/CSR 指令在硬件层面用的就是这个数。
 *      注意：read_csr(sstatus) 里写的是"名字"，编译器和汇编器再把它转成编号。
 *   3) CAUSE_xxx：异常/中断的原因号，会被填进 scause/mcause 寄存器。
 *      例如 CAUSE_MACHINE_ECALL = 0xb：S 态执行 ecall 进 M 态后，
 *       OpenSBI 读 mcause 就靠它判断"这是内核来请求服务了" ——
 *       也就是说，sbi.c 那 6 行内联汇编的"另一端"在这里有定义。
 * 4) DECLARE_CSR / DECLARE_INSN / DECLARE_CAUSE 是"回调式宏"：
 *      只有先 #define DECLARE_CSR(name, num)，下面几百行才会展开成东西；
 *      没人定义它们时，这一大段在预处理后就是空的（不产生任何代码）。
 *      反汇编工具、模拟器正是靠这种机制复用同一份表格。
 * ============================================================================
 */
/* Automatically generated by parse-opcodes */
#ifndef RISCV_ENCODING_H
#define RISCV_ENCODING_H
#define MATCH_BEQ 0x63
#define MASK_BEQ  0x707f
#define MATCH_BNE 0x1063
#define MASK_BNE  0x707f
#define MATCH_BLT 0x4063
#define MASK_BLT  0x707f
#define MATCH_BGE 0x5063
#define MASK_BGE  0x707f
#define MATCH_BLTU 0x6063
#define MASK_BLTU  0x707f
#define MATCH_BGEU 0x7063
#define MASK_BGEU  0x707f
#define MATCH_JALR 0x67
#define MASK_JALR  0x707f
#define MATCH_JAL 0x6f
#define MASK_JAL  0x7f
#define MATCH_LUI 0x37
#define MASK_LUI  0x7f
#define MATCH_AUIPC 0x17
#define MASK_AUIPC  0x7f
#define MATCH_ADDI 0x13
#define MASK_ADDI  0x707f
#define MATCH_SLLI 0x1013
#define MASK_SLLI  0xfc00707f
#define MATCH_SLTI 0x2013
#define MASK_SLTI  0x707f
#define MATCH_SLTIU 0x3013
#define MASK_SLTIU  0x707f
#define MATCH_XORI 0x4013
#define MASK_XORI  0x707f
#define MATCH_SRLI 0x5013
#define MASK_SRLI  0xfc00707f
#define MATCH_SRAI 0x40005013
#define MASK_SRAI  0xfc00707f
#define MATCH_ORI 0x6013
#define MASK_ORI  0x707f
#define MATCH_ANDI 0x7013
#define MASK_ANDI  0x707f
#define MATCH_ADD 0x33
#define MASK_ADD  0xfe00707f
#define MATCH_SUB 0x40000033
#define MASK_SUB  0xfe00707f
#define MATCH_SLL 0x1033
#define MASK_SLL  0xfe00707f
#define MATCH_SLT 0x2033
#define MASK_SLT  0xfe00707f
#define MATCH_SLTU 0x3033
#define MASK_SLTU  0xfe00707f
#define MATCH_XOR 0x4033
#define MASK_XOR  0xfe00707f
#define MATCH_SRL 0x5033
#define MASK_SRL  0xfe00707f
#define MATCH_SRA 0x40005033
#define MASK_SRA  0xfe00707f
#define MATCH_OR 0x6033
#define MASK_OR  0xfe00707f
#define MATCH_AND 0x7033
#define MASK_AND  0xfe00707f
#define MATCH_ADDIW 0x1b
#define MASK_ADDIW  0x707f
#define MATCH_SLLIW 0x101b
#define MASK_SLLIW  0xfe00707f
#define MATCH_SRLIW 0x501b
#define MASK_SRLIW  0xfe00707f
#define MATCH_SRAIW 0x4000501b
#define MASK_SRAIW  0xfe00707f
#define MATCH_ADDW 0x3b
#define MASK_ADDW  0xfe00707f
#define MATCH_SUBW 0x4000003b
#define MASK_SUBW  0xfe00707f
#define MATCH_SLLW 0x103b
#define MASK_SLLW  0xfe00707f
#define MATCH_SRLW 0x503b
#define MASK_SRLW  0xfe00707f
#define MATCH_SRAW 0x4000503b
#define MASK_SRAW  0xfe00707f
#define MATCH_LB 0x3
#define MASK_LB  0x707f
#define MATCH_LH 0x1003
#define MASK_LH  0x707f
#define MATCH_LW 0x2003
#define MASK_LW  0x707f
#define MATCH_LD 0x3003
#define MASK_LD  0x707f
#define MATCH_LBU 0x4003
#define MASK_LBU  0x707f
#define MATCH_LHU 0x5003
#define MASK_LHU  0x707f
#define MATCH_LWU 0x6003
#define MASK_LWU  0x707f
#define MATCH_SB 0x23
#define MASK_SB  0x707f
#define MATCH_SH 0x1023
#define MASK_SH  0x707f
#define MATCH_SW 0x2023
#define MASK_SW  0x707f
#define MATCH_SD 0x3023
#define MASK_SD  0x707f
#define MATCH_FENCE 0xf
#define MASK_FENCE  0x707f
#define MATCH_FENCE_I 0x100f
#define MASK_FENCE_I  0x707f
#define MATCH_MUL 0x2000033
#define MASK_MUL  0xfe00707f
#define MATCH_MULH 0x2001033
#define MASK_MULH  0xfe00707f
#define MATCH_MULHSU 0x2002033
#define MASK_MULHSU  0xfe00707f
#define MATCH_MULHU 0x2003033
#define MASK_MULHU  0xfe00707f
#define MATCH_DIV 0x2004033
#define MASK_DIV  0xfe00707f
#define MATCH_DIVU 0x2005033
#define MASK_DIVU  0xfe00707f
#define MATCH_REM 0x2006033
#define MASK_REM  0xfe00707f
#define MATCH_REMU 0x2007033
#define MASK_REMU  0xfe00707f
#define MATCH_MULW 0x200003b
#define MASK_MULW  0xfe00707f
#define MATCH_DIVW 0x200403b
#define MASK_DIVW  0xfe00707f
#define MATCH_DIVUW 0x200503b
#define MASK_DIVUW  0xfe00707f
#define MATCH_REMW 0x200603b
#define MASK_REMW  0xfe00707f
#define MATCH_REMUW 0x200703b
#define MASK_REMUW  0xfe00707f
#define MATCH_AMOADD_W 0x202f
#define MASK_AMOADD_W  0xf800707f
#define MATCH_AMOXOR_W 0x2000202f
#define MASK_AMOXOR_W  0xf800707f
#define MATCH_AMOOR_W 0x4000202f
#define MASK_AMOOR_W  0xf800707f
#define MATCH_AMOAND_W 0x6000202f
#define MASK_AMOAND_W  0xf800707f
#define MATCH_AMOMIN_W 0x8000202f
#define MASK_AMOMIN_W  0xf800707f
#define MATCH_AMOMAX_W 0xa000202f
#define MASK_AMOMAX_W  0xf800707f
#define MATCH_AMOMINU_W 0xc000202f
#define MASK_AMOMINU_W  0xf800707f
#define MATCH_AMOMAXU_W 0xe000202f
#define MASK_AMOMAXU_W  0xf800707f
#define MATCH_AMOSWAP_W 0x800202f
#define MASK_AMOSWAP_W  0xf800707f
#define MATCH_LR_W 0x1000202f
#define MASK_LR_W  0xf9f0707f
#define MATCH_SC_W 0x1800202f
#define MASK_SC_W  0xf800707f
#define MATCH_AMOADD_D 0x302f
#define MASK_AMOADD_D  0xf800707f
#define MATCH_AMOXOR_D 0x2000302f
#define MASK_AMOXOR_D  0xf800707f
#define MATCH_AMOOR_D 0x4000302f
#define MASK_AMOOR_D  0xf800707f
#define MATCH_AMOAND_D 0x6000302f
#define MASK_AMOAND_D  0xf800707f
#define MATCH_AMOMIN_D 0x8000302f
#define MASK_AMOMIN_D  0xf800707f
#define MATCH_AMOMAX_D 0xa000302f
#define MASK_AMOMAX_D  0xf800707f
#define MATCH_AMOMINU_D 0xc000302f
#define MASK_AMOMINU_D  0xf800707f
#define MATCH_AMOMAXU_D 0xe000302f
#define MASK_AMOMAXU_D  0xf800707f
#define MATCH_AMOSWAP_D 0x800302f
#define MASK_AMOSWAP_D  0xf800707f
#define MATCH_LR_D 0x1000302f
#define MASK_LR_D  0xf9f0707f
#define MATCH_SC_D 0x1800302f
#define MASK_SC_D  0xf800707f
#define MATCH_ECALL 0x73
#define MASK_ECALL  0xffffffff
#define MATCH_EBREAK 0x100073
#define MASK_EBREAK  0xffffffff
#define MATCH_URET 0x200073
#define MASK_URET  0xffffffff
#define MATCH_SRET 0x10200073
#define MASK_SRET  0xffffffff
#define MATCH_HRET 0x20200073
#define MASK_HRET  0xffffffff
#define MATCH_MRET 0x30200073
#define MASK_MRET  0xffffffff
#define MATCH_DRET 0x7b200073
#define MASK_DRET  0xffffffff
#define MATCH_SFENCE_VM 0x10400073
#define MASK_SFENCE_VM  0xfff07fff
#define MATCH_WFI 0x10500073
#define MASK_WFI  0xffffffff
#define MATCH_CSRRW 0x1073
#define MASK_CSRRW  0x707f
#define MATCH_CSRRS 0x2073
#define MASK_CSRRS  0x707f
#define MATCH_CSRRC 0x3073
#define MASK_CSRRC  0x707f
#define MATCH_CSRRWI 0x5073
#define MASK_CSRRWI  0x707f
#define MATCH_CSRRSI 0x6073
#define MASK_CSRRSI  0x707f
#define MATCH_CSRRCI 0x7073
#define MASK_CSRRCI  0x707f
#define MATCH_FADD_S 0x53
#define MASK_FADD_S  0xfe00007f
#define MATCH_FSUB_S 0x8000053
#define MASK_FSUB_S  0xfe00007f
#define MATCH_FMUL_S 0x10000053
#define MASK_FMUL_S  0xfe00007f
#define MATCH_FDIV_S 0x18000053
#define MASK_FDIV_S  0xfe00007f
#define MATCH_FSGNJ_S 0x20000053
#define MASK_FSGNJ_S  0xfe00707f
#define MATCH_FSGNJN_S 0x20001053
#define MASK_FSGNJN_S  0xfe00707f
#define MATCH_FSGNJX_S 0x20002053
#define MASK_FSGNJX_S  0xfe00707f
#define MATCH_FMIN_S 0x28000053
#define MASK_FMIN_S  0xfe00707f
#define MATCH_FMAX_S 0x28001053
#define MASK_FMAX_S  0xfe00707f
#define MATCH_FSQRT_S 0x58000053
#define MASK_FSQRT_S  0xfff0007f
#define MATCH_FADD_D 0x2000053
#define MASK_FADD_D  0xfe00007f
#define MATCH_FSUB_D 0xa000053
#define MASK_FSUB_D  0xfe00007f
#define MATCH_FMUL_D 0x12000053
#define MASK_FMUL_D  0xfe00007f
#define MATCH_FDIV_D 0x1a000053
#define MASK_FDIV_D  0xfe00007f
#define MATCH_FSGNJ_D 0x22000053
#define MASK_FSGNJ_D  0xfe00707f
#define MATCH_FSGNJN_D 0x22001053
#define MASK_FSGNJN_D  0xfe00707f
#define MATCH_FSGNJX_D 0x22002053
#define MASK_FSGNJX_D  0xfe00707f
#define MATCH_FMIN_D 0x2a000053
#define MASK_FMIN_D  0xfe00707f
#define MATCH_FMAX_D 0x2a001053
#define MASK_FMAX_D  0xfe00707f
#define MATCH_FCVT_S_D 0x40100053
#define MASK_FCVT_S_D  0xfff0007f
#define MATCH_FCVT_D_S 0x42000053
#define MASK_FCVT_D_S  0xfff0007f
#define MATCH_FSQRT_D 0x5a000053
#define MASK_FSQRT_D  0xfff0007f
#define MATCH_FLE_S 0xa0000053
#define MASK_FLE_S  0xfe00707f
#define MATCH_FLT_S 0xa0001053
#define MASK_FLT_S  0xfe00707f
#define MATCH_FEQ_S 0xa0002053
#define MASK_FEQ_S  0xfe00707f
#define MATCH_FLE_D 0xa2000053
#define MASK_FLE_D  0xfe00707f
#define MATCH_FLT_D 0xa2001053
#define MASK_FLT_D  0xfe00707f
#define MATCH_FEQ_D 0xa2002053
#define MASK_FEQ_D  0xfe00707f
#define MATCH_FCVT_W_S 0xc0000053
#define MASK_FCVT_W_S  0xfff0007f
#define MATCH_FCVT_WU_S 0xc0100053
#define MASK_FCVT_WU_S  0xfff0007f
#define MATCH_FCVT_L_S 0xc0200053
#define MASK_FCVT_L_S  0xfff0007f
#define MATCH_FCVT_LU_S 0xc0300053
#define MASK_FCVT_LU_S  0xfff0007f
#define MATCH_FMV_X_S 0xe0000053
#define MASK_FMV_X_S  0xfff0707f
#define MATCH_FCLASS_S 0xe0001053
#define MASK_FCLASS_S  0xfff0707f
#define MATCH_FCVT_W_D 0xc2000053
#define MASK_FCVT_W_D  0xfff0007f
#define MATCH_FCVT_WU_D 0xc2100053
#define MASK_FCVT_WU_D  0xfff0007f
#define MATCH_FCVT_L_D 0xc2200053
#define MASK_FCVT_L_D  0xfff0007f
#define MATCH_FCVT_LU_D 0xc2300053
#define MASK_FCVT_LU_D  0xfff0007f
#define MATCH_FMV_X_D 0xe2000053
#define MASK_FMV_X_D  0xfff0707f
#define MATCH_FCLASS_D 0xe2001053
#define MASK_FCLASS_D  0xfff0707f
#define MATCH_FCVT_S_W 0xd0000053
#define MASK_FCVT_S_W  0xfff0007f
#define MATCH_FCVT_S_WU 0xd0100053
#define MASK_FCVT_S_WU  0xfff0007f
#define MATCH_FCVT_S_L 0xd0200053
#define MASK_FCVT_S_L  0xfff0007f
#define MATCH_FCVT_S_LU 0xd0300053
#define MASK_FCVT_S_LU  0xfff0007f
#define MATCH_FMV_S_X 0xf0000053
#define MASK_FMV_S_X  0xfff0707f
#define MATCH_FCVT_D_W 0xd2000053
#define MASK_FCVT_D_W  0xfff0007f
#define MATCH_FCVT_D_WU 0xd2100053
#define MASK_FCVT_D_WU  0xfff0007f
#define MATCH_FCVT_D_L 0xd2200053
#define MASK_FCVT_D_L  0xfff0007f
#define MATCH_FCVT_D_LU 0xd2300053
#define MASK_FCVT_D_LU  0xfff0007f
#define MATCH_FMV_D_X 0xf2000053
#define MASK_FMV_D_X  0xfff0707f
#define MATCH_FLW 0x2007
#define MASK_FLW  0x707f
#define MATCH_FLD 0x3007
#define MASK_FLD  0x707f
#define MATCH_FSW 0x2027
#define MASK_FSW  0x707f
#define MATCH_FSD 0x3027
#define MASK_FSD  0x707f
#define MATCH_FMADD_S 0x43
#define MASK_FMADD_S  0x600007f
#define MATCH_FMSUB_S 0x47
#define MASK_FMSUB_S  0x600007f
#define MATCH_FNMSUB_S 0x4b
#define MASK_FNMSUB_S  0x600007f
#define MATCH_FNMADD_S 0x4f
#define MASK_FNMADD_S  0x600007f
#define MATCH_FMADD_D 0x2000043
#define MASK_FMADD_D  0x600007f
#define MATCH_FMSUB_D 0x2000047
#define MASK_FMSUB_D  0x600007f
#define MATCH_FNMSUB_D 0x200004b
#define MASK_FNMSUB_D  0x600007f
#define MATCH_FNMADD_D 0x200004f
#define MASK_FNMADD_D  0x600007f
#define MATCH_C_NOP 0x1
#define MASK_C_NOP  0xffff
#define MATCH_C_ADDI16SP 0x6101
#define MASK_C_ADDI16SP  0xef83
#define MATCH_C_JR 0x8002
#define MASK_C_JR  0xf07f
#define MATCH_C_JALR 0x9002
#define MASK_C_JALR  0xf07f
#define MATCH_C_EBREAK 0x9002
#define MASK_C_EBREAK  0xffff
#define MATCH_C_LD 0x6000
#define MASK_C_LD  0xe003
#define MATCH_C_SD 0xe000
#define MASK_C_SD  0xe003
#define MATCH_C_ADDIW 0x2001
#define MASK_C_ADDIW  0xe003
#define MATCH_C_LDSP 0x6002
#define MASK_C_LDSP  0xe003
#define MATCH_C_SDSP 0xe002
#define MASK_C_SDSP  0xe003
#define MATCH_C_ADDI4SPN 0x0
#define MASK_C_ADDI4SPN  0xe003
#define MATCH_C_FLD 0x2000
#define MASK_C_FLD  0xe003
#define MATCH_C_LW 0x4000
#define MASK_C_LW  0xe003
#define MATCH_C_FLW 0x6000
#define MASK_C_FLW  0xe003
#define MATCH_C_FSD 0xa000
#define MASK_C_FSD  0xe003
#define MATCH_C_SW 0xc000
#define MASK_C_SW  0xe003
#define MATCH_C_FSW 0xe000
#define MASK_C_FSW  0xe003
#define MATCH_C_ADDI 0x1
#define MASK_C_ADDI  0xe003
#define MATCH_C_JAL 0x2001
#define MASK_C_JAL  0xe003
#define MATCH_C_LI 0x4001
#define MASK_C_LI  0xe003
#define MATCH_C_LUI 0x6001
#define MASK_C_LUI  0xe003
#define MATCH_C_SRLI 0x8001
#define MASK_C_SRLI  0xec03
#define MATCH_C_SRAI 0x8401
#define MASK_C_SRAI  0xec03
#define MATCH_C_ANDI 0x8801
#define MASK_C_ANDI  0xec03
#define MATCH_C_SUB 0x8c01
#define MASK_C_SUB  0xfc63
#define MATCH_C_XOR 0x8c21
#define MASK_C_XOR  0xfc63
#define MATCH_C_OR 0x8c41
#define MASK_C_OR  0xfc63
#define MATCH_C_AND 0x8c61
#define MASK_C_AND  0xfc63
#define MATCH_C_SUBW 0x9c01
#define MASK_C_SUBW  0xfc63
#define MATCH_C_ADDW 0x9c21
#define MASK_C_ADDW  0xfc63
#define MATCH_C_J 0xa001
#define MASK_C_J  0xe003
#define MATCH_C_BEQZ 0xc001
#define MASK_C_BEQZ  0xe003
#define MATCH_C_BNEZ 0xe001
#define MASK_C_BNEZ  0xe003
#define MATCH_C_SLLI 0x2
#define MASK_C_SLLI  0xe003
#define MATCH_C_FLDSP 0x2002
#define MASK_C_FLDSP  0xe003
#define MATCH_C_LWSP 0x4002
#define MASK_C_LWSP  0xe003
#define MATCH_C_FLWSP 0x6002
#define MASK_C_FLWSP  0xe003
#define MATCH_C_MV 0x8002
#define MASK_C_MV  0xf003
#define MATCH_C_ADD 0x9002
#define MASK_C_ADD  0xf003
#define MATCH_C_FSDSP 0xa002
#define MASK_C_FSDSP  0xe003
#define MATCH_C_SWSP 0xc002
#define MASK_C_SWSP  0xe003
#define MATCH_C_FSWSP 0xe002
#define MASK_C_FSWSP  0xe003
#define MATCH_CUSTOM0 0xb
#define MASK_CUSTOM0  0x707f
#define MATCH_CUSTOM0_RS1 0x200b
#define MASK_CUSTOM0_RS1  0x707f
#define MATCH_CUSTOM0_RS1_RS2 0x300b
#define MASK_CUSTOM0_RS1_RS2  0x707f
#define MATCH_CUSTOM0_RD 0x400b
#define MASK_CUSTOM0_RD  0x707f
#define MATCH_CUSTOM0_RD_RS1 0x600b
#define MASK_CUSTOM0_RD_RS1  0x707f
#define MATCH_CUSTOM0_RD_RS1_RS2 0x700b
#define MASK_CUSTOM0_RD_RS1_RS2  0x707f
#define MATCH_CUSTOM1 0x2b
#define MASK_CUSTOM1  0x707f
#define MATCH_CUSTOM1_RS1 0x202b
#define MASK_CUSTOM1_RS1  0x707f
#define MATCH_CUSTOM1_RS1_RS2 0x302b
#define MASK_CUSTOM1_RS1_RS2  0x707f
#define MATCH_CUSTOM1_RD 0x402b
#define MASK_CUSTOM1_RD  0x707f
#define MATCH_CUSTOM1_RD_RS1 0x602b
#define MASK_CUSTOM1_RD_RS1  0x707f
#define MATCH_CUSTOM1_RD_RS1_RS2 0x702b
#define MASK_CUSTOM1_RD_RS1_RS2  0x707f
#define MATCH_CUSTOM2 0x5b
#define MASK_CUSTOM2  0x707f
#define MATCH_CUSTOM2_RS1 0x205b
#define MASK_CUSTOM2_RS1  0x707f
#define MATCH_CUSTOM2_RS1_RS2 0x305b
#define MASK_CUSTOM2_RS1_RS2  0x707f
#define MATCH_CUSTOM2_RD 0x405b
#define MASK_CUSTOM2_RD  0x707f
#define MATCH_CUSTOM2_RD_RS1 0x605b
#define MASK_CUSTOM2_RD_RS1  0x707f
#define MATCH_CUSTOM2_RD_RS1_RS2 0x705b
#define MASK_CUSTOM2_RD_RS1_RS2  0x707f
#define MATCH_CUSTOM3 0x7b
#define MASK_CUSTOM3  0x707f
#define MATCH_CUSTOM3_RS1 0x207b
#define MASK_CUSTOM3_RS1  0x707f
#define MATCH_CUSTOM3_RS1_RS2 0x307b
#define MASK_CUSTOM3_RS1_RS2  0x707f
#define MATCH_CUSTOM3_RD 0x407b
#define MASK_CUSTOM3_RD  0x707f
#define MATCH_CUSTOM3_RD_RS1 0x607b
#define MASK_CUSTOM3_RD_RS1  0x707f
#define MATCH_CUSTOM3_RD_RS1_RS2 0x707b
#define MASK_CUSTOM3_RD_RS1_RS2  0x707f
/* ------------------------------------------------------------------
 * CSR 编号表（0x000 ~ 0xfff，共 4096 个编号，这里只列常用的）。
 *    编码规律（写在规范里，看懂规律就不必背）：
 *      高 4 位区分读写权限与是否计数器，低 8 位标功能；
 *      [0x100, 0x1ff] 是 S 态（管理态）寄存器：sstatus/sie/stvec/sepc/scause/satp…
 *      [0x300, 0x3ff] 是 M 态（机器态）寄存器：mstatus/mie/mtvec/mepc/mcause…
 *      [0xc00, 0xcff] 是只读计数器：cycle/time/instret（对应上面的 rdtime 等）
 *      [0xf00, 0xfff] 是只读标识寄存器：mvendorid/marchid/mimpid/mhartid…
 *    因为我们的内核是 S 态，写 vstvec/写 stvec 才是合法操作，
 *    写 mtvec 会触发非法指令异常 —— 权限由此体现。
 * ------------------------------------------------------------------ */
#define CSR_FFLAGS 0x1  // 浮点异常标志
#define CSR_FRM 0x2
#define CSR_FCSR 0x3
#define CSR_CYCLE 0xc00  // 周期计数器（低 32 位）
#define CSR_TIME 0xc01  // 时间计数器（低 32 位）
#define CSR_INSTRET 0xc02
#define CSR_HPMCOUNTER3 0xc03
#define CSR_HPMCOUNTER4 0xc04
#define CSR_HPMCOUNTER5 0xc05
#define CSR_HPMCOUNTER6 0xc06
#define CSR_HPMCOUNTER7 0xc07
#define CSR_HPMCOUNTER8 0xc08
#define CSR_HPMCOUNTER9 0xc09
#define CSR_HPMCOUNTER10 0xc0a
#define CSR_HPMCOUNTER11 0xc0b
#define CSR_HPMCOUNTER12 0xc0c
#define CSR_HPMCOUNTER13 0xc0d
#define CSR_HPMCOUNTER14 0xc0e
#define CSR_HPMCOUNTER15 0xc0f
#define CSR_HPMCOUNTER16 0xc10
#define CSR_HPMCOUNTER17 0xc11
#define CSR_HPMCOUNTER18 0xc12
#define CSR_HPMCOUNTER19 0xc13
#define CSR_HPMCOUNTER20 0xc14
#define CSR_HPMCOUNTER21 0xc15
#define CSR_HPMCOUNTER22 0xc16
#define CSR_HPMCOUNTER23 0xc17
#define CSR_HPMCOUNTER24 0xc18
#define CSR_HPMCOUNTER25 0xc19
#define CSR_HPMCOUNTER26 0xc1a
#define CSR_HPMCOUNTER27 0xc1b
#define CSR_HPMCOUNTER28 0xc1c
#define CSR_HPMCOUNTER29 0xc1d
#define CSR_HPMCOUNTER30 0xc1e
#define CSR_HPMCOUNTER31 0xc1f
#define CSR_SSTATUS 0x100  // S 态状态寄存器（mstatus 的 S 态视图）
#define CSR_SIE 0x104  // S 态中断使能
#define CSR_STVEC 0x105  // S 态 trap 入口地址
#define CSR_SSCRATCH 0x140
#define CSR_SEPC 0x141  // S 态异常时的 PC（保存"出错的那条指令地址"）
#define CSR_SCAUSE 0x142  // S 态 trap 原因号
#define CSR_SBADADDR 0x143
#define CSR_SIP 0x144  // S 态中断挂起位
#define CSR_SPTBR 0x180  // 页表基址寄存器（新版规范叫 satp）：分页实验要写它
#define CSR_MSTATUS 0x300  // M 态状态寄存器（OpenSBI 用）
#define CSR_MISA 0x301  // 机器指令集架构描述（MISA：字长 + 支持的扩展）
#define CSR_MEDELEG 0x302  // M 态委托给 S 态的异常
#define CSR_MIDELEG 0x303
#define CSR_MIE 0x304  // M 态中断使能
#define CSR_MTVEC 0x305  // M 态 trap 入口地址
#define CSR_MSCRATCH 0x340  // M 态临时寄存器（保存一个指针，trap 时可用）
#define CSR_MEPC 0x341  // M 态异常时的 PC
#define CSR_MCAUSE 0x342  // M 态 trap 原因号（ecall 进 M 态时靠它识别服务请求）
#define CSR_MBADADDR 0x343
#define CSR_MIP 0x344  // M 态中断挂起位
#define CSR_TSELECT 0x7a0  // 断点/触发器选择寄存器
#define CSR_TDATA1 0x7a1
#define CSR_TDATA2 0x7a2
#define CSR_TDATA3 0x7a3
#define CSR_DCSR 0x7b0
#define CSR_DPC 0x7b1
#define CSR_DSCRATCH 0x7b2
#define CSR_MCYCLE 0xb00
#define CSR_MINSTRET 0xb02
#define CSR_MHPMCOUNTER3 0xb03
#define CSR_MHPMCOUNTER4 0xb04
#define CSR_MHPMCOUNTER5 0xb05
#define CSR_MHPMCOUNTER6 0xb06
#define CSR_MHPMCOUNTER7 0xb07
#define CSR_MHPMCOUNTER8 0xb08
#define CSR_MHPMCOUNTER9 0xb09
#define CSR_MHPMCOUNTER10 0xb0a
#define CSR_MHPMCOUNTER11 0xb0b
#define CSR_MHPMCOUNTER12 0xb0c
#define CSR_MHPMCOUNTER13 0xb0d
#define CSR_MHPMCOUNTER14 0xb0e
#define CSR_MHPMCOUNTER15 0xb0f
#define CSR_MHPMCOUNTER16 0xb10
#define CSR_MHPMCOUNTER17 0xb11
#define CSR_MHPMCOUNTER18 0xb12
#define CSR_MHPMCOUNTER19 0xb13
#define CSR_MHPMCOUNTER20 0xb14
#define CSR_MHPMCOUNTER21 0xb15
#define CSR_MHPMCOUNTER22 0xb16
#define CSR_MHPMCOUNTER23 0xb17
#define CSR_MHPMCOUNTER24 0xb18
#define CSR_MHPMCOUNTER25 0xb19
#define CSR_MHPMCOUNTER26 0xb1a
#define CSR_MHPMCOUNTER27 0xb1b
#define CSR_MHPMCOUNTER28 0xb1c
#define CSR_MHPMCOUNTER29 0xb1d
#define CSR_MHPMCOUNTER30 0xb1e
#define CSR_MHPMCOUNTER31 0xb1f
#define CSR_MUCOUNTEREN 0x320
#define CSR_MSCOUNTEREN 0x321
#define CSR_MHPMEVENT3 0x323
#define CSR_MHPMEVENT4 0x324
#define CSR_MHPMEVENT5 0x325
#define CSR_MHPMEVENT6 0x326
#define CSR_MHPMEVENT7 0x327
#define CSR_MHPMEVENT8 0x328
#define CSR_MHPMEVENT9 0x329
#define CSR_MHPMEVENT10 0x32a
#define CSR_MHPMEVENT11 0x32b
#define CSR_MHPMEVENT12 0x32c
#define CSR_MHPMEVENT13 0x32d
#define CSR_MHPMEVENT14 0x32e
#define CSR_MHPMEVENT15 0x32f
#define CSR_MHPMEVENT16 0x330
#define CSR_MHPMEVENT17 0x331
#define CSR_MHPMEVENT18 0x332
#define CSR_MHPMEVENT19 0x333
#define CSR_MHPMEVENT20 0x334
#define CSR_MHPMEVENT21 0x335
#define CSR_MHPMEVENT22 0x336
#define CSR_MHPMEVENT23 0x337
#define CSR_MHPMEVENT24 0x338
#define CSR_MHPMEVENT25 0x339
#define CSR_MHPMEVENT26 0x33a
#define CSR_MHPMEVENT27 0x33b
#define CSR_MHPMEVENT28 0x33c
#define CSR_MHPMEVENT29 0x33d
#define CSR_MHPMEVENT30 0x33e
#define CSR_MHPMEVENT31 0x33f
/* 以下几个是只读的"身份"寄存器。*/
#define CSR_MVENDORID 0xf11  // 厂商 ID（只读）
#define CSR_MARCHID 0xf12
#define CSR_MIMPID 0xf13
#define CSR_MHARTID 0xf14  // ★ hart id：当前 CPU 核的编号（只读），多核实验用
#define CSR_CYCLEH 0xc80  // 周期计数器（高 32 位）
#define CSR_TIMEH 0xc81  // 时间计数器（高 32 位）
#define CSR_INSTRETH 0xc82
#define CSR_HPMCOUNTER3H 0xc83
#define CSR_HPMCOUNTER4H 0xc84
#define CSR_HPMCOUNTER5H 0xc85
#define CSR_HPMCOUNTER6H 0xc86
#define CSR_HPMCOUNTER7H 0xc87
#define CSR_HPMCOUNTER8H 0xc88
#define CSR_HPMCOUNTER9H 0xc89
#define CSR_HPMCOUNTER10H 0xc8a
#define CSR_HPMCOUNTER11H 0xc8b
#define CSR_HPMCOUNTER12H 0xc8c
#define CSR_HPMCOUNTER13H 0xc8d
#define CSR_HPMCOUNTER14H 0xc8e
#define CSR_HPMCOUNTER15H 0xc8f
#define CSR_HPMCOUNTER16H 0xc90
#define CSR_HPMCOUNTER17H 0xc91
#define CSR_HPMCOUNTER18H 0xc92
#define CSR_HPMCOUNTER19H 0xc93
#define CSR_HPMCOUNTER20H 0xc94
#define CSR_HPMCOUNTER21H 0xc95
#define CSR_HPMCOUNTER22H 0xc96
#define CSR_HPMCOUNTER23H 0xc97
#define CSR_HPMCOUNTER24H 0xc98
#define CSR_HPMCOUNTER25H 0xc99
#define CSR_HPMCOUNTER26H 0xc9a
#define CSR_HPMCOUNTER27H 0xc9b
#define CSR_HPMCOUNTER28H 0xc9c
#define CSR_HPMCOUNTER29H 0xc9d
#define CSR_HPMCOUNTER30H 0xc9e
#define CSR_HPMCOUNTER31H 0xc9f
#define CSR_MCYCLEH 0xb80
#define CSR_MINSTRETH 0xb82
#define CSR_MHPMCOUNTER3H 0xb83
#define CSR_MHPMCOUNTER4H 0xb84
#define CSR_MHPMCOUNTER5H 0xb85
#define CSR_MHPMCOUNTER6H 0xb86
#define CSR_MHPMCOUNTER7H 0xb87
#define CSR_MHPMCOUNTER8H 0xb88
#define CSR_MHPMCOUNTER9H 0xb89
#define CSR_MHPMCOUNTER10H 0xb8a
#define CSR_MHPMCOUNTER11H 0xb8b
#define CSR_MHPMCOUNTER12H 0xb8c
#define CSR_MHPMCOUNTER13H 0xb8d
#define CSR_MHPMCOUNTER14H 0xb8e
#define CSR_MHPMCOUNTER15H 0xb8f
#define CSR_MHPMCOUNTER16H 0xb90
#define CSR_MHPMCOUNTER17H 0xb91
#define CSR_MHPMCOUNTER18H 0xb92
#define CSR_MHPMCOUNTER19H 0xb93
#define CSR_MHPMCOUNTER20H 0xb94
#define CSR_MHPMCOUNTER21H 0xb95
#define CSR_MHPMCOUNTER22H 0xb96
#define CSR_MHPMCOUNTER23H 0xb97
#define CSR_MHPMCOUNTER24H 0xb98
#define CSR_MHPMCOUNTER25H 0xb99
#define CSR_MHPMCOUNTER26H 0xb9a
#define CSR_MHPMCOUNTER27H 0xb9b
#define CSR_MHPMCOUNTER28H 0xb9c
#define CSR_MHPMCOUNTER29H 0xb9d
#define CSR_MHPMCOUNTER30H 0xb9e
#define CSR_MHPMCOUNTER31H 0xb9f
/* ------------------------------------------------------------------
 * 异常/中断原因号（填进 scause/mcause 的低位）。
 *    最高位表示类型：0 = 异常（exception），1 = 中断（interrupt）。
 *    lab1 最相关的一条是 CAUSE_MACHINE_ECALL = 0xb：内核执行 ecall 后，
 *    OpenSBI 在 mcause 里看到的正是这个值，于是知道"内核来请求 SBI 服务"。
 *    CAUSE_SUPERVISOR_ECALL = 0x9 则是 lab4 之后的系统调用入口：
 *    应用在 U 态执行 ecall，内核在 S 态看到这个原因号，就知道"用户要系统调用"。
 *    ------------------------------------------------------------------ */
#define CAUSE_MISALIGNED_FETCH 0x0  // 0x0 = 取指地址未对齐
#define CAUSE_FAULT_FETCH 0x1  // 0x1 = 取指访问错误
#define CAUSE_ILLEGAL_INSTRUCTION 0x2  // 0x2 = 非法指令
#define CAUSE_BREAKPOINT 0x3  // 0x3 = 断点（ebreak）
#define CAUSE_MISALIGNED_LOAD 0x4  // 0x4 = 取数地址未对齐
#define CAUSE_FAULT_LOAD 0x5  // 0x5 = 取数访问错误
#define CAUSE_MISALIGNED_STORE 0x6  // 0x6 = 存数地址未对齐
#define CAUSE_FAULT_STORE 0x7  // 0x7 = 存数访问错误
#define CAUSE_USER_ECALL 0x8  // 0x8 = U 态 ecall（将来系统调用的入口）
#define CAUSE_SUPERVISOR_ECALL 0x9  // 0x9 = S 态 ecall（主动陷入 M 态，如 SBI 服务请求）
#define CAUSE_HYPERVISOR_ECALL 0xa  // 0xa = H 态 ecall
#define CAUSE_MACHINE_ECALL 0xb  // 0xb = M 态 ecall（对应 sbi.c 里的 ecall 指令）
#endif
/* 以下 DECLARE_INSN 列表把"MASK/MATCH 模板"和"指令名字"绑定起来，
 * 供反汇编器/模拟器查表；没有任何人定义 DECLARE_INSN 时它整段为空。*/
#ifdef DECLARE_INSN
DECLARE_INSN(beq, MATCH_BEQ, MASK_BEQ)
DECLARE_INSN(bne, MATCH_BNE, MASK_BNE)
DECLARE_INSN(blt, MATCH_BLT, MASK_BLT)
DECLARE_INSN(bge, MATCH_BGE, MASK_BGE)
DECLARE_INSN(bltu, MATCH_BLTU, MASK_BLTU)
DECLARE_INSN(bgeu, MATCH_BGEU, MASK_BGEU)
DECLARE_INSN(jalr, MATCH_JALR, MASK_JALR)
DECLARE_INSN(jal, MATCH_JAL, MASK_JAL)
DECLARE_INSN(lui, MATCH_LUI, MASK_LUI)
DECLARE_INSN(auipc, MATCH_AUIPC, MASK_AUIPC)
DECLARE_INSN(addi, MATCH_ADDI, MASK_ADDI)
DECLARE_INSN(slli, MATCH_SLLI, MASK_SLLI)
DECLARE_INSN(slti, MATCH_SLTI, MASK_SLTI)
DECLARE_INSN(sltiu, MATCH_SLTIU, MASK_SLTIU)
DECLARE_INSN(xori, MATCH_XORI, MASK_XORI)
DECLARE_INSN(srli, MATCH_SRLI, MASK_SRLI)
DECLARE_INSN(srai, MATCH_SRAI, MASK_SRAI)
DECLARE_INSN(ori, MATCH_ORI, MASK_ORI)
DECLARE_INSN(andi, MATCH_ANDI, MASK_ANDI)
DECLARE_INSN(add, MATCH_ADD, MASK_ADD)
DECLARE_INSN(sub, MATCH_SUB, MASK_SUB)
DECLARE_INSN(sll, MATCH_SLL, MASK_SLL)
DECLARE_INSN(slt, MATCH_SLT, MASK_SLT)
DECLARE_INSN(sltu, MATCH_SLTU, MASK_SLTU)
DECLARE_INSN(xor, MATCH_XOR, MASK_XOR)
DECLARE_INSN(srl, MATCH_SRL, MASK_SRL)
DECLARE_INSN(sra, MATCH_SRA, MASK_SRA)
DECLARE_INSN(or, MATCH_OR, MASK_OR)
DECLARE_INSN(and, MATCH_AND, MASK_AND)
DECLARE_INSN(addiw, MATCH_ADDIW, MASK_ADDIW)
DECLARE_INSN(slliw, MATCH_SLLIW, MASK_SLLIW)
DECLARE_INSN(srliw, MATCH_SRLIW, MASK_SRLIW)
DECLARE_INSN(sraiw, MATCH_SRAIW, MASK_SRAIW)
DECLARE_INSN(addw, MATCH_ADDW, MASK_ADDW)
DECLARE_INSN(subw, MATCH_SUBW, MASK_SUBW)
DECLARE_INSN(sllw, MATCH_SLLW, MASK_SLLW)
DECLARE_INSN(srlw, MATCH_SRLW, MASK_SRLW)
DECLARE_INSN(sraw, MATCH_SRAW, MASK_SRAW)
DECLARE_INSN(lb, MATCH_LB, MASK_LB)
DECLARE_INSN(lh, MATCH_LH, MASK_LH)
DECLARE_INSN(lw, MATCH_LW, MASK_LW)
DECLARE_INSN(ld, MATCH_LD, MASK_LD)
DECLARE_INSN(lbu, MATCH_LBU, MASK_LBU)
DECLARE_INSN(lhu, MATCH_LHU, MASK_LHU)
DECLARE_INSN(lwu, MATCH_LWU, MASK_LWU)
DECLARE_INSN(sb, MATCH_SB, MASK_SB)
DECLARE_INSN(sh, MATCH_SH, MASK_SH)
DECLARE_INSN(sw, MATCH_SW, MASK_SW)
DECLARE_INSN(sd, MATCH_SD, MASK_SD)
DECLARE_INSN(fence, MATCH_FENCE, MASK_FENCE)
DECLARE_INSN(fence_i, MATCH_FENCE_I, MASK_FENCE_I)
DECLARE_INSN(mul, MATCH_MUL, MASK_MUL)
DECLARE_INSN(mulh, MATCH_MULH, MASK_MULH)
DECLARE_INSN(mulhsu, MATCH_MULHSU, MASK_MULHSU)
DECLARE_INSN(mulhu, MATCH_MULHU, MASK_MULHU)
DECLARE_INSN(div, MATCH_DIV, MASK_DIV)
DECLARE_INSN(divu, MATCH_DIVU, MASK_DIVU)
DECLARE_INSN(rem, MATCH_REM, MASK_REM)
DECLARE_INSN(remu, MATCH_REMU, MASK_REMU)
DECLARE_INSN(mulw, MATCH_MULW, MASK_MULW)
DECLARE_INSN(divw, MATCH_DIVW, MASK_DIVW)
DECLARE_INSN(divuw, MATCH_DIVUW, MASK_DIVUW)
DECLARE_INSN(remw, MATCH_REMW, MASK_REMW)
DECLARE_INSN(remuw, MATCH_REMUW, MASK_REMUW)
DECLARE_INSN(amoadd_w, MATCH_AMOADD_W, MASK_AMOADD_W)
DECLARE_INSN(amoxor_w, MATCH_AMOXOR_W, MASK_AMOXOR_W)
DECLARE_INSN(amoor_w, MATCH_AMOOR_W, MASK_AMOOR_W)
DECLARE_INSN(amoand_w, MATCH_AMOAND_W, MASK_AMOAND_W)
DECLARE_INSN(amomin_w, MATCH_AMOMIN_W, MASK_AMOMIN_W)
DECLARE_INSN(amomax_w, MATCH_AMOMAX_W, MASK_AMOMAX_W)
DECLARE_INSN(amominu_w, MATCH_AMOMINU_W, MASK_AMOMINU_W)
DECLARE_INSN(amomaxu_w, MATCH_AMOMAXU_W, MASK_AMOMAXU_W)
DECLARE_INSN(amoswap_w, MATCH_AMOSWAP_W, MASK_AMOSWAP_W)
DECLARE_INSN(lr_w, MATCH_LR_W, MASK_LR_W)
DECLARE_INSN(sc_w, MATCH_SC_W, MASK_SC_W)
DECLARE_INSN(amoadd_d, MATCH_AMOADD_D, MASK_AMOADD_D)
DECLARE_INSN(amoxor_d, MATCH_AMOXOR_D, MASK_AMOXOR_D)
DECLARE_INSN(amoor_d, MATCH_AMOOR_D, MASK_AMOOR_D)
DECLARE_INSN(amoand_d, MATCH_AMOAND_D, MASK_AMOAND_D)
DECLARE_INSN(amomin_d, MATCH_AMOMIN_D, MASK_AMOMIN_D)
DECLARE_INSN(amomax_d, MATCH_AMOMAX_D, MASK_AMOMAX_D)
DECLARE_INSN(amominu_d, MATCH_AMOMINU_D, MASK_AMOMINU_D)
DECLARE_INSN(amomaxu_d, MATCH_AMOMAXU_D, MASK_AMOMAXU_D)
DECLARE_INSN(amoswap_d, MATCH_AMOSWAP_D, MASK_AMOSWAP_D)
DECLARE_INSN(lr_d, MATCH_LR_D, MASK_LR_D)
DECLARE_INSN(sc_d, MATCH_SC_D, MASK_SC_D)
DECLARE_INSN(ecall, MATCH_ECALL, MASK_ECALL)
DECLARE_INSN(ebreak, MATCH_EBREAK, MASK_EBREAK)
DECLARE_INSN(uret, MATCH_URET, MASK_URET)
DECLARE_INSN(sret, MATCH_SRET, MASK_SRET)
DECLARE_INSN(hret, MATCH_HRET, MASK_HRET)
DECLARE_INSN(mret, MATCH_MRET, MASK_MRET)
DECLARE_INSN(dret, MATCH_DRET, MASK_DRET)
DECLARE_INSN(sfence_vm, MATCH_SFENCE_VM, MASK_SFENCE_VM)
DECLARE_INSN(wfi, MATCH_WFI, MASK_WFI)
DECLARE_INSN(csrrw, MATCH_CSRRW, MASK_CSRRW)
DECLARE_INSN(csrrs, MATCH_CSRRS, MASK_CSRRS)
DECLARE_INSN(csrrc, MATCH_CSRRC, MASK_CSRRC)
DECLARE_INSN(csrrwi, MATCH_CSRRWI, MASK_CSRRWI)
DECLARE_INSN(csrrsi, MATCH_CSRRSI, MASK_CSRRSI)
DECLARE_INSN(csrrci, MATCH_CSRRCI, MASK_CSRRCI)
DECLARE_INSN(fadd_s, MATCH_FADD_S, MASK_FADD_S)
DECLARE_INSN(fsub_s, MATCH_FSUB_S, MASK_FSUB_S)
DECLARE_INSN(fmul_s, MATCH_FMUL_S, MASK_FMUL_S)
DECLARE_INSN(fdiv_s, MATCH_FDIV_S, MASK_FDIV_S)
DECLARE_INSN(fsgnj_s, MATCH_FSGNJ_S, MASK_FSGNJ_S)
DECLARE_INSN(fsgnjn_s, MATCH_FSGNJN_S, MASK_FSGNJN_S)
DECLARE_INSN(fsgnjx_s, MATCH_FSGNJX_S, MASK_FSGNJX_S)
DECLARE_INSN(fmin_s, MATCH_FMIN_S, MASK_FMIN_S)
DECLARE_INSN(fmax_s, MATCH_FMAX_S, MASK_FMAX_S)
DECLARE_INSN(fsqrt_s, MATCH_FSQRT_S, MASK_FSQRT_S)
DECLARE_INSN(fadd_d, MATCH_FADD_D, MASK_FADD_D)
DECLARE_INSN(fsub_d, MATCH_FSUB_D, MASK_FSUB_D)
DECLARE_INSN(fmul_d, MATCH_FMUL_D, MASK_FMUL_D)
DECLARE_INSN(fdiv_d, MATCH_FDIV_D, MASK_FDIV_D)
DECLARE_INSN(fsgnj_d, MATCH_FSGNJ_D, MASK_FSGNJ_D)
DECLARE_INSN(fsgnjn_d, MATCH_FSGNJN_D, MASK_FSGNJN_D)
DECLARE_INSN(fsgnjx_d, MATCH_FSGNJX_D, MASK_FSGNJX_D)
DECLARE_INSN(fmin_d, MATCH_FMIN_D, MASK_FMIN_D)
DECLARE_INSN(fmax_d, MATCH_FMAX_D, MASK_FMAX_D)
DECLARE_INSN(fcvt_s_d, MATCH_FCVT_S_D, MASK_FCVT_S_D)
DECLARE_INSN(fcvt_d_s, MATCH_FCVT_D_S, MASK_FCVT_D_S)
DECLARE_INSN(fsqrt_d, MATCH_FSQRT_D, MASK_FSQRT_D)
DECLARE_INSN(fle_s, MATCH_FLE_S, MASK_FLE_S)
DECLARE_INSN(flt_s, MATCH_FLT_S, MASK_FLT_S)
DECLARE_INSN(feq_s, MATCH_FEQ_S, MASK_FEQ_S)
DECLARE_INSN(fle_d, MATCH_FLE_D, MASK_FLE_D)
DECLARE_INSN(flt_d, MATCH_FLT_D, MASK_FLT_D)
DECLARE_INSN(feq_d, MATCH_FEQ_D, MASK_FEQ_D)
DECLARE_INSN(fcvt_w_s, MATCH_FCVT_W_S, MASK_FCVT_W_S)
DECLARE_INSN(fcvt_wu_s, MATCH_FCVT_WU_S, MASK_FCVT_WU_S)
DECLARE_INSN(fcvt_l_s, MATCH_FCVT_L_S, MASK_FCVT_L_S)
DECLARE_INSN(fcvt_lu_s, MATCH_FCVT_LU_S, MASK_FCVT_LU_S)
DECLARE_INSN(fmv_x_s, MATCH_FMV_X_S, MASK_FMV_X_S)
DECLARE_INSN(fclass_s, MATCH_FCLASS_S, MASK_FCLASS_S)
DECLARE_INSN(fcvt_w_d, MATCH_FCVT_W_D, MASK_FCVT_W_D)
DECLARE_INSN(fcvt_wu_d, MATCH_FCVT_WU_D, MASK_FCVT_WU_D)
DECLARE_INSN(fcvt_l_d, MATCH_FCVT_L_D, MASK_FCVT_L_D)
DECLARE_INSN(fcvt_lu_d, MATCH_FCVT_LU_D, MASK_FCVT_LU_D)
DECLARE_INSN(fmv_x_d, MATCH_FMV_X_D, MASK_FMV_X_D)
DECLARE_INSN(fclass_d, MATCH_FCLASS_D, MASK_FCLASS_D)
DECLARE_INSN(fcvt_s_w, MATCH_FCVT_S_W, MASK_FCVT_S_W)
DECLARE_INSN(fcvt_s_wu, MATCH_FCVT_S_WU, MASK_FCVT_S_WU)
DECLARE_INSN(fcvt_s_l, MATCH_FCVT_S_L, MASK_FCVT_S_L)
DECLARE_INSN(fcvt_s_lu, MATCH_FCVT_S_LU, MASK_FCVT_S_LU)
DECLARE_INSN(fmv_s_x, MATCH_FMV_S_X, MASK_FMV_S_X)
DECLARE_INSN(fcvt_d_w, MATCH_FCVT_D_W, MASK_FCVT_D_W)
DECLARE_INSN(fcvt_d_wu, MATCH_FCVT_D_WU, MASK_FCVT_D_WU)
DECLARE_INSN(fcvt_d_l, MATCH_FCVT_D_L, MASK_FCVT_D_L)
DECLARE_INSN(fcvt_d_lu, MATCH_FCVT_D_LU, MASK_FCVT_D_LU)
DECLARE_INSN(fmv_d_x, MATCH_FMV_D_X, MASK_FMV_D_X)
DECLARE_INSN(flw, MATCH_FLW, MASK_FLW)
DECLARE_INSN(fld, MATCH_FLD, MASK_FLD)
DECLARE_INSN(fsw, MATCH_FSW, MASK_FSW)
DECLARE_INSN(fsd, MATCH_FSD, MASK_FSD)
DECLARE_INSN(fmadd_s, MATCH_FMADD_S, MASK_FMADD_S)
DECLARE_INSN(fmsub_s, MATCH_FMSUB_S, MASK_FMSUB_S)
DECLARE_INSN(fnmsub_s, MATCH_FNMSUB_S, MASK_FNMSUB_S)
DECLARE_INSN(fnmadd_s, MATCH_FNMADD_S, MASK_FNMADD_S)
DECLARE_INSN(fmadd_d, MATCH_FMADD_D, MASK_FMADD_D)
DECLARE_INSN(fmsub_d, MATCH_FMSUB_D, MASK_FMSUB_D)
DECLARE_INSN(fnmsub_d, MATCH_FNMSUB_D, MASK_FNMSUB_D)
DECLARE_INSN(fnmadd_d, MATCH_FNMADD_D, MASK_FNMADD_D)
DECLARE_INSN(c_nop, MATCH_C_NOP, MASK_C_NOP)
DECLARE_INSN(c_addi16sp, MATCH_C_ADDI16SP, MASK_C_ADDI16SP)
DECLARE_INSN(c_jr, MATCH_C_JR, MASK_C_JR)
DECLARE_INSN(c_jalr, MATCH_C_JALR, MASK_C_JALR)
DECLARE_INSN(c_ebreak, MATCH_C_EBREAK, MASK_C_EBREAK)
DECLARE_INSN(c_ld, MATCH_C_LD, MASK_C_LD)
DECLARE_INSN(c_sd, MATCH_C_SD, MASK_C_SD)
DECLARE_INSN(c_addiw, MATCH_C_ADDIW, MASK_C_ADDIW)
DECLARE_INSN(c_ldsp, MATCH_C_LDSP, MASK_C_LDSP)
DECLARE_INSN(c_sdsp, MATCH_C_SDSP, MASK_C_SDSP)
DECLARE_INSN(c_addi4spn, MATCH_C_ADDI4SPN, MASK_C_ADDI4SPN)
DECLARE_INSN(c_fld, MATCH_C_FLD, MASK_C_FLD)
DECLARE_INSN(c_lw, MATCH_C_LW, MASK_C_LW)
DECLARE_INSN(c_flw, MATCH_C_FLW, MASK_C_FLW)
DECLARE_INSN(c_fsd, MATCH_C_FSD, MASK_C_FSD)
DECLARE_INSN(c_sw, MATCH_C_SW, MASK_C_SW)
DECLARE_INSN(c_fsw, MATCH_C_FSW, MASK_C_FSW)
DECLARE_INSN(c_addi, MATCH_C_ADDI, MASK_C_ADDI)
DECLARE_INSN(c_jal, MATCH_C_JAL, MASK_C_JAL)
DECLARE_INSN(c_li, MATCH_C_LI, MASK_C_LI)
DECLARE_INSN(c_lui, MATCH_C_LUI, MASK_C_LUI)
DECLARE_INSN(c_srli, MATCH_C_SRLI, MASK_C_SRLI)
DECLARE_INSN(c_srai, MATCH_C_SRAI, MASK_C_SRAI)
DECLARE_INSN(c_andi, MATCH_C_ANDI, MASK_C_ANDI)
DECLARE_INSN(c_sub, MATCH_C_SUB, MASK_C_SUB)
DECLARE_INSN(c_xor, MATCH_C_XOR, MASK_C_XOR)
DECLARE_INSN(c_or, MATCH_C_OR, MASK_C_OR)
DECLARE_INSN(c_and, MATCH_C_AND, MASK_C_AND)
DECLARE_INSN(c_subw, MATCH_C_SUBW, MASK_C_SUBW)
DECLARE_INSN(c_addw, MATCH_C_ADDW, MASK_C_ADDW)
DECLARE_INSN(c_j, MATCH_C_J, MASK_C_J)
DECLARE_INSN(c_beqz, MATCH_C_BEQZ, MASK_C_BEQZ)
DECLARE_INSN(c_bnez, MATCH_C_BNEZ, MASK_C_BNEZ)
DECLARE_INSN(c_slli, MATCH_C_SLLI, MASK_C_SLLI)
DECLARE_INSN(c_fldsp, MATCH_C_FLDSP, MASK_C_FLDSP)
DECLARE_INSN(c_lwsp, MATCH_C_LWSP, MASK_C_LWSP)
DECLARE_INSN(c_flwsp, MATCH_C_FLWSP, MASK_C_FLWSP)
DECLARE_INSN(c_mv, MATCH_C_MV, MASK_C_MV)
DECLARE_INSN(c_add, MATCH_C_ADD, MASK_C_ADD)
DECLARE_INSN(c_fsdsp, MATCH_C_FSDSP, MASK_C_FSDSP)
DECLARE_INSN(c_swsp, MATCH_C_SWSP, MASK_C_SWSP)
DECLARE_INSN(c_fswsp, MATCH_C_FSWSP, MASK_C_FSWSP)
DECLARE_INSN(custom0, MATCH_CUSTOM0, MASK_CUSTOM0)
DECLARE_INSN(custom0_rs1, MATCH_CUSTOM0_RS1, MASK_CUSTOM0_RS1)
DECLARE_INSN(custom0_rs1_rs2, MATCH_CUSTOM0_RS1_RS2, MASK_CUSTOM0_RS1_RS2)
DECLARE_INSN(custom0_rd, MATCH_CUSTOM0_RD, MASK_CUSTOM0_RD)
DECLARE_INSN(custom0_rd_rs1, MATCH_CUSTOM0_RD_RS1, MASK_CUSTOM0_RD_RS1)
DECLARE_INSN(custom0_rd_rs1_rs2, MATCH_CUSTOM0_RD_RS1_RS2, MASK_CUSTOM0_RD_RS1_RS2)
DECLARE_INSN(custom1, MATCH_CUSTOM1, MASK_CUSTOM1)
DECLARE_INSN(custom1_rs1, MATCH_CUSTOM1_RS1, MASK_CUSTOM1_RS1)
DECLARE_INSN(custom1_rs1_rs2, MATCH_CUSTOM1_RS1_RS2, MASK_CUSTOM1_RS1_RS2)
DECLARE_INSN(custom1_rd, MATCH_CUSTOM1_RD, MASK_CUSTOM1_RD)
DECLARE_INSN(custom1_rd_rs1, MATCH_CUSTOM1_RD_RS1, MASK_CUSTOM1_RD_RS1)
DECLARE_INSN(custom1_rd_rs1_rs2, MATCH_CUSTOM1_RD_RS1_RS2, MASK_CUSTOM1_RD_RS1_RS2)
DECLARE_INSN(custom2, MATCH_CUSTOM2, MASK_CUSTOM2)
DECLARE_INSN(custom2_rs1, MATCH_CUSTOM2_RS1, MASK_CUSTOM2_RS1)
DECLARE_INSN(custom2_rs1_rs2, MATCH_CUSTOM2_RS1_RS2, MASK_CUSTOM2_RS1_RS2)
DECLARE_INSN(custom2_rd, MATCH_CUSTOM2_RD, MASK_CUSTOM2_RD)
DECLARE_INSN(custom2_rd_rs1, MATCH_CUSTOM2_RD_RS1, MASK_CUSTOM2_RD_RS1)
DECLARE_INSN(custom2_rd_rs1_rs2, MATCH_CUSTOM2_RD_RS1_RS2, MASK_CUSTOM2_RD_RS1_RS2)
DECLARE_INSN(custom3, MATCH_CUSTOM3, MASK_CUSTOM3)
DECLARE_INSN(custom3_rs1, MATCH_CUSTOM3_RS1, MASK_CUSTOM3_RS1)
DECLARE_INSN(custom3_rs1_rs2, MATCH_CUSTOM3_RS1_RS2, MASK_CUSTOM3_RS1_RS2)
DECLARE_INSN(custom3_rd, MATCH_CUSTOM3_RD, MASK_CUSTOM3_RD)
DECLARE_INSN(custom3_rd_rs1, MATCH_CUSTOM3_RD_RS1, MASK_CUSTOM3_RD_RS1)
DECLARE_INSN(custom3_rd_rs1_rs2, MATCH_CUSTOM3_RD_RS1_RS2, MASK_CUSTOM3_RD_RS1_RS2)
#endif
/* 以下 DECLARE_CSR 列表把"CSR 编号"和"CSR 名字"绑定起来，
 * 本质就是上面 CSR_xxx 编号表的另一种表述形式。*/
#ifdef DECLARE_CSR
DECLARE_CSR(fflags, CSR_FFLAGS)
DECLARE_CSR(frm, CSR_FRM)
DECLARE_CSR(fcsr, CSR_FCSR)
DECLARE_CSR(cycle, CSR_CYCLE)
DECLARE_CSR(time, CSR_TIME)
DECLARE_CSR(instret, CSR_INSTRET)
DECLARE_CSR(hpmcounter3, CSR_HPMCOUNTER3)
DECLARE_CSR(hpmcounter4, CSR_HPMCOUNTER4)
DECLARE_CSR(hpmcounter5, CSR_HPMCOUNTER5)
DECLARE_CSR(hpmcounter6, CSR_HPMCOUNTER6)
DECLARE_CSR(hpmcounter7, CSR_HPMCOUNTER7)
DECLARE_CSR(hpmcounter8, CSR_HPMCOUNTER8)
DECLARE_CSR(hpmcounter9, CSR_HPMCOUNTER9)
DECLARE_CSR(hpmcounter10, CSR_HPMCOUNTER10)
DECLARE_CSR(hpmcounter11, CSR_HPMCOUNTER11)
DECLARE_CSR(hpmcounter12, CSR_HPMCOUNTER12)
DECLARE_CSR(hpmcounter13, CSR_HPMCOUNTER13)
DECLARE_CSR(hpmcounter14, CSR_HPMCOUNTER14)
DECLARE_CSR(hpmcounter15, CSR_HPMCOUNTER15)
DECLARE_CSR(hpmcounter16, CSR_HPMCOUNTER16)
DECLARE_CSR(hpmcounter17, CSR_HPMCOUNTER17)
DECLARE_CSR(hpmcounter18, CSR_HPMCOUNTER18)
DECLARE_CSR(hpmcounter19, CSR_HPMCOUNTER19)
DECLARE_CSR(hpmcounter20, CSR_HPMCOUNTER20)
DECLARE_CSR(hpmcounter21, CSR_HPMCOUNTER21)
DECLARE_CSR(hpmcounter22, CSR_HPMCOUNTER22)
DECLARE_CSR(hpmcounter23, CSR_HPMCOUNTER23)
DECLARE_CSR(hpmcounter24, CSR_HPMCOUNTER24)
DECLARE_CSR(hpmcounter25, CSR_HPMCOUNTER25)
DECLARE_CSR(hpmcounter26, CSR_HPMCOUNTER26)
DECLARE_CSR(hpmcounter27, CSR_HPMCOUNTER27)
DECLARE_CSR(hpmcounter28, CSR_HPMCOUNTER28)
DECLARE_CSR(hpmcounter29, CSR_HPMCOUNTER29)
DECLARE_CSR(hpmcounter30, CSR_HPMCOUNTER30)
DECLARE_CSR(hpmcounter31, CSR_HPMCOUNTER31)
DECLARE_CSR(sstatus, CSR_SSTATUS)
DECLARE_CSR(sie, CSR_SIE)
DECLARE_CSR(stvec, CSR_STVEC)
DECLARE_CSR(sscratch, CSR_SSCRATCH)
DECLARE_CSR(sepc, CSR_SEPC)
DECLARE_CSR(scause, CSR_SCAUSE)
DECLARE_CSR(sbadaddr, CSR_SBADADDR)
DECLARE_CSR(sip, CSR_SIP)
DECLARE_CSR(sptbr, CSR_SPTBR)
DECLARE_CSR(mstatus, CSR_MSTATUS)
DECLARE_CSR(misa, CSR_MISA)
DECLARE_CSR(medeleg, CSR_MEDELEG)
DECLARE_CSR(mideleg, CSR_MIDELEG)
DECLARE_CSR(mie, CSR_MIE)
DECLARE_CSR(mtvec, CSR_MTVEC)
DECLARE_CSR(mscratch, CSR_MSCRATCH)
DECLARE_CSR(mepc, CSR_MEPC)
DECLARE_CSR(mcause, CSR_MCAUSE)
DECLARE_CSR(mbadaddr, CSR_MBADADDR)
DECLARE_CSR(mip, CSR_MIP)
DECLARE_CSR(tselect, CSR_TSELECT)
DECLARE_CSR(tdata1, CSR_TDATA1)
DECLARE_CSR(tdata2, CSR_TDATA2)
DECLARE_CSR(tdata3, CSR_TDATA3)
DECLARE_CSR(dcsr, CSR_DCSR)
DECLARE_CSR(dpc, CSR_DPC)
DECLARE_CSR(dscratch, CSR_DSCRATCH)
DECLARE_CSR(mcycle, CSR_MCYCLE)
DECLARE_CSR(minstret, CSR_MINSTRET)
DECLARE_CSR(mhpmcounter3, CSR_MHPMCOUNTER3)
DECLARE_CSR(mhpmcounter4, CSR_MHPMCOUNTER4)
DECLARE_CSR(mhpmcounter5, CSR_MHPMCOUNTER5)
DECLARE_CSR(mhpmcounter6, CSR_MHPMCOUNTER6)
DECLARE_CSR(mhpmcounter7, CSR_MHPMCOUNTER7)
DECLARE_CSR(mhpmcounter8, CSR_MHPMCOUNTER8)
DECLARE_CSR(mhpmcounter9, CSR_MHPMCOUNTER9)
DECLARE_CSR(mhpmcounter10, CSR_MHPMCOUNTER10)
DECLARE_CSR(mhpmcounter11, CSR_MHPMCOUNTER11)
DECLARE_CSR(mhpmcounter12, CSR_MHPMCOUNTER12)
DECLARE_CSR(mhpmcounter13, CSR_MHPMCOUNTER13)
DECLARE_CSR(mhpmcounter14, CSR_MHPMCOUNTER14)
DECLARE_CSR(mhpmcounter15, CSR_MHPMCOUNTER15)
DECLARE_CSR(mhpmcounter16, CSR_MHPMCOUNTER16)
DECLARE_CSR(mhpmcounter17, CSR_MHPMCOUNTER17)
DECLARE_CSR(mhpmcounter18, CSR_MHPMCOUNTER18)
DECLARE_CSR(mhpmcounter19, CSR_MHPMCOUNTER19)
DECLARE_CSR(mhpmcounter20, CSR_MHPMCOUNTER20)
DECLARE_CSR(mhpmcounter21, CSR_MHPMCOUNTER21)
DECLARE_CSR(mhpmcounter22, CSR_MHPMCOUNTER22)
DECLARE_CSR(mhpmcounter23, CSR_MHPMCOUNTER23)
DECLARE_CSR(mhpmcounter24, CSR_MHPMCOUNTER24)
DECLARE_CSR(mhpmcounter25, CSR_MHPMCOUNTER25)
DECLARE_CSR(mhpmcounter26, CSR_MHPMCOUNTER26)
DECLARE_CSR(mhpmcounter27, CSR_MHPMCOUNTER27)
DECLARE_CSR(mhpmcounter28, CSR_MHPMCOUNTER28)
DECLARE_CSR(mhpmcounter29, CSR_MHPMCOUNTER29)
DECLARE_CSR(mhpmcounter30, CSR_MHPMCOUNTER30)
DECLARE_CSR(mhpmcounter31, CSR_MHPMCOUNTER31)
DECLARE_CSR(mucounteren, CSR_MUCOUNTEREN)
DECLARE_CSR(mscounteren, CSR_MSCOUNTEREN)
DECLARE_CSR(mhpmevent3, CSR_MHPMEVENT3)
DECLARE_CSR(mhpmevent4, CSR_MHPMEVENT4)
DECLARE_CSR(mhpmevent5, CSR_MHPMEVENT5)
DECLARE_CSR(mhpmevent6, CSR_MHPMEVENT6)
DECLARE_CSR(mhpmevent7, CSR_MHPMEVENT7)
DECLARE_CSR(mhpmevent8, CSR_MHPMEVENT8)
DECLARE_CSR(mhpmevent9, CSR_MHPMEVENT9)
DECLARE_CSR(mhpmevent10, CSR_MHPMEVENT10)
DECLARE_CSR(mhpmevent11, CSR_MHPMEVENT11)
DECLARE_CSR(mhpmevent12, CSR_MHPMEVENT12)
DECLARE_CSR(mhpmevent13, CSR_MHPMEVENT13)
DECLARE_CSR(mhpmevent14, CSR_MHPMEVENT14)
DECLARE_CSR(mhpmevent15, CSR_MHPMEVENT15)
DECLARE_CSR(mhpmevent16, CSR_MHPMEVENT16)
DECLARE_CSR(mhpmevent17, CSR_MHPMEVENT17)
DECLARE_CSR(mhpmevent18, CSR_MHPMEVENT18)
DECLARE_CSR(mhpmevent19, CSR_MHPMEVENT19)
DECLARE_CSR(mhpmevent20, CSR_MHPMEVENT20)
DECLARE_CSR(mhpmevent21, CSR_MHPMEVENT21)
DECLARE_CSR(mhpmevent22, CSR_MHPMEVENT22)
DECLARE_CSR(mhpmevent23, CSR_MHPMEVENT23)
DECLARE_CSR(mhpmevent24, CSR_MHPMEVENT24)
DECLARE_CSR(mhpmevent25, CSR_MHPMEVENT25)
DECLARE_CSR(mhpmevent26, CSR_MHPMEVENT26)
DECLARE_CSR(mhpmevent27, CSR_MHPMEVENT27)
DECLARE_CSR(mhpmevent28, CSR_MHPMEVENT28)
DECLARE_CSR(mhpmevent29, CSR_MHPMEVENT29)
DECLARE_CSR(mhpmevent30, CSR_MHPMEVENT30)
DECLARE_CSR(mhpmevent31, CSR_MHPMEVENT31)
DECLARE_CSR(mvendorid, CSR_MVENDORID)
DECLARE_CSR(marchid, CSR_MARCHID)
DECLARE_CSR(mimpid, CSR_MIMPID)
DECLARE_CSR(mhartid, CSR_MHARTID)
DECLARE_CSR(cycleh, CSR_CYCLEH)
DECLARE_CSR(timeh, CSR_TIMEH)
DECLARE_CSR(instreth, CSR_INSTRETH)
DECLARE_CSR(hpmcounter3h, CSR_HPMCOUNTER3H)
DECLARE_CSR(hpmcounter4h, CSR_HPMCOUNTER4H)
DECLARE_CSR(hpmcounter5h, CSR_HPMCOUNTER5H)
DECLARE_CSR(hpmcounter6h, CSR_HPMCOUNTER6H)
DECLARE_CSR(hpmcounter7h, CSR_HPMCOUNTER7H)
DECLARE_CSR(hpmcounter8h, CSR_HPMCOUNTER8H)
DECLARE_CSR(hpmcounter9h, CSR_HPMCOUNTER9H)
DECLARE_CSR(hpmcounter10h, CSR_HPMCOUNTER10H)
DECLARE_CSR(hpmcounter11h, CSR_HPMCOUNTER11H)
DECLARE_CSR(hpmcounter12h, CSR_HPMCOUNTER12H)
DECLARE_CSR(hpmcounter13h, CSR_HPMCOUNTER13H)
DECLARE_CSR(hpmcounter14h, CSR_HPMCOUNTER14H)
DECLARE_CSR(hpmcounter15h, CSR_HPMCOUNTER15H)
DECLARE_CSR(hpmcounter16h, CSR_HPMCOUNTER16H)
DECLARE_CSR(hpmcounter17h, CSR_HPMCOUNTER17H)
DECLARE_CSR(hpmcounter18h, CSR_HPMCOUNTER18H)
DECLARE_CSR(hpmcounter19h, CSR_HPMCOUNTER19H)
DECLARE_CSR(hpmcounter20h, CSR_HPMCOUNTER20H)
DECLARE_CSR(hpmcounter21h, CSR_HPMCOUNTER21H)
DECLARE_CSR(hpmcounter22h, CSR_HPMCOUNTER22H)
DECLARE_CSR(hpmcounter23h, CSR_HPMCOUNTER23H)
DECLARE_CSR(hpmcounter24h, CSR_HPMCOUNTER24H)
DECLARE_CSR(hpmcounter25h, CSR_HPMCOUNTER25H)
DECLARE_CSR(hpmcounter26h, CSR_HPMCOUNTER26H)
DECLARE_CSR(hpmcounter27h, CSR_HPMCOUNTER27H)
DECLARE_CSR(hpmcounter28h, CSR_HPMCOUNTER28H)
DECLARE_CSR(hpmcounter29h, CSR_HPMCOUNTER29H)
DECLARE_CSR(hpmcounter30h, CSR_HPMCOUNTER30H)
DECLARE_CSR(hpmcounter31h, CSR_HPMCOUNTER31H)
DECLARE_CSR(mcycleh, CSR_MCYCLEH)
DECLARE_CSR(minstreth, CSR_MINSTRETH)
DECLARE_CSR(mhpmcounter3h, CSR_MHPMCOUNTER3H)
DECLARE_CSR(mhpmcounter4h, CSR_MHPMCOUNTER4H)
DECLARE_CSR(mhpmcounter5h, CSR_MHPMCOUNTER5H)
DECLARE_CSR(mhpmcounter6h, CSR_MHPMCOUNTER6H)
DECLARE_CSR(mhpmcounter7h, CSR_MHPMCOUNTER7H)
DECLARE_CSR(mhpmcounter8h, CSR_MHPMCOUNTER8H)
DECLARE_CSR(mhpmcounter9h, CSR_MHPMCOUNTER9H)
DECLARE_CSR(mhpmcounter10h, CSR_MHPMCOUNTER10H)
DECLARE_CSR(mhpmcounter11h, CSR_MHPMCOUNTER11H)
DECLARE_CSR(mhpmcounter12h, CSR_MHPMCOUNTER12H)
DECLARE_CSR(mhpmcounter13h, CSR_MHPMCOUNTER13H)
DECLARE_CSR(mhpmcounter14h, CSR_MHPMCOUNTER14H)
DECLARE_CSR(mhpmcounter15h, CSR_MHPMCOUNTER15H)
DECLARE_CSR(mhpmcounter16h, CSR_MHPMCOUNTER16H)
DECLARE_CSR(mhpmcounter17h, CSR_MHPMCOUNTER17H)
DECLARE_CSR(mhpmcounter18h, CSR_MHPMCOUNTER18H)
DECLARE_CSR(mhpmcounter19h, CSR_MHPMCOUNTER19H)
DECLARE_CSR(mhpmcounter20h, CSR_MHPMCOUNTER20H)
DECLARE_CSR(mhpmcounter21h, CSR_MHPMCOUNTER21H)
DECLARE_CSR(mhpmcounter22h, CSR_MHPMCOUNTER22H)
DECLARE_CSR(mhpmcounter23h, CSR_MHPMCOUNTER23H)
DECLARE_CSR(mhpmcounter24h, CSR_MHPMCOUNTER24H)
DECLARE_CSR(mhpmcounter25h, CSR_MHPMCOUNTER25H)
DECLARE_CSR(mhpmcounter26h, CSR_MHPMCOUNTER26H)
DECLARE_CSR(mhpmcounter27h, CSR_MHPMCOUNTER27H)
DECLARE_CSR(mhpmcounter28h, CSR_MHPMCOUNTER28H)
DECLARE_CSR(mhpmcounter29h, CSR_MHPMCOUNTER29H)
DECLARE_CSR(mhpmcounter30h, CSR_MHPMCOUNTER30H)
DECLARE_CSR(mhpmcounter31h, CSR_MHPMCOUNTER31H)
#endif
/* 以下 DECLARE_CAUSE 列表把"原因号"和"文字说明"绑定起来，
 * 异常发生时打印原因（后续实验的 do_pgfault / trap 处理会用到）。*/
#ifdef DECLARE_CAUSE
DECLARE_CAUSE("misaligned fetch", CAUSE_MISALIGNED_FETCH)
DECLARE_CAUSE("fault fetch", CAUSE_FAULT_FETCH)
DECLARE_CAUSE("illegal instruction", CAUSE_ILLEGAL_INSTRUCTION)
DECLARE_CAUSE("breakpoint", CAUSE_BREAKPOINT)
DECLARE_CAUSE("misaligned load", CAUSE_MISALIGNED_LOAD)
DECLARE_CAUSE("fault load", CAUSE_FAULT_LOAD)
DECLARE_CAUSE("misaligned store", CAUSE_MISALIGNED_STORE)
DECLARE_CAUSE("fault store", CAUSE_FAULT_STORE)
DECLARE_CAUSE("user_ecall", CAUSE_USER_ECALL)
DECLARE_CAUSE("supervisor_ecall", CAUSE_SUPERVISOR_ECALL)
DECLARE_CAUSE("hypervisor_ecall", CAUSE_HYPERVISOR_ECALL)
DECLARE_CAUSE("machine_ecall", CAUSE_MACHINE_ECALL)
#endif
