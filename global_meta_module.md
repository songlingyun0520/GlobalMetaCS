# GlobalMetaManagement 模块文档

## 目录

- [模块概述](#模块概述)
- [文件结构](#文件结构)
- [模块调用关系](#模块调用关系)
- [数据模型](#数据模型)
- [RPC 接口定义](#rpc-接口定义)
- [Server 实现](#server-实现)
- [Client 封装](#client-封装)
- [测试用例](#测试用例)
- [构建说明](#构建说明)
- [线程安全](#线程安全)

---

## 模块概述

GlobalMetaManagement（GMM）是一个基于 ONC RPC（TI-RPC）的全局元数据服务，维护一张
**key → value** 的字符串映射表。不区分 TokenKey 与 BlockKey，统一以单一字符串 key 存取。

典型使用场景：

- **查询路由**：当本地缓存未命中时，调用 GMM 的 `query` 作为回退路径，获取目标节点地址。
- **节点迁移**：通过 `update` 更新已有 key 对应的 value，无需重新插入整条记录。
- **请求结束**：调用 `remove` 清理映射，释放元数据空间。

程序号 `0x20001235`，位于用户自定义区间（`0x20000000`–`0x3FFFFFFF`），与 HiIndexDRAM（`0x20001234`）相邻。

---

## 文件结构

手写文件：

```text
gmm_types.h                    共享类型：Status / Result<T>
global_meta.x                  RPC 接口描述（XDR / rpcgen 源文件）
global_meta_management.h       服务端核心类声明
global_meta_management.cpp     服务端核心类实现
global_meta_server.cpp         rpcgen _svc 分发函数实现
global_meta_client.h           客户端封装类声明
global_meta_client.cpp         客户端封装类实现
global_meta_client_test.cpp    端到端测试驱动（含 main）
```

rpcgen 由 `global_meta.x` 生成以下文件（不纳入版本控制）：

```text
global_meta.h        C 类型定义 / 宏
global_meta_xdr.c    XDR 序列化 / 反序列化
global_meta_svc.c    服务端 RPC 分发框架
global_meta_clnt.c   客户端 RPC 桩函数
```

---

## 模块调用关系

### 整体架构

```text
┌─────────────────────────────┐        ┌──────────────────────────────────┐
│         Client Side         │        │           Server Side             │
│                             │        │                                  │
│  global_meta_client_test    │        │                                  │
│           │                 │        │   GlobalMetaManagement           │
│           │ 创建/调用        │        │   (management.h / .cpp)          │
│           ▼                 │        │            ▲                     │
│  GlobalMetaClient           │        │            │ 调用                │
│  (client.h / client.cpp)    │        │   gmm_xxx_1_svc()                │
│           │                 │        │   (global_meta_server.cpp)       │
│           │ 调用 RPC 桩      │        │            ▲                     │
│           ▼                 │        │            │ 分发                │
│   global_meta_clnt.c        │        │   global_meta_svc.c              │
│   (rpcgen 生成)             │        │   (rpcgen 生成)                  │
│           │                 │        │            ▲                     │
└───────────┼─────────────────┘        └────────────┼─────────────────────┘
            │                                        │
            └──────────── TCP (RPC over TI-RPC) ─────┘
```

### 头文件依赖关系

```text
gmm_types.h  ◄──────────────────────────────────────────────┐
     ▲                                                       │
     │ #include                                             │ #include
     │                                                       │
global_meta_management.h          global_meta_client.h ─────┘
     ▲                                    ▲
     │ #include                           │ #include
     │                                    │
global_meta_server.cpp       global_meta_client.cpp
                                          ▲
                                          │ #include
                                          │
                             global_meta_client_test.cpp


global_meta.h (rpcgen 生成)
     ▲
     │ extern "C" { #include }
     └── global_meta_client.h
```

### 单次操作调用链（以 query 为例）

```text
global_meta_client_test.cpp
  client.batchQueryGlobal({"seq:001"})
    └─► GlobalMetaClient::batchQueryGlobal()           [global_meta_client.cpp]
          └─► GlobalMetaClient::rpc_query("seq:001")   [global_meta_client.cpp]
                └─► gmm_query_1(arg, clnt_)             [global_meta_clnt.c, rpcgen]
                      └─► [TCP 网络传输]
                            └─► gmm_query_1_svc()       [global_meta_server.cpp]
                                  └─► GlobalMetaManagement::batchQueryGlobal()
                                        └─► store_.find()
                                              └─► Result<string>
                                  ◄── 序列化为 gmm_query_result
                      ◄── XDR 反序列化
                └─► Result<string>
          └─► Result<string>
    └─► vector<Result<string>>
```

---

## 数据模型

### 常量

| Constant         | Value | Description     |
| ---------------- | ----- | --------------- |
| `GMM_MAX_KEY_SZ` | 256   | max key bytes   |
| `GMM_MAX_VAL_SZ` | 256   | max value bytes |

### 核心存储

GMM 内部维护一张哈希表，O(1) 平均复杂度：

```text
store_ : key → value (std::string → std::string)
```

插入时若 key 已存在，`batchInsertGlobal` 返回 `"already exists"` 错误（非覆盖写）。

---

## RPC 接口定义

程序定义（`global_meta.x`）：

```c
program GLOBALMETA_PROG {
    version GLOBALMETA_VERS {
        gmm_status_result gmm_insert (gmm_kv_arg)  = 1;
        gmm_query_result  gmm_query  (gmm_key_arg) = 2;
        gmm_status_result gmm_update (gmm_kv_arg)  = 3;
        gmm_status_result gmm_remove (gmm_key_arg) = 4;
    } = 1;
} = 0x20001235;
```

### 过程 1：gmm\_insert

插入一条 key-value 记录，key 不存在时成功；已存在时返回错误。

#### 请求

```c
struct gmm_kv_arg {
    GmmKey   key;
    GmmValue value;
};
```

**响应**：`status` 0=成功，-1=key 已存在。

---

### 过程 2：gmm\_query

按 key 查询，返回对应的 value。

#### 请求

```c
struct gmm_key_arg { GmmKey key; };
```

**响应**（discriminated union）

```c
union gmm_query_result switch (int found) {
case 1:  GmmValue value;  // 命中
default: void;            // 未命中
};
```

---

### 过程 3：gmm\_update

更新已有 key 的 value。key 不存在时返回错误。

**请求**：`gmm_kv_arg`（同 gmm_insert）

**响应**：`status` 0=成功，-1=key 不存在。

---

### 过程 4：gmm\_remove

删除指定 key 的映射。

#### 请求

```c
struct gmm_key_arg { GmmKey key; };
```

**响应**：`status` 0=成功，-1=key 不存在。

---

## Server 实现

### GlobalMetaManagement 类

声明在 `global_meta_management.h`，实现在 `global_meta_management.cpp`。
所有公有方法均持有互斥锁，线程安全。

| 方法 | 返回值 | 说明 |
| --- | --- | --- |
| `batchInsertGlobal(keys, values)` | `vector<Status>` | key 已存在时返回 error（非覆盖写） |
| `batchQueryGlobal(keys)` | `vector<Result<string>>` | 未命中返回 `Status::error("not found")` |
| `batchUpdateGlobal(keys, values)` | `vector<Status>` | key 不存在时返回 error |
| `batchDeleteGlobal(keys)` | `vector<Status>` | key 不存在时返回 error |

### _svc 分发函数（`global_meta_server.cpp`）

每个 `gmm_xxx_1_svc` 函数：

1. 以单元素 vector 调用 `GlobalMetaManagement` 对应的批量方法，取 `[0]`。
2. 将结果翻译为 rpcgen 要求的 C 结构体；query 结果写入 `static std::string` 再令 `char *` 字段指向它（满足指针生命周期要求）。
3. 打印带时间戳的操作日志。

时间戳由 `ts()` 生成，格式为 `HH:MM:SS`。服务启动日志通过 `__attribute__((constructor))` 在 `main()` 执行前打印。

---

## Client 封装

### GlobalMetaClient 类

声明在 `global_meta_client.h`，实现在 `global_meta_client.cpp`。
持有一个 `CLIENT *` 句柄，将 RPC 调用包装为类型安全的 C++ 接口，风格对齐
`metastore::RedisMetaStoreGlobalAdapter`（见 `demo.cpp`）。

#### 批量操作接口

```cpp
vector<Status>          batchInsertGlobal(const vector<string>& keys,
                                           const vector<string>& values) const;
vector<Result<string>>  batchQueryGlobal(const vector<string>& keys) const;
vector<Status>          batchUpdateGlobal(const vector<string>& keys,
                                           const vector<string>& values) const;
vector<Status>          batchDeleteGlobal(const vector<string>& keys) const;
```

批量操作在客户端循环调用对应的单操作 helper（`rpc_insert/rpc_query/rpc_update/rpc_remove`），每条记录独立返回 `Status` / `Result`。

**错误处理**：RPC 桩返回 `nullptr` 时，helper 将 `clnt_sperror()` 的描述包装为 `Status::error(message)`，不抛异常。

> `const_cast<char *>(str.c_str())` 用于向 rpcgen 生成的 `char *` 字段传参，是与 C RPC 接口交互的必要妥协。

---

## 测试用例

`global_meta_client_test.cpp` 使用与 `demo.cpp` 相同的输出辅助函数，依次执行三个场景：

| 场景 | 批量操作 | 验证点 |
| --- | --- | --- |
| 1 | `batchQueryGlobal` × 3 条 | 命中返回 value；未命中返回 FAILED(not found) |
| 2 | `batchUpdateGlobal`（存在/不存在） | 迁移后再查返回新 value；不存在的 key 返回 FAILED |
| 3 | `batchDeleteGlobal` + 再查 | 已删除返回 FAILED；未删除的条目仍可查 |

---

## 构建说明

### 第一步：生成 RPC 桩代码

```bash
rpcgen -N global_meta.x
# 生成: global_meta.h  global_meta_xdr.c  global_meta_svc.c  global_meta_clnt.c
```

### 第二步：编译 Server

```bash
g++ -std=c++17 -Wall -O2 -I/usr/include/tirpc \
    global_meta_svc.c global_meta_xdr.c \
    global_meta_management.cpp global_meta_server.cpp \
    -o gmm_server -ltirpc -lpthread
```

### 第三步：编译 Client 测试

```bash
g++ -std=c++17 -Wall -O2 -I/usr/include/tirpc \
    global_meta_clnt.c global_meta_xdr.c \
    global_meta_client.cpp global_meta_client_test.cpp \
    -o gmm_client_test -ltirpc
```

### 第四步：运行

```bash
# 终端 1：启动 Server
./gmm_server

# 终端 2：运行测试（替换为实际 Server IP）
./gmm_client_test 127.0.0.1
```

---

## 线程安全

`GlobalMetaManagement` 的所有公有方法均通过 `std::lock_guard<std::mutex>` 保护，满足并发写入要求。`batchQueryGlobal` 将 `mutex_` 声明为 `mutable`，保持 `const` 语义。

`_svc` 分发函数中的 `static` 变量（用于 RPC 返回指针的生命周期）**不是**线程安全的——TI-RPC 默认单线程分发，若启用多线程模式，需为每个过程的静态变量增加独立锁保护。
