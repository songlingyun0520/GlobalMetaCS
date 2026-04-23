# LocalMetaManagement 中文说明

`LocalMetaManagement` 是一个进程内的本地元数据管理组件，用来维护：

```text
key -> LocalMetaValue(type, location)
```

它和当前仓库中的 `GlobalMetaManagement` 分工不同：

- `GlobalMetaManagement` 负责全局定位，回答“去哪里找”
- `LocalMetaManagement` 负责本地定位，回答“本机具体放在哪里”

这里的 `value` 不是 IP 地址，也不是远端节点信息，而是本地存储位置：

- `VA`：数据位于本地 DRAM
- `LBA`：数据位于本地 SSD

## 1. 适用场景

这个组件适合以下本地数据路径：

- 本地热数据命中，直接通过 `VA` 访问 DRAM
- 冷数据落盘后，通过 `LBA` 定位本地 SSD
- 数据在 DRAM 和 SSD 之间迁移时，更新本地元数据
- 数据彻底失效后，删除本地元数据记录

典型访问流程：

1. 先查询 `LocalMetaManagement`
2. 命中 `kDramVA`，走本地内存访问路径
3. 命中 `kSsdLBA`，走本地 SSD 访问路径
4. 未命中时，再回退到全局元数据或其他路径

## 2. 核心类型

头文件见 [local_meta_management.h](/D:/github/GlobalMetaCS/local_meta_management.h)。

```cpp
enum class LocalLocationType {
    kDramVA = 0,
    kSsdLBA = 1,
};

struct LocalMetaValue {
    LocalLocationType type = LocalLocationType::kDramVA;
    std::uint64_t location = 0;
};
```

说明：

- `type == kDramVA` 时，`location` 表示本地 DRAM 中的虚拟地址
- `type == kSsdLBA` 时，`location` 表示本地 SSD 中的逻辑块地址

## 3. 对外接口

类定义：

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
};
```

返回风格与现有 `GlobalMetaManagement` 保持一致：

- 写接口返回 `vector<Status>`
- 读接口返回 `vector<Result<LocalMetaValue>>`
- 返回结果顺序与输入 `keys` 顺序一致
- 支持批内部分成功

## 4. 接口语义

### 4.1 `batchInsertLocal(keys, values)`

仅当 `key` 不存在时插入。

- 成功：返回 `OK`
- `key` 已存在：返回 `already exists`
- `key` 为空：返回 `invalid argument: key must not be empty`
- `keys` 和 `values` 长度不一致：返回 `invalid argument: keys and values size mismatch`

### 4.2 `batchQueryLocal(keys)`

查询本地位置。

- 命中：返回 `Result<LocalMetaValue>{value, OK}`
- 未命中：返回 `not found`
- `key` 为空：返回 `invalid argument: key must not be empty`

### 4.3 `batchUpdateLocal(keys, values)`

仅更新已存在的 key。

- 成功：返回 `OK`
- `key` 不存在：返回 `not found`
- `key` 为空：返回 `invalid argument`
- `type` 非法：返回 `invalid argument: invalid location type`

常见用法：

- DRAM 淘汰到 SSD：`VA -> LBA`
- SSD 回温到 DRAM：`LBA -> VA`

### 4.4 `batchDeleteLocal(keys)`

当前实现采用幂等删除语义。

- `key` 存在：删除并返回 `OK`
- `key` 不存在：仍返回 `OK`
- `key` 为空：返回 `invalid argument`

这样更适合驱逐、GC、重复清理和补偿流程。

## 5. 并发模型

当前版本采用最小实现方案：

- 内部使用一个 `std::unordered_map<std::string, LocalMetaValue>` 保存数据
- 所有公有 batch 接口共用一个 `std::mutex`
- 每次 batch 调用在持锁状态下一次完成

这套策略和现有 [global_meta_management.cpp](/D:/github/GlobalMetaCS/global_meta_management.cpp) 的粗粒度互斥风格一致，优先保证线程安全和实现简单，不追求读写锁优化。

## 6. 代码文件

本地元数据相关文件：

```text
local_meta_management.h         LocalMetaManagement 头文件
local_meta_management.cpp       LocalMetaManagement 实现
local_meta_management_test.cpp  本地测试程序
local_meta_module.md            详细设计文档
```

公共类型依赖：

```text
gmm_types.h                     Status / Result<T>
```

## 7. 测试覆盖

测试程序见 [local_meta_management_test.cpp](/D:/github/GlobalMetaCS/local_meta_management_test.cpp)，当前覆盖了这些场景：

- 批量插入后查询，验证顺序保持一致
- `VA` 和 `LBA` 两类值的插入与查询
- 重复插入返回 `already exists`
- 已存在 key 从 `VA -> LBA` 更新成功
- 更新不存在 key 返回 `not found`
- 删除不存在 key 仍返回成功
- 空 key 校验
- `keys/values` 长度不一致校验
- 非法 `LocalLocationType` 校验
- 简单并发读写 smoke test

## 8. 构建与运行

当前 `Makefile` 已增加 `local_meta_management_test` 目标。

### 方式一：使用 Makefile

```bash
make local_meta_management_test
./local_meta_management_test
```

### 方式二：手动编译

```bash
g++ -std=c++17 -Wall -O2 \
    local_meta_management.cpp \
    local_meta_management_test.cpp \
    -o local_meta_management_test \
    -lpthread
./local_meta_management_test
```

如果测试通过，程序会输出：

```text
LocalMetaManagement tests passed
```

## 9. 使用示例

```cpp
#include "local_meta_management.h"

LocalMetaManagement local;

local.batchInsertLocal(
    {"token:1", "block:9"},
    {
        {LocalLocationType::kDramVA, 0x1000},
        {LocalLocationType::kSsdLBA, 4096},
    });

auto result = local.batchQueryLocal({"token:1", "block:9", "missing"});

auto update = local.batchUpdateLocal(
    {"token:1"},
    {{LocalLocationType::kSsdLBA, 8192}});

auto del = local.batchDeleteLocal({"token:1", "missing"});
```

## 10. 与设计文档的关系

这份 README 面向“快速了解和上手”，详细设计请看：

- [local_meta_module.md](/D:/github/GlobalMetaCS/local_meta_module.md)

如果你需要了解和全局元数据模块的关系，也可以一起看：

- [global_meta_module.md](/D:/github/GlobalMetaCS/global_meta_module.md)

## 11. 当前限制

当前版本有这些明确边界：

- 只实现进程内本地元数据索引
- 不提供 RPC 接口
- 不负责 DRAM 分配和 SSD 写入本身
- 不做持久化恢复
- 不做复杂并发优化

它的职责很单一：把“本地数据位置”这件事表达清楚、管理清楚。
