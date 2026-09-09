# MIT 6.S081 / 6.828 (Fall 2021) · xv6 操作系统实验

本仓库是 MIT 6.S081/6.828 **2021 秋季学期** xv6 操作系统课程实验的完整实现。
实验基于 RISC-V 版本的 xv6 教学操作系统，共完成全部 **10 个必修 Lab**，
全部通过官方 `make grade` 评测。

- 官方课程页：<https://pdos.csail.mit.edu/6.828/2021/xv6.html>
- 官方 Lab 仓库：`git://g.csail.mit.edu/xv6-labs-2021`（上游）
- 本机实现（10 个 Lab 各占一个分支）：`cow fs lock mmap net pgtbl syscall thread traps util`

## 目录结构

```
project_xv6/
├── README.md            # 项目说明（本文件）
├── 实验报告.md          # 完整实验报告（环境搭建 / 各 Lab 目的、过程、问题与心得）
└── xv6-labs-2021/       # xv6 源码 + 10 个 Lab 分支的解答
    ├── kernel/          # 内核（进程、虚拟内存、文件系统、驱动等）
    ├── user/            # 用户程序（各 Lab 编写的测试/工具程序）
    ├── notxv6/          # 在宿主机运行的线程实验（ph/barrier）
    ├── grade-lab-*      # 官方评分脚本（make grade 调用）
    └── Makefile         # 构建/运行/评测入口
```

## 环境与工具链

在 **macOS (Apple Silicon, arm64)** 上搭建：

| 组件 | 版本/位置 |
| --- | --- |
| 交叉编译工具链 | xPack `riscv-none-elf-gcc 15.2.0`（符号链接为 `riscv64-unknown-elf-*`，`~/tools/riscv64-unknown-elf-bin`） |
| 模拟器 | Homebrew `qemu-system-riscv64 11.1.1` |
| 评分脚本 | Homebrew Python 3.11（`~/tools/py3bin/python3`，系统 python3.14 因缺 `pipes` 模块不可用） |
| 网络 | 直连 github.com 被限制，需走本机 Clash 代理 `127.0.0.1:7890`（git 已全局配置） |

Makefile 针对新工具链做了少量兼容性改动（见 `实验报告.md` §2.3）：
`-march=rv64gc -mabi=lp64d`、`ASFLAGS`、链接 `-m elf64lriscv`、
`-Wno-infinite-recursion -Wno-error=incompatible-pointer-types` 等。

## 使用方式

每个 Lab 的解答在独立分支上，评测需先设置 PATH 再进入对应分支：

```bash
export PATH="$HOME/tools/py3bin:$HOME/tools/riscv64-unknown-elf-bin:$PATH"

cd xv6-labs-2021
git checkout util        # 换成 syscall / pgtbl / traps / cow / thread / net / lock / fs / mmap
make clean
make grade              # 运行该 Lab 官方评测
# 或单独跑某个测试： ./grade-lab-<lab> <test>
```

快速上手（在 xv6 内跑用户程序）：

```bash
make qemu               # 启动 xv6；Ctrl-A x 退出
# 例如：sleep 10 / pingpong / primes / find . b / mmaptest ...
```

## Lab 总览与成绩

| # | Lab | 分支 | 主题 | make grade |
| --- | --- | --- | --- | --- |
| 1 | Lab util | `util` | Unix 工具：sleep/pingpong/primes/find/xargs | 100/100 |
| 2 | Lab syscall | `syscall` | 系统调用：trace、sysinfo | 35/35 |
| 3 | Lab pgtbl | `pgtbl` | 页表：USYSCALL、vmprint、pgaccess | 46/46 |
| 4 | Lab traps | `traps` | 陷阱：backtrace、sigalarm/sigreturn | 85/85 |
| 5 | Lab cow | `cow` | 写时复制 fork | 110/110 |
| 6 | Lab thread | `thread` | 多线程：uthread/ph/barrier | 60/60 |
| 7 | Lab net | `net` | 网络驱动：E1000 | 100/100 |
| 8 | Lab lock | `lock` | 锁优化：内存分配器、块缓存 | 70/70 |
| 9 | Lab fs | `fs` | 文件系统：大文件、符号链接 | 100/100 |
| 10 | Lab mmap | `mmap` | 内存映射文件 mmap/munmap | 140/140 |

> `make grade` 的评分脚本与 TA 使用的一致；每个分支顶端提交即最终解答。

## 各 Lab 实现要点速览

- **util**：用 xv6 的 pipe/fork/read/write 等系统调用实现 5 个 Unix 小程序。
- **syscall**：新增 `trace`（按 mask 打印被跟踪系统调用）与 `sysinfo`（空闲内存/进程数），
  掌握从用户 stub → 内核分发表 → 实现的完整加系统调用流程。
- **pgtbl**：每进程映射只读 `USYSCALL` 页加速 `getpid()`；递归打印页表 `vmprint`；
  用 RISC-V PTE 的 A 位实现 `pgaccess`。
- **traps**：利用帧指针(s0)实现内核 `backtrace`；用保存/恢复 trapframe 实现用户态周期告警
  `sigalarm/sigreturn`（含防重入）。
- **cow**：fork 只复制页表并共享物理页（清 W 位、置 COW 位），缺页/`copyout` 时按需复制；
  物理页维护引用计数。
- **thread**：用户级线程切换（只保存被调用者保存寄存器）；`ph` 用每桶锁消除丢键并达到并行加速；
  `barrier` 用条件变量实现多轮屏障。
- **net**：补全 E1000 网卡驱动的 TX/RX 描述符环，配合 xv6 自带 IP/UDP/ARP 协议栈完成
  ping/DNS 测试。
- **lock**：`kalloc` 改为每 CPU 一个空闲链表（空时偷取）；`bcache` 改为 13 桶哈希 +
  每桶锁 + 全局锁只负责淘汰（LRU 用 ticks 时间戳）。
- **fs**：`bmap/itrunc` 增加双重间接块，文件上限 268 → 65803 块；新增 `symlink` 系统调用与
  open 的符号链接跟随（含环检测）。
- **mmap**：进程维护 VMA 表，缺页时才读文件并映射；`munmap` 支持整段/头/尾部分卸载与
  MAP_SHARED 回写；fork 复制 VMA、exit/exec 自动卸载。

