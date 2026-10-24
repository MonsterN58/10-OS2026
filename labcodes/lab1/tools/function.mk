# ============================================================================
# tools/function.mk —— Makefile 的"函数库"（被 Makefile include 进来）
# ============================================================================
# 【为什么需要它】
#   内核有几十个 .c/.S，如果为每个文件手写编译规则，Makefile 会又臭又长。
#   这个文件用 make 的"宏函数"（$(call xxx) + $(eval)）自动生成规则：
#     Makefile 里只需写一行
#         $(call add_files_cc,$(call listf_cc,$(KSRCDIR)),kernel,$(KCFLAGS))
#     就等价于为 kern/ 下每个 .c/.S 都写：
#         obj/kern/xxx.o: kern/xxx.c
#               $(CC) ... -c $< -o $@
#   【答辩要点】$(eval) 在"解析阶段"展开并生效、$(foreach) 遍历文件列表、
#   .SECONDEXPANSION 让依赖里的 $$(...) 延迟到第二次展开 —— 这三件事是本文件
#   的核心技巧，一句话说清即可，不要求背语法。
# ============================================================================
OBJPREFIX	:= __objs_

# 开启"二次展开"：先展开一次收集文件列表，再展开一次生成真正的规则。
.SECONDEXPANSION:
# ===== 下面是内部实现（do_xxx，一般不用直接看）=====
# -------------------- function begin --------------------

# listf(目录, 后缀)：列出这些目录下所有指定后缀的文件（SLASH 即 /）。
# list all files in some directories: (#directories, #types)
listf = $(filter $(if $(2),$(addprefix %.,$(2)),%),\
		  $(wildcard $(addsuffix $(SLASH)*,$(1))))

# toobj(文件[, 包名])：把 a/b.c 映射成 obj/[包名/]a/b.o。
# get .o obj files: (#files[, packet])
toobj = $(addprefix $(OBJDIR)$(SLASH)$(if $(2),$(2)$(SLASH)),\
		$(addsuffix .o,$(basename $(1))))

# todep：同名 .d 依赖文件（由 gcc -MM 生成，记录该 .o 依赖哪些头文件）。
# get .d dependency files: (#files[, packet])
todep = $(patsubst %.o,%.d,$(call toobj,$(1),$(2)))

# totarget：最终产物放 bin/ 下，例如 kernel -> bin/kernel。
totarget = $(addprefix $(BINDIR)$(SLASH),$(1))

# packetname：给"包"变量名加前缀 __objs_，如 libs -> __objs_libs。
# change $(name) to $(OBJPREFIX)$(name): (#names)
packetname = $(if $(1),$(addprefix $(OBJPREFIX),$(1)),$(OBJPREFIX))

# cc_template：真正生成"编译一条命令"的模板（依赖规则 + 编译规则）。
# cc compile template, generate rule for dep, obj: (file, cc[, flags, dir])
define cc_template
$$(call todep,$(1),$(4)): $(1) | $$$$(dir $$$$@)
	@$(2) -I$$(dir $(1)) $(3) -MM $$< -MT "$$(patsubst %.d,%.o,$$@) $$@"> $$@
$$(call toobj,$(1),$(4)): $(1) | $$$$(dir $$$$@)
	@echo + cc $$<
	$(V)$(2) -I$$(dir $(1)) $(3) -c $$< -o $$@
ALLOBJS += $$(call toobj,$(1),$(4))
endef

# do_cc_compile：对文件列表里每个文件套用上面的 cc_template。
# compile file: (#files, cc[, flags, dir])
define do_cc_compile
$$(foreach f,$(1),$$(eval $$(call cc_template,$$(f),$(2),$(3),$(4))))
endef

# do_add_files_to_packet：编译文件并把生成的 .o 记进某个"包"变量里，
#   这样后面可以用 $(call read_packet,kernel libs) 一次取出所有 .o。
# add files to packet: (#files, cc[, flags, packet, dir])
define do_add_files_to_packet
__temp_packet__ := $(call packetname,$(4))
ifeq ($$(origin $$(__temp_packet__)),undefined)
$$(__temp_packet__) :=
endif
__temp_objs__ := $(call toobj,$(1),$(5))
$$(foreach f,$(1),$$(eval $$(call cc_template,$$(f),$(2),$(3),$(5))))
$$(__temp_packet__) += $$(__temp_objs__)
endef

# do_add_objs_to_packet：把已有的 .o 直接塞进包。
# add objs to packet: (#objs, packet)
define do_add_objs_to_packet
__temp_packet__ := $(call packetname,$(2))
ifeq ($$(origin $$(__temp_packet__)),undefined)
$$(__temp_packet__) :=
endif
$$(__temp_packet__) += $(1)
endef

# do_create_target：创建一个最终目标（如 bin/kernel），
#   依赖 = 相关包里的全部 .o，命令 = 用 ld 链接出这个文件。
# add packets and objs to target (target, #packes, #objs[, cc, flags])
define do_create_target
__temp_target__ = $(call totarget,$(1))
__temp_objs__ = $$(foreach p,$(call packetname,$(2)),$$($$(p))) $(3)
TARGETS += $$(__temp_target__)
ifneq ($(4),)
$$(__temp_target__): $$(__temp_objs__) | $$$$(dir $$$$@)
	$(V)$(4) $(5) $$^ -o $$@
else
$$(__temp_target__): $$(__temp_objs__) | $$$$(dir $$$$@)
endif
endef

# do_finish_all：收尾。ALLDEPS 收集全部 .d 文件（供 Makefile 末尾 -include），
#   并确保 obj/、bin/ 及各级子目录存在（用 mkdir -p）。
# finish all
define do_finish_all
ALLDEPS = $$(ALLOBJS:.o=.d)
$$(sort $$(dir $$(ALLOBJS)) $(BINDIR)$(SLASH) $(OBJDIR)$(SLASH)):
	@$(MKDIR) $$@
endef

# ===== 下面这些是给 Makefile 用的"对外接口" =====
#   实现套路都一样：$(call do_xxx,...) 交给 $(eval) 立即执行。
# --------------------  function end  --------------------
# compile file: (#files, cc[, flags, dir])
# cc_compile：编译若干文件。
cc_compile = $(eval $(call do_cc_compile,$(1),$(2),$(3),$(4)))

# add files to packet: (#files, cc[, flags, packet, dir])
# add_files：编译文件并加入包（Makefile 里 add_files_cc/add_files_host 都基于它）。
add_files = $(eval $(call do_add_files_to_packet,$(1),$(2),$(3),$(4),$(5)))

# add objs to packet: (#objs, packet)
# add_objs：把 .o 加入包。
add_objs = $(eval $(call do_add_objs_to_packet,$(1),$(2)))

# add packets and objs to target (target, #packes, #objs, cc, [, flags])
# create_target：创建最终目标（Makefile 用它生成 bin/kernel、bin/ucore.img）。
create_target = $(eval $(call do_create_target,$(1),$(2),$(3),$(4),$(5)))

# read_packet：取出某个包里所有 .o，即"要链接的目标文件列表"。
read_packet = $(foreach p,$(call packetname,$(1)),$($(p)))

# add_dependency：手工给目标加依赖。
add_dependency = $(eval $(1): $(2))

# finish_all：收尾（Makefile 里 $(call finish_all) 就是调它）。
finish_all = $(eval $(call do_finish_all))

