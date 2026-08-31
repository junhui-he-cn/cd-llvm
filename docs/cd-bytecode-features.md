# CD 字节码目标（CD Bytecode Target）当前功能参考

> 本文描述 2026-08-10 现有实现的功能边界，供使用者与审查者核对“当前能做什么”。
> 本文不是开发计划，不含待办、里程碑或下一步执行顺序；开发状态见
> [Roadmap（现状与边界）](superpowers/plans/2026-07-31-cd-bytecode-roadmap.md)。

## 1. 这是什么

CD 是 LLVM 中的一个实验性软件目标（software target）：把受控的 LLVM IR
子集编译成 Compiler Design 字节码虚拟机（VM）的文本产物（artifact）
`cdbc 0.2`，再由 `cd-compiler` 的 Rust VM 执行。目标侧只负责生成产物；
运行时语义（类型检查、资源预算、回调帧、错误诊断）由 Rust VM 负责。

```text
LLVM IR --llc -mtriple=cd-unknown-unknown--> cdbc 0.2 --> cd-compiler Rust VM
```

## 2. 快速开始

构建所需工具：

```sh
ninja -C build-cd llc FileCheck count not opt llvm-config llvm-readobj split-file
```

生成程序产物（默认路径）：

```sh
build-cd/bin/llc -mtriple=cd-unknown-unknown input.ll -o output.cdbc
```

生成模块产物（供 VM 链接）：

```sh
build-cd/bin/llc -mtriple=cd-unknown-unknown -cd-artifact=module input.ll -o module.cdbc
```

使用显式开启的机器后端（与直接路径行为一致）：

```sh
build-cd/bin/llc -mtriple=cd-unknown-unknown -cd-backend=machine input.ll -o output-machine.cdbc
```

产物是确定性文本；同一输入重复生成得到逐字节相同的 `cdbc 0.2` 文件。

## 3. 值模型与 ABI 规则

- CD `number` 的语义是 IEEE-754 双精度浮点数（double）。LLVM 整数常量仅在
  带符号值可被 double 精确表示时接受；`undef`、`poison`、非有限浮点常量
  被拒绝。
- LLVM `i1` 映射为 CD `bool`；地址空间 0（address-space zero）的
  `ptr null` 映射为 CD `nil`。
- CD 动态值在 LLVM IR 中表示为地址空间 0 的 `ptr` 令牌（token），只能来自
  显式 `llvm.cd.*` 内建函数（intrinsic）、带 `cd.value.params` /
  `cd.value.return` 标记的函数边界，或已验证的动态 `select` / PHI /
  单槽存储。
- 普通 LLVM 指针、聚合、全局变量、外部调用永远不被推断为 CD 值；
  唯一的例外是单参数 `print` / `cd_print` 外部声明（输出）。

## 4. 支持的 LLVM IR 子集

### 标量与控制流

| 类别 | 支持 |
| --- | --- |
| 算术 | `add`/`fadd`、`sub`/`fsub`、`mul`/`fmul`、`sdiv`/`fdiv`（映射为 CD `add`/`subtract`/`multiply`/`divide`） |
| 一元运算 | `fneg` -> `negate`；`xor i1 <v>, true`（两种操作数顺序）-> `not` |
| 比较 | `icmp` 的 `eq`/`ne`/`sgt`/`sge`/`slt`/`sle`；`fcmp` 的有序/无序六种谓词 |
| 类型转换 | `trunc`/`zext`/`sext`/`fptrunc`/`fpext`/`uitofp`/`sitofp`/`fptoui`/`fptosi`/`bitcast` -> `move` |
| 选择 | 标量 `select` 与动态 CD `select`（条件为 `i1`，动态双臂须均为已验证 CD 令牌） |
| 控制流 | 条件/无条件分支、PHI（边写入数值槽位 + 块入口读取）、`ret`（含 `ret void` -> `nil`） |
| 函数 | 已定义函数的直接调用；回调以 `make_function` 物化后再 `call`/`call_native` |
| 存储 | 直接单槽 `alloca`：标量槽，或带完整路径存储证明的动态 CD 槽 |

### 明确拒绝（稳定诊断，不产出部分产物）

- `udiv`、无符号整数排序谓词（`ugt`/`uge`/`ult`/`ule`）、`unreachable`。
- 除 `print`/`cd_print` 外的外部调用、间接调用、未标记指针接口。
- 聚合常量、`undef`/`poison`、非有限常量、无法精确表示的整数常量。
- 逃逸的 `alloca`、GEP/bitcast 别名、未初始化/部分初始化加载、
  volatile/atomic 访问、多槽 `alloca`。
- 未证明来源的指针 PHI、已证明与未证明混合的指针、外地址空间指针。

## 5. 显式 CD 内建函数

| 内建函数 | 签名 | 语义 |
| --- | --- | --- |
| `llvm.cd.string` | `ptr (ptr)` | 私有、常量、单 NUL 结尾的 UTF-8 字节全局 -> 不可变 CD 字符串 |
| `llvm.cd.array` | `ptr (i32, ...)` | 元素数量 + 标量/`nil`/CD 令牌元素 -> 新数组 |
| `llvm.cd.map` | `ptr (i32, ...)` | 元素数量 + 交替键/值 -> 新映射 |
| `llvm.cd.struct` | `ptr (ptr, i32, ...)` | 可选类型名全局 + 字段名/值对 -> 新记录 |
| `llvm.cd.variant` | `ptr (ptr, ptr, i32, ...)` | 枚举/变体名全局 + 载荷（payload）-> 新变体 |
| `llvm.cd.variant.tag` | `i1 (ptr, ptr, ptr)` | 变体 + 枚举/变体名 -> 标签相等判断 |
| `llvm.cd.variant.field` | `T (ptr, i32)` | 变体 + 载荷索引 -> 字段值 |
| `llvm.cd.field` | `T (ptr, ptr)` | 记录 + 字段名 -> 字段值 |
| `llvm.cd.assign.field` | `T (ptr, ptr, T)` | 记录 + 字段名 + 新值 -> 原地更新并返回被赋的值 |
| `llvm.cd.index` | `ptr (ptr, double)` | 集合 + 索引 -> 元素；越界由 VM 报运行时错误 |
| `llvm.cd.assign.index` | `T (ptr, double, T)` | 集合 + 索引 + 新值 -> 原地更新并返回被赋的值 |
| `llvm.cd.len` | `double (ptr)` | 集合长度 |
| `llvm.cd.native` | `T (ptr, ...)` | 私有名称全局 + 有界操作数 -> 原生调用 |

旧的 `llvm.cd.assert_array` 调用形状不是 0.2 接口；目标在编译期拒绝它，
不会生成兼容的字节码。数组访问只能使用 `llvm.cd.index` 和 `llvm.cd.len`。

## 6. 原生标准库允许名单

`llvm.cd.native` 只接受下面这些名字；未列出的名字在编译期被拒绝。
名称必须是私有、常量、地址空间 0、非空 UTF-8 字节全局。

### 数值与内省

| 名字 | 参数 | 结果 | 运行时语义 |
| --- | --- | --- | --- |
| `floor` / `ceil` / `sqrt` | 一个 `double` | `double` | 数值运算（`sqrt(-1)` 为 VM 运行时错误） |
| `str` / `typeOf` | 一个标量或 CD 动态值 | `ptr` | 字符串转换 / 运行时类型名 |
| `hash` | 一个标量或 CD 动态值 | `double` | 运行时哈希数 |

### 集合

| 名字 | 参数 | 结果 | 运行时语义 |
| --- | --- | --- | --- |
| `contains` | 一个 CD 集合 + 标量/CD 待查值 | `i1` | 数组、映射或 range 成员判断 |
| `slice` | CD 数组 + 两个 `double` | `ptr` | 新浅数组切片 |
| `copy` | CD 数组 | `ptr` | 新浅数组拷贝 |
| `concat` | 两个 CD 数组 | `ptr` | 按序拼接的新数组 |
| `push` | CD 数组 + 标量/CD 值 | `ptr` | 原地追加，返回 `nil` |
| `pop` | CD 数组 | `ptr` | 返回并移除最后一个元素；空数组为运行时错误 |
| `remove` | CD 映射 + 标量/CD 键 | `ptr` | 返回并移除第一个匹配键值 |
| `clear` | CD 映射 | `ptr` | 原地清空，返回 `nil` |
| `merge` | 两个 CD 映射 | `ptr` | 新有序映射，右侧重复键胜出 |
| `keys` | CD 映射 | `ptr` | 插入序键数组 |
| `values` | CD 映射 | `ptr` | 插入序值数组 |
| `range` | 一至三个 `double` | `ptr` | range 值，可被 `len`/`index`/`contains` 消费 |

### 字符串

| 名字 | 参数 | 结果 | 运行时语义 |
| --- | --- | --- | --- |
| `substr` | CD 字符串 + 两个 `double` | `ptr` | Unicode 标量值切片（新字符串） |
| `charAt` | CD 字符串 + 一个 `double` | `ptr` | Unicode 标量值字符提取（新字符串） |

`substr`/`charAt` 的字符串参数是编译期无法验明标签的 CD 令牌；
非字符串在运行时得到 VM 类型诊断，而不是编译期错误。

### 回调原生函数

| 名字 | 回调形状 | 结果 | 运行时语义 |
| --- | --- | --- | --- |
| `map` | 一个参数、标记 CD 返回 | `ptr` | 新数组，逐元素回调结果 |
| `flatMap` | 同 `map` | `ptr` | 新数组，回调数组结果只展平一层 |
| `filter` | 一个 CD 参数、精确 `i1` 返回 | `ptr` | 新数组，仅保留谓词为真的元素 |
| `any` / `all` | 同 `filter` | `i1` | 短路存在/全称判断 |
| `count` | 同 `filter` | `double` | 谓词为真的元素数量 |
| `find` | 同 `filter` | `ptr` | 第一个匹配元素或 `nil` |
| `findIndex` | 同 `filter` | `double` | 第一个匹配的零基索引或 `-1` |
| `reduce` | 两个参数（`cd.value.params="0,1"`）、标记 CD 返回 | `ptr` | 从左到右累积，空数组返回初始值 |

回调必须是已定义函数，并带完整 `cd.value.params` / `cd.value.return`
标记；通过 `make_function` 物化。回调帧、原生检查点（checkpoint）、
资源预算、取消和运行时类型错误均由 Rust VM 负责。

## 7. 动态值支持

- 函数边界：`cd.value.params`（索引必须严格递增、在界内）与
  `cd.value.return` 属性标记地址空间 0 指针参数/返回。
- 动态 `select`：`i1` 条件 + 双臂均为已验证 CD 令牌；普通/外地址空间/
  混合双臂被拒绝。
- 动态 PHI：结果是非空地址空间 0 `ptr` PHI，且每个入边（incoming）都有
  已验证 CD 来源（含回边携带的 PHI）；`undef`/poison/混合输入被拒绝。
- 单槽动态存储：一个直接、非 volatile、非 atomic 的 `alloca ptr`，
  只允许直接 load/store 用户；每个 store 必须携带已验证 CD 值，
  每个 load 必须被所有路径上的先写证明覆盖。

## 8. 调试与可观测性

目标侧输出三类显式调试节，供 VM 的 trace/profile/debug 使用：

- `debug_sources`：来自显式 `!cd.sources`（`path,text` 或
  `module,path,text`），UTF-8、非空、`(module,path)` 唯一。
- `debug_locations`：来自与源码表匹配的非零 `DILocation`，
  稀疏输出为 `s<source>:<line>:<column>`。
- `debug_ranges`：来自显式 `!cd.ranges`（`DILocation` + 起始/结束字节
  偏移），从不从行/列推断。

已验证的端到端调试器面（direct/machine 一致性）包括：`break`/`break-range`、
`continue`/`step`/`next`/`delete`/`quit` 及其短别名、help 输出、
行断点删除、源码定位的运行时错误暂停（除零、`sqrt(-1)`、索引越界等）、
冻结的暂停状态契约（`docs/cd-bytecode-debugger-contract.md`）。

尚未实现：`where`/`locals`/`breakpoints`/`list` 查询命令（仅有设计文档
`docs/cd-bytecode-debugger-query-design.md`）。`llc -g` 被上游驱动拒绝，
CD 调试只依赖显式元数据。

## 9. 模块产物与链接

- `-cd-artifact=module` 输出 VM 的 `artifact: module` 封装（envelope）；
  默认程序模式遇到 `!cd.module` 元数据会报错而不是静默丢弃。
- `!cd.module`：`identity`、`path`、`canonical_path`、`i1 entry`、
  可选 `entry_order`；`!cd.dependencies` 为有序 `kind`/目标身份/
  本地 lowering 锚点/请求路径记录。锚点只用于在初始化函数对应位置
  发射 `init_module mN`，不进入 0.2 工件。
- 非入口模块的顶层代码位于初始化函数，模块 `main` 是空的可执行桩；
  `llvm-link` 拼接多个 `!cd.module` 的情况被拒绝，链接由 Rust VM 的
  模块感知 `link` 完成。
- 测试框架：`llvm/utils/cd_module_link.py` 覆盖合法 direct/机器产物、
  链接执行、未链接运行拒绝、缺失依赖/环/重复身份/顺序与初始化引用错误、
  链接后诊断。

## 10. 机器后端

`-cd-backend=machine` 是显式开启的 TableGen 路径：为 `@main` 和每个
已定义函数构造机器函数（`MachineFunction`），用 CD 虚拟值伪指令表示运算，
再桥接到与 direct 相同的类型化产物模型。它与 direct 共享 `CDValueABI`
校验、`CDBytecodeFormat` 序列化和一致性（parity）验证；条件 PHI 边使用
合成的边块（edge block），符号块目标在验证前被补丁为最终指令偏移。
该路径同样只输出文本 `cdbc 0.2`，不支持对象/汇编。

## 11. 验证与质量门

- 仅 LLVM：`env -u CD_COMPILER_ROOT build-cd/bin/llvm-lit -sv llvm/test/CodeGen/CD`
  -> 126 个用例，`125 passed / 1 unsupported`。
- Direct/machine 一致性：`llvm/utils/cd_bytecode_parity.py` + 清单
  `llvm/test/CodeGen/CD/cdbc-machine-parity.list` -> 95/95。
- Rust VM：`cargo test --manifest-path cd-compiler/vm-rs/Cargo.toml`
  -> `73 + 3 + 8` 分组通过；模块链接测试框架通过。
- CI：`.github/workflows/cd-bytecode.yml` 的 `llvm-only` 与 `vm-integration`
  两个作业；工具闭包完整（含 `opt`）；托管平台重跑属于外部非阻塞项。
- 详细方法与环境见 `docs/cd-bytecode-verification.md`。

## 12. 明确不支持

- 对象文件、汇编、JIT、二进制 `.cdbc`、新产物版本、Clang CD 前端。
- `llc -g`（上游驱动边界）、从普通 LLVM 调试元数据推断源码文本。
- 未列入允许名单的原生函数名、不完整回调标记、普通指针参数、
  间接调用、逃逸/易失/原子存储、多槽 `alloca`、外地址空间指针。
- 调试器查询命令（`where`/`locals`/`breakpoints`/`list`）、表达式求值、
  条件断点、监视点（watchpoint）。
