# ============================================================================
# Makefile —— lab1 的构建总控（答辩常问：make 到底做了什么？）
# ============================================================================
# 【三条命令，三个层次】
#   make        : 用交叉编译器把所有 .c/.S 编译成 .o，再链接出 ELF 内核
#                 bin/kernel，最后 objcopy 成纯二进制 bin/ucore.img。
#   make qemu   : 启动 QEMU 虚拟机，用 OpenSBI 当 BIOS，把 ucore.img 加载到
#                 0x80200000 并运行（看到 "(THU.CST) os is loading ..." 就成功）。
#   make debug + make gdb : 让 QEMU 停在第一条指令等待 GDB 连接，
#                 用于练习 2"跟踪从加电到内核第一条指令"。
# 【为什么要交叉编译】
#   我们的电脑是 x86，内核要跑在 RISC-V 上，所以必须用 riscv64-unknown-elf-
#   前缀的工具链编译出 RISC-V 机器码。
# ============================================================================
PROJ	:= lab1
EMPTY	:=
SPACE	:= $(EMPTY) $(EMPTY)
SLASH	:= /

# V := @ ：编译命令前加 @，make 就只打印 "+ cc xxx" 这样的简洁提示，
#   而不把完整的 gcc 命令行刷满屏幕。改成 V := 可以看完整命令。
V       := @

# 交叉工具链前缀：riscv64-unknown-elf-gcc / ld / objcopy / gdb 等。
#ifndef GCCPREFIX
GCCPREFIX := riscv64-unknown-elf-
#endif

# QEMU 可执行文件，默认 qemu-system-riscv64（模拟 64 位 RISC-V 整机）。
ifndef QEMU
QEMU := qemu-system-riscv64
endif


# 清掉 make 内置的 .c/.S 后缀推导规则，避免它用主机 gcc 误编译内核。
# eliminate default suffix rules
.SUFFIXES: .c .S .h

# 构建出错（或 Ctrl-C）时，自动删掉写了一半的目标文件，防止脏文件被误用。
# delete target files if there is an error (or make is interrupted)
.DELETE_ON_ERROR:

# define compiler and flags
# HOSTCC：主机上的 gcc，只用来编译 tools/ 下跑在主机上的小工具。
HOSTCC		:= gcc
HOSTCFLAGS	:= -Wall -O2

# 交叉调试器，配合 make debug 使用（连到 QEMU 的 1234 端口）。
GDB		:= $(GCCPREFIX)gdb

# CC：交叉编译器 —— 生成 RISC-V 机器码的关键。
CC		:= $(GCCPREFIX)gcc
# CFLAGS 逐条解释（答辩容易被追问）：
#   -mcmodel=medany : 代码模型。内核被链接/加载到 0x80200000 这样的高地址，
#                     普通 PC 相对寻址范围不够，必须用 medany 允许 ±2GiB 范围。
#   -std=gnu99      : 用 C99 + GNU 扩展。
#   -Werror         : 警告当错误（所以注释写错也会编译失败，是"严格模式"）。
#   -Wno-unused     : 但允许有未使用的变量/函数（内核里大量占位函数）。
CFLAGS  := -mcmodel=medany -std=gnu99 -Wno-unused -Werror
#   -fno-builtin : 不要用编译器内建版本的 memcpy/printf 等，我们有自己的 libs/。
#   -Wall -O2    : 打开警告、开优化。
#   -nostdinc    : 不使用系统标准头文件（-I 里指到我们自己的 libs/）。
CFLAGS	+= -fno-builtin -Wall -O2 -nostdinc $(DEFS)
#   -ffunction-sections -fdata-sections：每个函数/变量单独一个段，
#   配合链接时的 --gc-sections，可以把没人用的代码/数据直接扔掉。
CFLAGS	+= -fno-stack-protector -ffunction-sections -fdata-sections
#   -g : 生成调试信息（带行号），这是 make debug/gdb 能看源码的前提。
CFLAGS	+= -g
CTYPE	:= c S

# LD：交叉链接器。
LD      := $(GCCPREFIX)ld
# -m elf64lriscv : 链接出的 ELF 是 64 位小端 RISC-V 格式。
LDFLAGS	:= -m elf64lriscv
# -nostdlib : 不链接标准库与 C 运行时（没有 main/crt0，入口由 kernel.ld 指定）。
#   --gc-sections : 与上面的 -ffunction-sections 配套，剔除无用段。
LDFLAGS	+= -nostdlib --gc-sections

# objcopy 把 ELF 变成"纯二进制"；objdump 用来反汇编（生成 kernel.asm 供阅读）。
OBJCOPY := $(GCCPREFIX)objcopy
OBJDUMP := $(GCCPREFIX)objdump

COPY	:= cp
MKDIR   := mkdir -p
MV		:= mv
RM		:= rm -f
AWK		:= awk
SED		:= sed
SH		:= sh
TR		:= tr
TOUCH	:= touch -c

# 所有 .o 放在 obj/，最终产物放在 bin/。
OBJDIR	:= obj
BINDIR	:= bin

# 下面这三个变量由 tools/function.mk 里的宏通过 $(eval) 动态填充：
#   ALLOBJS  = 所有目标文件    ALLDEPS = 所有 .d 依赖文件
#   TARGETS  = 需要生成的最终目标（这里就是 bin/kernel 和 bin/ucore.img）
ALLOBJS	:=
ALLDEPS	:=
TARGETS	:=

# 引入构建框架：它提供了 add_files / create_target / toobj 等"宏函数"，
#   让这个 Makefile 不用为每个目录手写编译规则（见 tools/function.mk）。
include tools/function.mk

# listf_cc(dir) = 列出目录下的 .c/.S 文件（CTYPE := c S）。
listf_cc = $(call listf,$(1),$(CTYPE))

# for cc
# add_files_cc：把某目录下的源文件都加入"编译并放进某个包(packet)"。
add_files_cc = $(call add_files,$(1),$(CC),$(CFLAGS) $(3),$(2),$(4))
create_target_cc = $(call create_target,$(1),$(2),$(3),$(CC),$(CFLAGS))

# for hostcc
add_files_host = $(call add_files,$(1),$(HOSTCC),$(HOSTCFLAGS),$(2),$(3))
create_target_host = $(call create_target,$(1),$(2),$(3),$(HOSTCC),$(HOSTCFLAGS))

# 目标文件命名转换的辅助宏：
#   a.c -> obj/a.o（objfile）、a.c -> obj/a.asm/.out/.sym（反汇编等辅助产物）。
cgtype = $(patsubst %.$(2),%.$(3),$(1))
objfile = $(call toobj,$(1))
asmfile = $(call cgtype,$(call toobj,$(1)),o,asm)
outfile = $(call cgtype,$(call toobj,$(1)),o,out)
symfile = $(call cgtype,$(call toobj,$(1)),o,sym)

# for match pattern
match = $(shell echo $(2) | $(AWK) '{for(i=1;i<=NF;i++){if(match("$(1)","^"$$(i)"$$")){exit 1;}}}'; echo $$?)

# >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
# include kernel/user

# 头文件搜索路径：-Ilibs（所以内核里写 #include <defs.h> 就能找到 libs/defs.h）。
INCLUDE	+= libs 

CFLAGS	+= $(addprefix -I,$(INCLUDE))

# ===== libs/ 目录：内核自己实现的"迷你标准库"（printf/string/sbi ...）=====
LIBDIR	+= libs

# 把 libs 下所有 .c 编成 .o 并放进名为 libs 的包。
$(call add_files_cc,$(call listf_cc,$(LIBDIR)),libs,)

# -------------------------------------------------------------------
# kernel

# 内核源码的 .h 搜索路径（kern/debug、kern/driver、kern/mm ...）。
KINCLUDE	+= kern/debug/ \
			   kern/driver/ \
			   kern/trap/ \
			   kern/libs/\
			   kern/mm/ \
			   kern/arch/

# 内核源码目录：init（入口/初始化）、driver（控制台）、libs、mm 等。
KSRCDIR		+= kern/init \
			   kern/debug \
			   kern/libs \
			   kern/driver \
			   kern/trap \
			   kern/mm

# 加 -I 前缀传给编译器。
KCFLAGS		+= $(addprefix -I,$(KINCLUDE))

# 把内核源码编成 .o 放进名为 kernel 的包。
$(call add_files_cc,$(call listf_cc,$(KSRCDIR)),kernel,$(KCFLAGS))

# KOBJS = libs 包 + kernel 包的全部 .o —— 这就是要链接的"全部输入目标文件"。
KOBJS	= $(call read_packet,kernel libs)

# create kernel target
kernel = $(call totarget,kernel)

# bin/kernel 依赖链接脚本：改了 kernel.ld 也要重新链接。
$(kernel): tools/kernel.ld

# 链接内核。下面三条命令属于同一个 recipe（都以制表符开头），
#   所以注释必须写在这里，不能插在它们中间（插进去会把命令行拆断）：
#   (1) $(LD) $(LDFLAGS) -T tools/kernel.ld -o bin/kernel $(KOBJS)
#       —— 真正的一句链接命令：-T 指定链接脚本（决定入口和 0x80200000 布局）。
#   (2) objdump -S 生成 kernel.asm（带源码行），方便答辩前对着汇编讲流程。
#   (3) objdump -t 导出 kernel.sym（地址 -> 符号名），看地址时非常有用。
$(kernel): $(KOBJS)
	@echo + ld $@
	$(V)$(LD) $(LDFLAGS) -T tools/kernel.ld -o $@ $(KOBJS)
	@$(OBJDUMP) -S $@ > $(call asmfile,kernel)
	@$(OBJDUMP) -t $@ | $(SED) '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $(call symfile,kernel)

$(call create_target,kernel)

# -------------------------------------------------------------------
# create ucore.img
# 最终产物：bin/ucore.img（QEMU 加载的就是它）。
UCOREIMG	:= $(call totarget,ucore.img)


# 从 ELF 内核抽出纯二进制镜像：
#   --strip-all 去掉符号表/调试信息（体积小，QEMU 直接当内存镜像用），
#   -O binary   输出"没有文件头、只有机器码和数据"的裸镜像。
$(UCOREIMG): $(kernel)
	$(OBJCOPY) $(kernel) --strip-all -O binary $@

$(call create_target,ucore.img)

# >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

# finish_all：收尾，生成 .d 依赖关系并创建 obj/、bin/ 目录。
$(call finish_all)

IGNORE_ALLDEPS	= clean \
				  dist-clean \
				  grade \
				  touch \
				  print-.+ \
				  handin

ifeq ($(call match,$(MAKECMDGOALS),$(IGNORE_ALLDEPS)),0)
-include $(ALLDEPS)
endif

# files for grade script

# 默认目标：生成 TARGETS（即 bin/kernel 与 bin/ucore.img）。所以敲 make 就行。
TARGETS: $(TARGETS)

.DEFAULT_GOAL := TARGETS

# ===== make qemu：启动虚拟机跑内核（lab1 最重要的验证手段）=====
.PHONY: qemu 
qemu: $(UCOREIMG) $(SWAPIMG) $(SFSIMG)
#	$(V)$(QEMU) -kernel $(UCOREIMG) -nographic
# QEMU 参数逐条解释（答辩会问）：
#   -machine virt : 模拟 QEMU 的标准虚拟 RISC-V 平台（内存布局、外设都定好）。
#   -nographic    : 不弹图形窗口，串口/控制台直接输出到你的终端。
#   -bios default : 使用自带的 OpenSBI 固件当 BIOS（对应 0x80000000 处的 M 态代码）。
#   -device loader,file=bin/ucore.img,addr=0x80200000 :
#        ★把内核镜像加载进物理内存 0x80200000 —— 必须和 kernel.ld 里
#        BASE_ADDRESS 完全一致，然后 OpenSBI 跳过来执行 kern_entry。
	$(V)$(QEMU) \
		-machine virt \
		-nographic \
		-bios default \
		-device loader,file=$(UCOREIMG),addr=0x80200000

# ===== make debug：让 QEMU 冻结在第 0 条指令，等待 GDB 连接（练习 2 用）=====
#   下面那条命令是用反斜杠连成的一整行，最后两个参数就是调试开关，
#   注释不能插进续行中间（否则会让 QEMU 丢掉 -device loader 和 -s -S）：
#     -s : 在 1234 端口开 gdb server；
#     -S : QEMU 启动后立刻暂停，等 GDB 发 continue 才真正执行。
debug: $(UCOREIMG) $(SWAPIMG) $(SFSIMG)
	$(V)$(QEMU) \
		-machine virt \
		-nographic \
		-bios default \
		-device loader,file=$(UCOREIMG),addr=0x80200000\
		-s -S

# ===== make gdb：另开一个终端，用 GDB 连上 QEMU 调试内核 =====
#   它就是一条 gdb 命令行，三个 -ex 依次是：
#     -ex 'file bin/kernel'     : 载入符号（能看到函数名/源码行）。
#     -ex 'set arch riscv:rv64' : 指定架构。
#     -ex 'target remote :1234' : 连上 make debug 开的 gdb server。
#   连上以后的常用操作（这些是在 gdb 里手敲的，不是 Makefile 的一部分）：
#     b kern_entry / b *0x80200000   : 在内核第一条指令处下断点
#     si                             : 单步汇编，一条指令一条指令地走
#     x/4i $pc                       : 看当前 PC 附近的指令
#     watch *0x80200000              : 观察内核被 OpenSBI 加载进内存的瞬间
#     info registers / c / quit      : 看寄存器 / 继续运行 / 退出
#   从 0x1000(MROM) 单步跟到 0x80000000(OpenSBI) 的过程，就能回答练习 2。
gdb:
	riscv64-unknown-elf-gdb \
    -ex 'file bin/kernel' \
    -ex 'set arch riscv:rv64' \
    -ex 'target remote localhost:1234'

	
.PHONY: grade touch

GRADE_GDB_IN	:= .gdb.in
GRADE_QEMU_OUT	:= .qemu.out
HANDIN			:= proj$(PROJ)-handin.tar.gz

TOUCH_FILES		:= kern/trap/trap.c

MAKEOPTS		:= --quiet --no-print-directory

# grade：先用 clean 清干净，再跑 tools/grade.sh 自动评分脚本。
grade:
	$(V)$(MAKE) $(MAKEOPTS) clean
	$(V)$(SH) tools/grade.sh

touch:
	$(V)$(foreach f,$(TOUCH_FILES),$(TOUCH) $(f))

print-%:
	@echo $($(shell echo $(patsubst print-%,%,$@) | $(TR) [a-z] [A-Z]))

.PHONY: clean dist-clean handin packall tags
# clean：删除 obj/、bin/ 和中间产物；dist-clean 还会删掉打包文件。
clean:
# 前面带 '-' 表示即使这条命令失败也继续执行（比如目录本来就不存在）。
	$(V)$(RM) $(GRADE_GDB_IN) $(GRADE_QEMU_OUT) cscope* tags
	-$(RM) -r $(OBJDIR) $(BINDIR)

dist-clean: clean
	-$(RM) $(HANDIN)

handin: packall
	@echo Please visit http://learn.tsinghua.edu.cn and upload $(HANDIN). Thanks!

packall: clean
	@$(RM) -f $(HANDIN)
	@tar -czf $(HANDIN) `find . -type f -o -type d | grep -v '^\.*$$' | grep -vF '$(HANDIN)'`

tags:
	@echo TAGS ALL
	$(V)rm -f cscope.files cscope.in.out cscope.out cscope.po.out tags
	$(V)find . -type f -name "*.[chS]" >cscope.files
	$(V)cscope -bq 
	$(V)ctags -L cscope.files
