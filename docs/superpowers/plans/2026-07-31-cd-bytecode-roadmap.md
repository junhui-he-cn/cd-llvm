# CD Bytecode Roadmap（现状与边界）

> **状态（2026-08-10）：功能开发暂停。** 近期不再安排新功能开发。
> 本文件已从“开发队列”重写为“现状与边界”记录，不再包含待办任务、检查框或下一步执行顺序。
> 历史开发细节保留在 `docs/superpowers/plans/2026-08-03-cd-bytecode-development-plan.md`
> （已冻结，仅作档案）。

## 1. 概述

CD 是 LLVM 中的一个实验性软件 target，用于把受控的 LLVM IR 子集编译为
Compiler Design 字节码 VM 的文本 `cdbc 0.1` artifact：

```text
LLVM IR --llc -mtriple=cd-unknown-unknown--> cdbc 0.1 --> cd-compiler Rust VM
```

Direct emitter 是默认兼容路径；TableGen machine emitter 是 opt-in 路径。
Rust VM 只作为执行 oracle 与验证依赖，位于独立的 `cd-compiler/` checkout，
不属于本仓库。

当前完整能力清单见 [docs/cd-bytecode-features.md](../../cd-bytecode-features.md)。

## 2. 架构与不变式

1. 产物固定为文本 `cdbc 0.1`；不新增 opcode、artifact 字段或新版本。
2. Direct 与 machine 两条路径共享 `CDValueABI`、`CDBytecodeFormat` 和
   direct/machine parity 验证；machine 路径保持 opt-in、text-only。
3. 普通 LLVM 指针、聚合、全局变量和外部调用永远不被推断为 CD 值；
   只有显式 `llvm.cd.*` intrinsic、`cd.value.params`/`cd.value.return`
   标记边界和已验证的 select/PHI/存储路径携带 CD 值。
4. 不支持对象/汇编输出、JIT、二进制 `.cdbc`、Clang CD 前端和 GC 布局；
   `-filetype=obj` 被拒绝。
5. `llc -g` 被上游 LLVM 24 驱动拒绝；CD debug 使用显式
   `!cd.sources`、`!dbg` 与 `!cd.ranges` 元数据，不从普通 debug 元数据推断源码。
6. 未定义契约的 native 名称、间接调用、逃逸/易失/原子存储等保持编译期拒绝，
   不产出部分有效 artifact。

## 3. 已完成能力快照

- 目标与 artifact 边界（M0-M1）：CD target 注册、typed `CDArtifact` 模型、
  `CDBytecodeFormat` 结构校验、确定性 canonical serializer。
- 标量与控制流（M2）：算术/比较/取反/布尔 not/类型转换、函数参数与返回、
  PHI edge store、`select`、`-O0`/`-O2` 行为一致性；无法表达的无符号语义被拒绝。
- machine 后端（M3）：TableGen 描述、`-cd-backend=machine` opt-in 路径、
  虚拟值 MachineInstr、MIR 覆盖、direct/machine 行为 parity。
- 值 ABI（M4）：字符串、数组、map、record、enum variant、索引/赋值、
  有界 native allowlist（集合、字符串、数值与回调 helper）。
- 调试与可观测性（M5）：`debug_sources`/`debug_locations`/`debug_ranges`、
  运行时诊断、trace/profile/debug parity、step/next/aliases/help/error-pause、
  冻结的 pause-state 契约。
- 模块产物与链接（M6）：module envelope、依赖元数据、fallthrough `main`、
  VM link 集成与链接后诊断。
- 本地验证门（M7-local）：LLVM-only、Rust VM、parity、module-link 四组门通过。
- 函数边界动态值传输（M8-first）：`cd.value.params`/`cd.value.return`、
  动态 `select`、动态 PHI（含 loop-carried）、单槽动态存储。

## 4. 交付记录

工作集中在 2026-07-31 至 2026-08-10，共 78 个功能提交（不含 snapshot），
全部位于 `main` 并已与 `origin/main` 同步。代表性提交：

| 日期 | 提交 | 内容 |
| --- | --- | --- |
| 2026-07-31 | `4ecb31671` | 引入 CD bytecode target 的 LLVM snapshot |
| 2026-08-01 | `fd2a88c4d` 等 | typed artifact、标量/控制流、TableGen machine 基础 |
| 2026-08-02 | `95547b0be` 等 | module envelope、debug sources/locations/ranges、bounded natives |
| 2026-08-03 | `d76a2addc` 等 | 动态值跨函数传输、string native helpers、调试器 parity |
| 2026-08-04 至 08-08 | `749aef4ba` 等 | map/filter/any/all/count/find/findIndex/flatMap/reduce 回调 natives |
| 2026-08-08 至 08-09 | `c82ecbdb5`、`9e8be48c9`、`0b857c4c8` | 动态 select、PHI、单槽存储 |
| 2026-08-09 | `4704d3668` | debugger query 契约设计（仅文档） |
| 2026-08-10 | `13bd594af` 至 `ca7adb9e5` | contains/slice/copy/concat/keys/values/remove/clear/merge/push/pop 有界 natives |

## 5. 当前验证状态

- LLVM-only：126 个 lit fixtures，`125 passed / 1 unsupported`
  （未设置 `CD_COMPILER_ROOT` 时 VM 集成用例报告 unsupported）。
- Direct/machine parity：manifest 95 项通过，覆盖值 ABI、natives、回调、
  动态 select/PHI/存储和运行时错误。
- Rust VM：Cargo 测试 `73 + 3 + 8` 通过；module-link harness 通过。
- 托管 CI：`.github/workflows/cd-bytecode.yml` 已包含完整八工具闭包
  （`llc`、`FileCheck`、`count`、`not`、`opt`、`llvm-config`、`llvm-readobj`、
  `split-file`）；hosted rerun 仍是外部非阻塞项。

## 6. 边界（保持不变）

- `cdbc 0.1` 文本格式、program/module 模式、驱动选项行为不变。
- 无对象/汇编输出、无 JIT、无二进制 artifact、无新 artifact 版本。
- `llc -g` 保持为上游驱动边界；目标侧 `-g` 语义不安排实现。
- machine 路径保持 opt-in；nested `cd-compiler/` 保持独立、只读。
- 未定义契约的 native 名称、不完整回调标记、普通指针参数保持拒绝。

## 7. 暂停/冻结清单

近期不开展以下工作：

- 新增 native stdlib 名称（除非重新定义并评审契约）。
- debugger 查询命令（`where`/`locals`/`breakpoints`/`list`，目前仅有设计文档）。
- 目标侧 `-g` 语义、对象/汇编/JIT 输出、二进制 `.cdbc`、新 artifact 版本。
- 任何需要修改 Rust VM 或 `.cdbc` 格式的工作。

`2026-08-03` 的 development plan 已冻结，仅作历史档案；恢复开发时以本文件
和功能文档为准重新评估。

## 8. 恢复开发的约定

若未来恢复开发，按既有验证文化推进：

1. 每个功能按“契约文档 -> 实现 -> fixtures -> direct/machine parity -> 验证门”
   的垂直切片推进，不合并多个独立切片。
2. 保持 `cdbc 0.1`、共享 ABI 校验、Rust VM oracle 与 nested checkout 边界。
3. 本地验证通过后再单独评估 commit / push；这两个动作仍是独立授权。

## 9. 相关文档

- 当前功能清单：[docs/cd-bytecode-features.md](../../cd-bytecode-features.md)
- ABI 契约：[docs/cd-bytecode-llvm-abi.md](../../cd-bytecode-llvm-abi.md)
- machine 后端设计：[docs/cd-bytecode-machine-backend.md](../../cd-bytecode-machine-backend.md)
- 调试器契约：[docs/cd-bytecode-debugger-contract.md](../../cd-bytecode-debugger-contract.md)
- 调试器查询设计（未实现）：[docs/cd-bytecode-debugger-query-design.md](../../cd-bytecode-debugger-query-design.md)
- 验证方法：[docs/cd-bytecode-verification.md](../../cd-bytecode-verification.md)
- 冻结的开发计划：[docs/superpowers/plans/2026-08-03-cd-bytecode-development-plan.md](2026-08-03-cd-bytecode-development-plan.md)
