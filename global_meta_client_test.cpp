/*
 * global_meta_client_test.cpp
 *
 * End-to-end demo for the GlobalMetaManagement RPC interface.
 * Interface and output style mirrors demo.cpp / metastore::RedisMetaStoreGlobalAdapter.
 *
 * Build (after rpcgen -N global_meta.x):
 *   g++ -std=c++17 -Wall -O2 -I/usr/include/tirpc \
 *       global_meta_clnt.o global_meta_xdr.o \
 *       global_meta_client.o global_meta_client_test.o \
 *       -o gmm_client_test -ltirpc
 *
 * Usage: ./gmm_client_test <server_ip>
 */

#include "global_meta_client.h"

#include <ctime>
#include <iostream>
#include <string>
#include <vector>

namespace {

void PrintBatchStatuses(const std::string &step,
                        const std::vector<Status> &statuses) {
    std::cout << step << ":\n";
    for (std::size_t i = 0; i < statuses.size(); ++i) {
        const auto &s = statuses[i];
        std::cout << "  [" << i << "] " << (s.ok() ? "OK" : "FAILED");
        if (!s.ok()) {
            std::cout << " (" << s.message() << ")";
        }
        std::cout << '\n';
    }
}

void PrintQueryResults(const std::string &step,
                       const std::vector<std::string> &keys,
                       const std::vector<Result<std::string>> &results) {
    std::cout << step << ":\n";
    for (std::size_t i = 0; i < results.size(); ++i) {
        const auto &r = results[i];
        std::cout << "  key=" << keys[i] << " -> ";
        if (r.ok()) {
            std::cout << r.value;
        } else {
            std::cout << "FAILED (" << r.status.message() << ")";
        }
        std::cout << '\n';
    }
}

}  // namespace

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <server_ip>\n";
        return 1;
    }

    CLIENT *clnt = clnt_create(argv[1], GLOBALMETA_PROG, GLOBALMETA_VERS, "tcp");
    if (clnt == nullptr) {
        clnt_pcreateerror(argv[1]);
        return 1;
    }

    GlobalMetaClient client(clnt);
    /* 打印当前连接到的服务端地址和 RPC program number，方便排查连接问题。 */
    std::cout << "[GMM Client] Connected to " << argv[1]
              << " (prog=0x20001235)\n\n";

    /* 为本次测试生成唯一前缀，避免多次运行时和旧数据冲突。 */
    const std::string prefix = "client_test:" + std::to_string(std::time(nullptr));
    /* 这 3 个 key 会贯穿插入、查询、更新、删除整个测试流程。 */
    const std::vector<std::string> keys = {
        prefix + ":001",
        prefix + ":002",
        prefix + ":003"
    };
    const std::vector<std::string> values = {"10.0.1.1", "10.0.1.2", "10.0.1.3"};
    /* 故意准备一个不存在的 key，用来验证失败分支。 */
    const std::string missing_key = prefix + ":999";

    /* 打印本次测试使用的 key 前缀，便于和服务端日志对照。 */
    std::cout << "[GMM Client] Test key prefix: " << prefix << "\n\n";

    /* 场景 1：批量插入。 */
    PrintBatchStatuses("batchInsertGlobal",
                       client.batchInsertGlobal(keys, values));

    PrintQueryResults("batchQueryGlobal (after insert)",
                      keys, client.batchQueryGlobal(keys));

    /* 场景 2：重复插入，预期已存在的 key 返回失败。 */
    PrintBatchStatuses("batchInsertGlobal (duplicate keys)",
                       client.batchInsertGlobal({keys[0], keys[2]},
                                                {"10.0.9.1", "10.0.9.3"}));

    /* 场景 3：更新已存在 key，并验证不存在 key 的失败返回。 */
    PrintBatchStatuses("batchUpdateGlobal",
                       client.batchUpdateGlobal({keys[1], keys[2], missing_key},
                                                {"10.0.2.2", "10.0.2.3", "10.0.9.9"}));

    PrintQueryResults("batchQueryGlobal (after update)",
                      {keys[1], keys[2]},
                      client.batchQueryGlobal({keys[1], keys[2]}));

    /* 场景 4：删除已存在 key，并验证不存在 key 的失败返回。 */
    PrintBatchStatuses("batchDeleteGlobal",
                       client.batchDeleteGlobal({keys[0], keys[1], keys[2], missing_key}));

    PrintQueryResults("batchQueryGlobal (after delete)",
                      keys, client.batchQueryGlobal(keys));

    clnt_destroy(clnt);
    std::cout << "\n>> All GlobalMetaManagement RPC calls completed\n";
    return 0;
}
