---
name: himeta-engine
description: 设计、实现或重构 HiMetaEngine 三大类接口封装。Use when Codex needs to build a thin wrapper that exposes HiIndex, GlobalMetaClient, and LocalMetaManagement directly as three capability groups, without adding IndexToTokenKey routing, remote local-meta resolution, or end-to-end query orchestration.
---

# HiMetaEngine

将 `HiMetaEngine` 视为统一元数据入口，负责协调三类能力：

- `HiIndex`
- `GlobalMetaClient`
- `LocalMetaManagement`

不要把这三者重新揉成一个无边界的大类。保持“统一封装、内部解耦”的结构。

## 定义目标

实现一个统一入口，对外直接暴露三类能力对象：

1. `HiIndex`
2. `GlobalMetaClient`
3. `LocalMetaManagement`

优先保证以下特性：

- 输入顺序稳定
- 支持批量接口
- 支持批内部分成功
- 依赖边界明确
- 不在 `HiMetaEngine` 中引入额外调用链
- 不在 `HiMetaEngine` 中混入远端解析逻辑

## 公开接口

`HiMetaEngine` 不要把三组能力重新包装成一堆平铺的 batch 方法。对外主接口直接暴露三个大类：

- `HiIndex`
- `GlobalMetaClient`
- `LocalMetaManagement`

推荐使用“公开 accessor / 能力入口”的方式，而不是把三组方法全部重新抄一遍。

推荐调用形态：

```cpp
engine.hiIndex().BatchQueryIndex(...)
engine.globalMetaClient().batchQueryGlobal(...)
engine.localMetaManagement().batchQueryLocal(...)
```

当前版本不要新增 orchestration helper，只保留三大类接口封装。

## 推荐类型边界

如果仓库里还没有统一类型，先采用最小别名或轻量 struct，不要一次性引入大而全的类型系统。

推荐最小表达：

```cpp
using RequestID = std::uint64_t;
using LayerID = std::uint32_t;
using IndexValue = std::string;
using TokenKey = std::string;
using BlockKey = std::string;
using Address = std::string;
using IpAddress = std::string;
```

处理原则：

- `requestID` 和 `layerID` 优先使用整数类型
- `index`、`TokenKey`、`BlockKey`、`address`、`IP` 先使用字符串
- 如果项目里已经存在更强类型定义，优先复用，不要重复定义

### 1. HiIndex 分组

保留以下接口名：

```cpp
BatchQueryIndex(requestID, layerID, indexList)
BatchInsertIndex(requestID, layerID, indexList, address)
BatchDeleteIndex(requestID, layerID, indexList)
BatchUpdateIndex(requestID, layerID, indexList, address)
```

约束：

- `requestID` 和 `layerID` 当作透传上下文，不额外推导业务语义
- `indexList` 的返回顺序必须和输入顺序一致
- `BatchQueryIndex` 的单条结果只允许三种状态：
  - `address`
  - `IP`
  - `NULL`

优先使用显式 tagged result，而不是裸字符串混合返回。

推荐表达：

```cpp
enum class IndexQueryKind {
    kAddress,
    kIp,
    kNull,
};

struct IndexQueryResult {
    IndexQueryKind kind;
    std::string value;
};
```

如果项目更偏向 `std::variant`，也可以使用等价的 variant 表达，但必须保留这三种语义。

### 2. GlobalMetaClient 分组

不要重新发明 global 接口。优先直接镜像当前 `GlobalMetaClient` 的四个 batch 接口：

```cpp
batchQueryGlobal(keys)
batchInsertGlobal(keys, values)
batchUpdateGlobal(keys, values)
batchDeleteGlobal(keys)
```

处理原则：

- 保持现有 `Status / Result<T>` 风格
- 不修改 `GlobalMetaClient` 的错误语义
- 当前 query 主链路里如果暂时不直接依赖 global 查询，也保留这一组作为 `HiMetaEngine` 的统一封装入口

### 3. LocalMetaManagement 分组

保留以下接口：

```cpp
batchQueryLocal(TokenKey / BlockKey)
batchInsertLocal(...)
batchUpdateLocal(...)
batchDeleteLocal(...)
```

处理原则：

- 当前仓库还没有强类型 `TokenKey` / `BlockKey`
- 如果没有单独类型定义，优先使用 `std::string` 或轻量 type alias 表示
- 不要凭空引入复杂 key class，除非用户明确要求

## 默认职责划分

默认把 `HiMetaEngine` 设计成“统一入口 + 轻封装”，而不是编排器。

按下面规则处理：

- `HiIndex` 分组接口
  - 默认只调用 `HiIndex`
  - 不自动联动 `GlobalMetaClient` 或 `LocalMetaManagement`
- `GlobalMetaClient` 分组接口
  - 默认只调用 `GlobalMetaClient`
  - 保持当前 RPC 语义
- `LocalMetaManagement` 分组接口
  - 默认只调用本地 `LocalMetaManagement`

如果未来需要“写 index 时同步写 local/global meta”，把它设计成新的组合接口，不要偷偷改变现有 batch 接口语义。

## 推荐结构

优先把 `HiMetaEngine` 做成“直接暴露三类能力”的组合式封装：

```cpp
class HiMetaEngine {
public:
    HiIndex& hiIndex();
    GlobalMetaClient& globalMetaClient();
    LocalMetaManagement& localMetaManagement();

private:
    HiIndex* hi_index_;
    GlobalMetaClient* global_meta_client_;
    LocalMetaManagement* local_meta_management_;
};
```

如果依赖需要所有权，再改成 `unique_ptr` 或值成员。不要一开始就把生命周期策略和业务逻辑混在一起。

## 推荐辅助接口

如果开始落代码，优先补下面这个最小抽象，而不是把 `HiIndex` 实现细节写死在 `HiMetaEngine` 里。

### HiIndex 抽象

```cpp
class HiIndex {
public:
    virtual ~HiIndex() = default;

    virtual std::vector<IndexQueryResult> BatchQueryIndex(
        RequestID request_id,
        LayerID layer_id,
        const std::vector<IndexValue>& index_list) = 0;

    virtual std::vector<Status> BatchInsertIndex(
        RequestID request_id,
        LayerID layer_id,
        const std::vector<IndexValue>& index_list,
        const std::vector<Address>& address_list) = 0;

    virtual std::vector<Status> BatchUpdateIndex(
        RequestID request_id,
        LayerID layer_id,
        const std::vector<IndexValue>& index_list,
        const std::vector<Address>& address_list) = 0;

    virtual std::vector<Status> BatchDeleteIndex(
        RequestID request_id,
        LayerID layer_id,
        const std::vector<IndexValue>& index_list) = 0;
};
```

## 实现规则

实现时遵循这些规则：

- 让 `HiIndex` 只负责 `index -> address/IP/NULL`
- 让 `LocalMetaManagement` 只负责 `TokenKey/BlockKey -> address`
- 让 `GlobalMetaClient` 保持现有 global RPC 语义
- 让 `HiMetaEngine` 对外直接暴露三类能力，不重复包装它们的基础 batch 接口
- 不在 `HiMetaEngine` 中新增调用链、分桶逻辑或远端解析逻辑

避免以下错误：

- 用一个字符串同时表达 address、IP、NULL 三种语义
- 为了“统一入口”而把三类能力的所有方法重新抄写一遍
- 假设 `GlobalMetaClient` 一定已经初始化完成
- 把 `HiIndex` 的索引语义和 `LocalMetaManagement` 的 key 语义混为一谈

## 验证重点

至少覆盖以下场景：

- `HiMetaEngine` 能直接暴露注入的 `HiIndex`
- `HiMetaEngine` 能直接暴露注入的 `LocalMetaManagement`
- `GlobalMetaClient` 未注入时，访问行为明确
- 通过 `engine.hiIndex()` 调用批量接口时，结果来自真实注入对象
- 通过 `engine.localMetaManagement()` 调用批量接口时，结果来自真实注入对象

## 当前仓库下的默认假设

在当前仓库里工作时，默认采用这些假设，除非用户明确推翻：

- `GlobalMetaClient` 已存在，可直接复用
- `LocalMetaManagement` 已存在，可直接复用
- `HiIndex` 还不存在，需要定义抽象接口或占位封装
- 当前版本不实现 `IndexToTokenKey`
- 当前版本不实现远端 `LocalMetaManagement` 调用链
