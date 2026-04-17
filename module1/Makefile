CXX := g++
HIREDIS_CFLAGS := $(shell pkg-config --cflags hiredis)
HIREDIS_LIBS := $(shell pkg-config --libs hiredis)
CXXFLAGS := -std=c++17 -Wall -Wextra -Werror -Iinclude -I../common -Isrc -Itests $(HIREDIS_CFLAGS)
LDFLAGS := $(HIREDIS_LIBS)

SOURCES := \
	../common/metastore/redis_meta_store_backend.cpp \
	src/redis_meta_store_global_adapter.cpp \
	tests/test_main.cpp \
	tests/smoke_test.cpp \
	tests/init_reset_test.cpp \
	tests/batch_operations_test.cpp \
	tests/build_surface_test.cpp \
	tests/unix_socket_test.cpp

OBJECTS := $(SOURCES:.cpp=.o)
TEST_BIN := build/metastore_tests

.PHONY: test clean

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(OBJECTS)
	mkdir -p build
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf build src/*.o tests/*.o
