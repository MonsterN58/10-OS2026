# 10-OS2026

操作系统实验（Lab1 / Lab2）。

## 目录结构

| 路径 | 内容 |
|---|---|
| `lab1/` | Lab1 提交版，含源码、实验报告与运行截图 |
| `labcodes/lab1/` | Lab1 学习版，源码带逐行教学注释 |
| `labcodes/lab2/` | Lab2 完整工程，含成员一、二、三的交付 |
| `交接材料/` | 成员二原始交付包与校验清单 |
| `合并说明.md` | Lab2 合并过程、遇到的实际问题与处理 |

## Lab2 内容

- 成员一（石昊洋，2411804）：`page_init`、设备树内存探测、页表/GDB 调试
- 成员二（邹俊轩，2412592）：First-Fit 修复、Best-Fit、简化 SLUB
- 成员三（秦东杰，2411738）：Buddy System、独立 Buddy/SLUB 测试、集成验收

默认页管理器是 Best-Fit，原始评分路径为 20/20。Buddy 通过编译期开关启用：

```sh
cd labcodes/lab2
make clean
make PMM=buddy MEMBER3_TESTS=1
make qemu QEMU=/实际课程QEMU路径/qemu-system-riscv64
```

一键验收两种配置：

```sh
cd labcodes/lab2
python3 tests/run_member3_checks.py \
  --qemu /opt/ucore-manual/qemu-4.1.1/bin/qemu-system-riscv64 \
  --log-prefix member3_vm
```

详细说明见 `labcodes/lab2/README.md`、`labcodes/lab2/实验报告.md`
与 `labcodes/lab2/specs/member3.md`。

## 说明

`PMM` 是编译期开关，切换前必须 `make clean`，否则会复用另一套目标文件。
单 hart 教学实现，不含并发保护。
