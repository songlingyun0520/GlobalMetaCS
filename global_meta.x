/*
 * global_meta.x — GlobalMetaManagement RPC 接口定义
 *
 * 用法: rpcgen -N global_meta.x
 * 生成: global_meta.h  global_meta_xdr.c  global_meta_svc.c  global_meta_clnt.c
 *
 * 四条过程（不区分 TokenKey / BlockKey，统一 key-value 存储）:
 *   1. gmm_insert — 插入 key → value 映射
 *   2. gmm_query  — 按 key 查询，返回 value
 *   3. gmm_update — 更新已有 key 的 value
 *   4. gmm_remove — 删除映射
 */

/* ── 常量 ──────────────────────────────────────────────────────────── */
const GMM_MAX_KEY_SZ = 256;    /* key 最大字节数   */
const GMM_MAX_VAL_SZ = 256;    /* value 最大字节数 */

/* ── 基础类型 ──────────────────────────────────────────────────────── */
typedef string GmmKey<GMM_MAX_KEY_SZ>;
typedef string GmmValue<GMM_MAX_VAL_SZ>;

/* ── 公共参数结构 ──────────────────────────────────────────────────── */
struct gmm_kv_arg {
    GmmKey   key;
    GmmValue value;
};

struct gmm_key_arg {
    GmmKey key;
};

/* ── 公共响应结构 ──────────────────────────────────────────────────── */
struct gmm_status_result {
    int status;   /* 0 = ok, -1 = error */
};

union gmm_query_result switch (int found) {
case 1:
    GmmValue value;
default:
    void;
};

/* ══════════════════════════════════════════════════════════════════
 * RPC 程序定义
 *   程序号 0x20001235 位于用户自定义区间 0x20000000~0x3FFFFFFF
 * ══════════════════════════════════════════════════════════════════ */
program GLOBALMETA_PROG {
    version GLOBALMETA_VERS {

        gmm_status_result gmm_insert (gmm_kv_arg)  = 1;
        gmm_query_result  gmm_query  (gmm_key_arg) = 2;
        gmm_status_result gmm_update (gmm_kv_arg)  = 3;
        gmm_status_result gmm_remove (gmm_key_arg) = 4;

    } = 1;
} = 0x20001235;
