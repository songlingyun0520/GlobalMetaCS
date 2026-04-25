# Makefile — GlobalMetaManagement RPC
#
# 依赖:
#   Ubuntu/Debian: sudo apt install libtirpc-dev rpcbind g++ make

CXX      = g++
CC       = gcc
HIREDIS_CFLAGS := $(shell pkg-config --cflags hiredis)
HIREDIS_LIBS   := $(shell pkg-config --libs hiredis)
CPPFLAGS = -I/usr/include/tirpc -I. $(HIREDIS_CFLAGS)
CXXFLAGS = -Wall -O2 -std=c++17
CFLAGS   = -Wall -O2
LIBS     = -ltirpc -lpthread $(HIREDIS_LIBS)

# ── GlobalMetaManagement (§5.12) ───────────────────────────────────
GMM_PROG        = global_meta
GMM_GENERATED   = $(GMM_PROG).h $(GMM_PROG)_xdr.c $(GMM_PROG)_svc.c $(GMM_PROG)_clnt.c
GMM_SERVER_OBJS = $(GMM_PROG)_svc.o $(GMM_PROG)_xdr.o \
                  global_meta_server.o \
                  common/metastore/redis_meta_store_backend.o \
                  metastoreglobal/src/redis_meta_store_global_adapter.o
GMM_CLIENT_OBJS = $(GMM_PROG)_clnt.o $(GMM_PROG)_xdr.o \
                  global_meta_client.o global_meta_client_test.o
LOCAL_META_OBJS = local_meta_management.o local_meta_management_test.o
HIMETA_OBJS = himeta_engine.o himeta_engine_test.o local_meta_management.o

.PHONY: all clean

all: gmm_server gmm_client_test local_meta_management_test himeta_engine_test

# ── rpcgen 代码生成 ─────────────────────────────────────────────────
$(GMM_GENERATED): $(GMM_PROG).x
	rpcgen -N $(GMM_PROG).x

# ── 编译规则 ────────────────────────────────────────────────────────
%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# ── 头文件依赖 ──────────────────────────────────────────────────────
$(GMM_PROG)_svc.o $(GMM_PROG)_clnt.o $(GMM_PROG)_xdr.o: $(GMM_PROG).h
global_meta_server.o:     $(GMM_PROG).h metastoreglobal/include/metastore/redis_meta_store_global_adapter.h common/metastore/meta_store_types.h
common/metastore/redis_meta_store_backend.o: common/metastore/redis_meta_store_backend.h common/metastore/meta_store_backend.h common/metastore/meta_store_types.h metastoreglobal/include/metastore/status.h
metastoreglobal/src/redis_meta_store_global_adapter.o: metastoreglobal/include/metastore/redis_meta_store_global_adapter.h common/metastore/redis_meta_store_backend.h
global_meta_client.o:     $(GMM_PROG).h global_meta_client.h gmm_types.h
global_meta_client_test.o: global_meta_client.h gmm_types.h
local_meta_management.o: local_meta_management.h gmm_types.h
local_meta_management_test.o: local_meta_management.h gmm_types.h
himeta_engine.o: himeta_engine.h hi_index.h local_meta_management.h gmm_types.h
himeta_engine_test.o: himeta_engine.h hi_index.h local_meta_management.h gmm_types.h

# ── 链接目标 ────────────────────────────────────────────────────────
gmm_server: $(GMM_GENERATED) $(GMM_SERVER_OBJS)
	$(CXX) -o $@ $(GMM_SERVER_OBJS) $(LIBS)

gmm_client_test: $(GMM_GENERATED) $(GMM_CLIENT_OBJS)
	$(CXX) -o $@ $(GMM_CLIENT_OBJS) $(LIBS)

local_meta_management_test: $(LOCAL_META_OBJS)
	$(CXX) -o $@ $(LOCAL_META_OBJS) -lpthread

himeta_engine_test: $(HIMETA_OBJS)
	$(CXX) -o $@ $(HIMETA_OBJS) -lpthread

clean:
	rm -f gmm_server gmm_client_test local_meta_management_test himeta_engine_test \
	      $(GMM_GENERATED) $(GMM_SERVER_OBJS) $(GMM_CLIENT_OBJS) $(LOCAL_META_OBJS) $(HIMETA_OBJS)
