# LocalMetaManagement 设计文档

## 1. 设计目标

`LocalMetaManagement` 用于维护当前节点本地元数据索引，提供按 `key` 查询本地数据位置的能力。
与 `GlobalMetaManagement` 中 `key -> string(IP/远端位置)` 的语义不同，`LocalMetaManagement`
维护的是 `key -> LocalMetaValue` 的映射，其中 `LocalMetaValue` 表示数据当前位于：

- 本地 DRAM：使用 `VA` 表示虚拟地址
- 本地 SSD：使用 `LBA` 表示逻辑块地址

该类的目标是为本地热数据命中、冷热迁移、淘汰删除提供统一的批量接口。

## 2. 设计范围

### 2.1 本次纳入范围

- 新增一个进程内类 `LocalMetaManagement`
- 提供批量插入、查询、更新、删除接口
- 明确 `VA/LBA` 的类型表达方式与校验约束
- 明确并发模型、批处理语义和错误语义

### 2.2 本次不纳入范围

- 不新增 RPC 接口
- 不负责实际 DRAM 分配、SSD 写入、数据搬迁
- 不负责全局路由或远端节点定位
- 不引入持久化恢复机制

V1 先实现为“本地进程内元数据索引”，后续如有需要，再考虑与 RPC 或 shared backend 打通。

## 3. 模块职责

`LocalMetaManagement` 只负责回答一个问题：

`某个 key 当前在本地哪里？`

因此它的职责边界如下：

- 输入：业务 `key`
- 输出：本地位置描述 `LocalMetaValue`
- 管理对象：`key -> (位置类型, 位置值)`
- 不关心：该 key 对应的数据内容本身

建议与现有模块的关系如下：

- `GlobalMetaManagement`：负责“全局去哪里找”
- `LocalMetaManagement`：负责“本机具体放在哪里”

典型访问路径：

1. 先查 `LocalMetaManagement`
2. 若命中 `VA`，直接访问本地 DRAM
3. 若命中 `LBA`，走本地 SSD 读取路径
4. 若未命中，再回退到 `GlobalMetaManagement` 或其他全局路径

## 4. 核心数据模型

### 4.1 类型定义

建议不要继续使用 `std::string value` 表示本地位置，而应使用强类型结构，避免把
“IP 地址 / 节点地址 / 位置地址”混在一起。

建议新增如下类型：

```cpp
enum class LocalLocationType {
    kDramVA = 0,
    kSsdLBA = 1,
};

struct LocalMetaValue {
    LocalLocationType type;
    std::uint64_t location = 0;
};
```

含义如下：

- `type == kDramVA`：`location` 表示本地 DRAM 中的虚拟地址 `VA`
- `type == kSsdLBA`：`location` 表示本地 SSD 中的逻辑块地址 `LBA`

### 4.2 为什么采用强类型

如果继续沿用 `std::string value`，会有几个问题：

- 调用方无法区分当前值到底是 `VA` 还是 `LBA`
- 后续很容易把全局 value 和本地 value 混用
- 单测和日志中缺少显式语义
- 未来扩展到更多本地层级时，不便演进

因此 V1 即建议将本地 value 从“字符串值”升级为“位置类型 + 位置数值”。

### 4.3 存储结构

建议内部使用：

```cpp
std::unordered_map<std::string, LocalMetaValue> store_;
```

语义为：

```text
key -> LocalMetaValue(type, location)
```

示例：

- `token:001 -> {kDramVA, 0x7f0012345000}`
- `block:888 -> {kSsdLBA, 1048576}`

## 5. 类接口设计

### 5.1 建议类定义

考虑当前仓库 C++ 命名风格，类名建议使用 `LocalMetaManagement`。

```cpp
class LocalMetaManagement {
public:
    std::vector<Status> batchInsertLocal(
        const std::vector<std::string>& keys,
        const std::vector<LocalMetaValue>& values);

    std::vector<Result<LocalMetaValue>> batchQueryLocal(
        const std::vector<std::string>& keys) const;

    std::vector<Status> batchUpdateLocal(
        const std::vector<std::string>& keys,
        const std::vector<LocalMetaValue>& values);

    std::vector<Status> batchDeleteLocal(
        const std::vector<std::string>& keys);

private:
    std::unordered_map<std::string, LocalMetaValue> store_;
    mutable std::mutex mutex_;
};
```

说明：

- 这里将需求里的 `batchUpdateLoacl` 统一规范为 `batchUpdateLocal`
- 如果上层接口已经依赖错误拼写，可以在实现阶段额外提供一个兼容转发函数

### 5.2 返回类型建议

- `batchInsertLocal`：返回 `vector<Status>`
- `batchQueryLocal`：返回 `vector<Result<LocalMetaValue>>`
- `batchUpdateLocal`：返回 `vector<Status>`
- `batchDeleteLocal`：返回 `vector<Status>`

原因：

- 与现有 `GlobalMetaManagement` 的批量接口风格一致
- 允许批内逐条返回状态，支持部分成功
- `query` 返回强类型位置结果，避免后续二次解析字符串

## 6. 接口语义定义

### 6.1 通用规则

所有批量接口遵循以下统一规则：

- 返回结果顺序与输入 `keys` 顺序完全一致
- 支持批内部分成功，不要求整批原子成功
- 空输入返回空结果
- `keys.size() != values.size()` 时，返回逐条 `InvalidArgument`
- `key` 为空字符串时，返回 `InvalidArgument`

### 6.2 `batchInsertLocal(keys, values)`

语义：仅当 `key` 不存在时插入。

- 成功：返回 `OK`
- `key` 已存在：返回 `AlreadyExists`
- 参数非法：返回 `InvalidArgument`

用途：

- 新写入数据先落本地 DRAM，建立 `key -> VA`
- 冷数据直接写入本地 SSD，建立 `key -> LBA`

### 6.3 `batchQueryLocal(keys)`

语义：查询本地位置映射。

- 命中：返回 `Result<LocalMetaValue>{OK, value}`
- 未命中：返回 `NotFound`
- 参数非法：返回 `InvalidArgument`

### 6.4 `batchUpdateLocal(keys, values)`

语义：仅更新已存在 key 的位置。

- 成功：返回 `OK`
- `key` 不存在：返回 `NotFound`
- 参数非法：返回 `InvalidArgument`

典型场景：

- 数据从 DRAM 淘汰到 SSD：`VA -> LBA`
- 数据从 SSD 回温到 DRAM：`LBA -> VA`

### 6.5 `batchDeleteLocal(keys)`

建议 V1 采用“幂等删除”语义：

- `key` 存在：删除后返回 `OK`
- `key` 不存在：仍返回 `OK`
- 参数非法：返回 `InvalidArgument`

这样设计的原因：

- 更适合驱逐、回收、重复清理等本地管理流程
- 与当前 shared backend 中 `DEL` 的幂等风格更一致
- 能减少上层状态同步时的异常分支

如果后续你更希望与当前 `GlobalMetaManagement` 演示类保持一致，也可以切换为：

- `key` 不存在时返回 `NotFound`

但建议在系统内统一一种删除语义，不要本地/全局长期分叉。

## 7. 并发设计

### 7.1 锁模型

当前阶段建议采用最小可落地方案：使用单个 `std::mutex` 提供基本线程安全。

- 所有公有 batch 接口统一使用同一个 `std::mutex`
- 每次 batch 调用在持锁状态下完成
- 目标是保证线程安全和语义清晰，而不是优化读写并发性能

原因：

- 与当前 `GlobalMetaManagement` 的 `std::mutex + std::lock_guard` 实现风格保持一致
- 当前阶段重点是先完成本地元数据语义和接口设计，不引入额外并发复杂度
- 粗粒度互斥更容易实现、测试和维护

### 7.2 批操作可见性

建议每个 batch 调用在持锁状态下一次完成，作为实现建议：

- 单个 batch 内不会被其他写操作打断
- 同一个 batch 内不拆分加锁/解锁

当前阶段不额外展开更细粒度的并发可见性承诺，先保证基本线程安全即可。

## 8. 校验策略

### 8.1 key 校验

- 不允许空字符串
- 不对 key 内容做业务语义解析

### 8.2 value 校验

V1 建议至少校验：

- `type` 必须是 `kDramVA` 或 `kSsdLBA`

V1 不强制校验：

- `location != 0`

原因：

- `LBA = 0` 在某些实现里可能是合法值
- 是否允许 `VA = 0` 更接近上层内存分配协议，不适合在元数据层硬编码

如果后续调用链明确禁止空地址，可在实现阶段增加可选断言或 debug 校验。

## 9. 与现有 GlobalMetaManagement 的关系

| 维度 | GlobalMetaManagement | LocalMetaManagement |
| --- | --- | --- |
| 管理范围 | 全局 | 单机本地 |
| value 语义 | 远端地址/通用字符串 | 本地位置 `VA/LBA` |
| 典型用途 | 路由、定位、回退 | 本地命中、本地迁移、淘汰 |
| 是否需要 RPC | 是，可对外暴露 | V1 否，先做进程内类 |
| 查询返回 | `Result<string>` | `Result<LocalMetaValue>` |

建议两者分层而不是复用同一个 `string value` 模型，否则本地/全局语义会混淆。

## 10. 典型业务流程

### 10.1 新数据写入本地 DRAM

```text
写入数据到本地 DRAM
    -> 生成 VA
    -> batchInsertLocal(keys, {{kDramVA, va}, ...})
```

### 10.2 DRAM 淘汰到本地 SSD

```text
数据刷入本地 SSD
    -> 获得 LBA
    -> batchUpdateLocal(keys, {{kSsdLBA, lba}, ...})
```

### 10.3 SSD 回温到 DRAM

```text
数据从 SSD 读回 DRAM
    -> 获得新 VA
    -> batchUpdateLocal(keys, {{kDramVA, va}, ...})
```

### 10.4 数据彻底失效

```text
释放本地资源
    -> batchDeleteLocal(keys)
```

## 11. 测试建议

建议新增 `local_meta_management_test.cpp`，至少覆盖以下场景：

- 批量插入后查询，结果顺序与输入一致
- 同批同时插入 `VA` 和 `LBA` 两类值
- 已存在 key 再插入，返回 `AlreadyExists`
- 已存在 key 从 `VA -> LBA` 更新成功
- 已存在 key 从 `LBA -> VA` 更新成功
- 更新不存在 key，返回 `NotFound`
- 删除存在 key 后再查询，返回 `NotFound`
- 删除不存在 key 的幂等语义
- 空 key 返回 `InvalidArgument`
- `keys/values` 长度不一致时返回 `InvalidArgument`
- 并发读写下无崩溃、无数据竞争

## 12. 建议落地文件

后续实现阶段建议新增以下文件：

```text
local_meta_management.h
local_meta_management.cpp
local_meta_management_test.cpp
local_meta_module.md
```

建议头文件中补充：

- `LocalLocationType`
- `LocalMetaValue`
- `LocalMetaManagement`

## 13. 实现建议

### 13.1 V1 最小实现

V1 建议先采用最简单、最稳妥的实现：

- `unordered_map<string, LocalMetaValue>`
- `mutex`
- 与现有 `Status` / `Result<T>` 风格保持一致
- 不引入序列化、RPC、持久化

这样可以先把“本地位置语义”从“全局字符串 value 语义”里拆出来，并与当前
`GlobalMetaManagement` 的粗粒度互斥策略保持一致。V1 不做读写锁优化；如果后续出现
明显的读多写少瓶颈，再考虑升级为 `shared_mutex`。

### 13.2 后续可扩展方向

后续如需要，可继续扩展：

- 增加 `size` / `version` / `timestamp` / `refcount`
- 增加更多本地层级，如 `PMem`、`NVMe namespace`
- 提供条件更新接口，如 CAS 或 compare-and-swap
- 提供批量迁移辅助接口，如 `promoteToDram()` / `demoteToSsd()`

## 14. 结论

`LocalMetaManagement` 不应复用 `GlobalMetaManagement` 的 `string value` 模型，而应引入
强类型的本地位置描述 `LocalMetaValue(type, location)`。

推荐方案如下：

- 类名使用 `LocalMetaManagement`
- value 使用 `{type, location}` 表示 `VA/LBA`
- 接口使用批量风格：`batchInsertLocal / batchQueryLocal / batchUpdateLocal / batchDeleteLocal`
- V1 仅做进程内本地索引，不加 RPC
- 删除语义建议采用幂等删除

该设计能把“全局路由”和“本地定位”清晰分层，也便于后续真正落代码和单测。
