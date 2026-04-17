#pragma once

#include <cstdlib>
#include <csignal>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <chrono>
#include <fcntl.h>
#include <unistd.h>
#include <vector>

#include "metastore/redis_meta_store_global_adapter.h"

struct TestCaseEntry {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCaseEntry>& TestRegistry() {
    static std::vector<TestCaseEntry> tests;
    return tests;
}

struct TestRegistrar {
    TestRegistrar(const std::string& name, std::function<void()> fn) {
        TestRegistry().push_back({name, std::move(fn)});
    }
};

#define TEST_CASE(name)                                                         \
    void test_##name();                                                         \
    static TestRegistrar registrar_##name(#name, test_##name);                  \
    void test_##name()

#define FAIL(message)                                                           \
    do {                                                                        \
        std::ostringstream oss;                                                 \
        oss << __FILE__ << ":" << __LINE__ << ": " << message;                  \
        throw std::runtime_error(oss.str());                                    \
    } while (false)

#define EXPECT_TRUE(expr)                                                       \
    do {                                                                        \
        if (!(expr)) {                                                          \
            FAIL("EXPECT_TRUE failed: " #expr);                                 \
        }                                                                       \
    } while (false)

#define EXPECT_EQ(lhs, rhs)                                                     \
    do {                                                                        \
        if (!((lhs) == (rhs))) {                                                \
            FAIL("EXPECT_EQ failed: " #lhs " == " #rhs);                        \
        }                                                                       \
    } while (false)

#define REQUIRE_OK(status_expr)                                                 \
    do {                                                                        \
        auto status_value = (status_expr);                                      \
        if (!status_value.ok()) {                                               \
            FAIL(std::string("REQUIRE_OK failed: ") + status_value.message());  \
        }                                                                       \
    } while (false)

#define EXPECT_ALL_OK(statuses_expr)                                            \
    do {                                                                        \
        auto statuses_value = (statuses_expr);                                  \
        for (const auto& status : statuses_value) {                             \
            if (!status.ok()) {                                                 \
                FAIL(std::string("EXPECT_ALL_OK failed: ") + status.message()); \
            }                                                                   \
        }                                                                       \
    } while (false)

inline metastore::MetaStoreInitOptions TestRedisOptions() {
    metastore::MetaStoreInitOptions options;
    options.backend = metastore::BackendKind::kRedis;
    options.endpoint = "127.0.0.1:6379";
    options.enable_clear = true;
    options.db_exclusive = true;
    return options;
}

inline std::string MakeTempDir() {
    std::string templ = "/tmp/metastore-redis-XXXXXX";
    std::vector<char> buffer(templ.begin(), templ.end());
    buffer.push_back('\0');
    char* path = mkdtemp(buffer.data());
    if (path == nullptr) {
        FAIL("mkdtemp failed");
    }
    return std::string(path);
}

inline bool PathExists(const std::string& path) {
    struct stat st {};
    return stat(path.c_str(), &st) == 0;
}

struct UnixSocketRedisServer {
    std::string dir;
    std::string socket_path;
    pid_t pid = -1;

    static UnixSocketRedisServer Start() {
        UnixSocketRedisServer server;
        server.dir = MakeTempDir();
        server.socket_path = server.dir + "/redis.sock";

        pid_t child = fork();
        if (child < 0) {
            FAIL("fork failed");
        }
        if (child == 0) {
            int devnull = open("/dev/null", O_WRONLY);
            if (devnull >= 0) {
                dup2(devnull, STDOUT_FILENO);
                dup2(devnull, STDERR_FILENO);
                close(devnull);
            }
            execlp("redis-server", "redis-server",
                   "--save", "",
                   "--appendonly", "no",
                   "--port", "0",
                   "--unixsocket", server.socket_path.c_str(),
                   "--dir", server.dir.c_str(),
                   static_cast<char*>(nullptr));
            _exit(127);
        }

        server.pid = child;
        for (int i = 0; i < 50; ++i) {
            if (PathExists(server.socket_path)) {
                return server;
            }

            int status = 0;
            pid_t done = waitpid(server.pid, &status, WNOHANG);
            if (done == server.pid) {
                FAIL("redis-server exited before socket became ready");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        kill(server.pid, SIGTERM);
        waitpid(server.pid, nullptr, 0);
        FAIL("redis unix socket did not become ready");
    }

    ~UnixSocketRedisServer() {
        if (pid <= 0) {
            return;
        }
        kill(pid, SIGTERM);
        waitpid(pid, nullptr, 0);
        unlink(socket_path.c_str());
        rmdir(dir.c_str());
    }
};
