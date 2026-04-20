# GlobalMetaManagement RPC

This directory contains a small ONC RPC / TI-RPC example service for a
process-local in-memory `key -> value` store.

The service exposes four RPC operations:

- `insert`
- `query`
- `update`
- `remove`

The client side provides a C++ wrapper named `GlobalMetaClient`, and the
end-to-end test program `gmm_client_test` exercises insert, query, duplicate
insert, update, and delete scenarios.

## Files

```text
global_meta.x                  RPC interface definition for rpcgen
gmm_types.h                    Shared C++ types: Status / Result<T>
global_meta_management.h/.cpp  In-memory server-side storage logic
global_meta_server.cpp         RPC service handlers
global_meta_client.h/.cpp      C++ client wrapper
global_meta_client_test.cpp    End-to-end client test program
Makefile                       Build script
global_meta_module.md          Detailed design notes
```

`rpcgen` generates these files from `global_meta.x` during build:

```text
global_meta.h
global_meta_xdr.c
global_meta_svc.c
global_meta_clnt.c
```

## Requirements

This module is intended to be built on Linux or WSL, for example Ubuntu or
Debian.

Install the required tools first:

```bash
sudo apt update
sudo apt install -y g++ gcc make rpcbind libtirpc-dev rpcsvc-proto
```

## Quick Start

### 1. Enter the directory

```bash
cd global_meta_CS
```

### 2. Start `rpcbind`

The generated RPC server normally needs `rpcbind` to be running so it can
register the program number.

If your system uses `systemd`:

```bash
sudo systemctl start rpcbind
```

If your environment does not use `systemd`, run:

```bash
rpcbind -w
```

### 3. Build

Use the provided Makefile:

```bash
make
```

This will:

1. Run `rpcgen -N global_meta.x`
2. Build `gmm_server`
3. Build `gmm_client_test`

To remove generated files and binaries:

```bash
make clean
```

### 4. Start the server

Run this in terminal 1:

```bash
./gmm_server
```

The server prints startup logs and then prints one log line per RPC request.

### 5. Run the client test

Run this in terminal 2:

```bash
./gmm_client_test 127.0.0.1
```

If the server is running on another machine, replace `127.0.0.1` with the
actual server IP.

## What the Test Program Does

`gmm_client_test` currently performs the following sequence:

1. Batch-insert 3 records
2. Query the inserted records
3. Try to insert duplicate keys and expect failure
4. Update existing keys and also try one missing key
5. Delete existing keys and also try one missing key
6. Query again after delete

The test program generates a unique key prefix using the current timestamp, so
it is safe to run repeatedly against the same server process.

## Manual Build Commands

If you prefer not to use `make`, you can build everything manually.

### 1. Generate RPC stubs

```bash
rpcgen -N global_meta.x
```

### 2. Build the server

```bash
gcc -Wall -O2 -I/usr/include/tirpc \
    -c global_meta_svc.c -o global_meta_svc.o
gcc -Wall -O2 -I/usr/include/tirpc \
    -c global_meta_xdr.c -o global_meta_xdr.o
g++ -std=c++17 -Wall -O2 -I/usr/include/tirpc \
    -c global_meta_management.cpp -o global_meta_management.o
g++ -std=c++17 -Wall -O2 -I/usr/include/tirpc \
    -c global_meta_server.cpp -o global_meta_server.o
g++ -o gmm_server \
    global_meta_svc.o global_meta_xdr.o \
    global_meta_management.o global_meta_server.o \
    -ltirpc -lpthread
```

### 3. Build the client test

```bash
gcc -Wall -O2 -I/usr/include/tirpc \
    -c global_meta_clnt.c -o global_meta_clnt.o
gcc -Wall -O2 -I/usr/include/tirpc \
    -c global_meta_xdr.c -o global_meta_xdr.o
g++ -std=c++17 -Wall -O2 -I/usr/include/tirpc \
    -c global_meta_client.cpp -o global_meta_client.o
g++ -std=c++17 -Wall -O2 -I/usr/include/tirpc \
    -c global_meta_client_test.cpp -o global_meta_client_test.o
g++ -o gmm_client_test \
    global_meta_clnt.o global_meta_xdr.o \
    global_meta_client.o global_meta_client_test.o \
    -ltirpc
```

## RPC Summary

Defined in `global_meta.x`:

- Program number: `0x20001235`
- Version: `1`
- Transport used by the test client: `tcp`

Operations:

- `gmm_insert(gmm_kv_arg)`: insert one `key -> value`; fails if key exists
- `gmm_query(gmm_key_arg)`: query one key
- `gmm_update(gmm_kv_arg)`: update one existing key; fails if key is missing
- `gmm_remove(gmm_key_arg)`: delete one key; fails if key is missing

Response conventions:

- `status = 0` means success
- `status = -1` means failure
- query returns `found = 0` when the key does not exist

## Troubleshooting

### `rpcgen: command not found`

Install:

```bash
sudo apt install -y rpcsvc-proto
```

### Missing `tirpc/rpc/rpc.h` or link failure for `-ltirpc`

Install:

```bash
sudo apt install -y libtirpc-dev
```

### The server fails to start or cannot register the RPC service

Usually `rpcbind` is not running yet:

```bash
sudo systemctl start rpcbind
```

You can inspect registered RPC programs with:

```bash
rpcinfo -p
```

### The client cannot connect

Check:

- the server process is running
- the IP passed to `gmm_client_test` is correct
- the server machine allows the required RPC network access

## Related Files

- [global_meta_module.md](./global_meta_module.md)
- [global_meta_client_test.cpp](./global_meta_client_test.cpp)
- [global_meta_server.cpp](./global_meta_server.cpp)
