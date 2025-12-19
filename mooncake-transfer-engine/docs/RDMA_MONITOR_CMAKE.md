# RDMA Traffic Monitor - CMakeLists.txt 集成指南

## 添加到现有的 CMakeLists.txt

在 Mooncake/mooncake-transfer-engine 的 CMakeLists.txt 中添加以下内容：

```cmake
# ============================================================================
# RDMA Traffic Monitor Library
# ============================================================================

add_library(rdma_traffic_monitor STATIC
    src/rdma_traffic_monitor.cpp
)

target_include_directories(rdma_traffic_monitor
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(rdma_traffic_monitor
    PUBLIC
    glog::glog
    PRIVATE
    rt          # 共享内存支持 (POSIX shm_open)
    pthread     # 线程和信号量支持
)

# ============================================================================
# 更新 mooncake-transfer-engine 库的依赖
# ============================================================================

# 找到现有的 target_link_libraries(mooncake-transfer-engine ...)
# 并添加 rdma_traffic_monitor

target_link_libraries(mooncake-transfer-engine
    PRIVATE
    rdma_traffic_monitor  # 新增
    # ... 保留其他现有依赖 ...
)

# ============================================================================
# RDMA Monitor 示例程序（可选）
# ============================================================================

add_executable(rdma_monitor_example
    examples/rdma_monitor_example.cpp
)

target_link_libraries(rdma_monitor_example
    PRIVATE
    rdma_traffic_monitor
    mooncake-transfer-engine
    glog::glog
)

# ============================================================================
# 安装配置（可选）
# ============================================================================

install(TARGETS rdma_traffic_monitor
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES
    include/rdma_traffic_monitor.h
    DESTINATION include/mooncake
)
```

## 完整的独立 CMakeLists.txt 示例

如果需要单独编译 RDMA Monitor，可以使用以下 CMakeLists.txt：

```cmake
cmake_minimum_required(VERSION 3.18)
project(rdma_traffic_monitor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================================
# 查找依赖
# ============================================================================

find_package(glog REQUIRED)
find_package(Threads REQUIRED)

# ============================================================================
# RDMA Traffic Monitor 库
# ============================================================================

add_library(rdma_traffic_monitor STATIC
    src/rdma_traffic_monitor.cpp
)

target_include_directories(rdma_traffic_monitor
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(rdma_traffic_monitor
    PUBLIC
    glog::glog
    PRIVATE
    rt
    Threads::Threads
)

# ============================================================================
# 编译选项
# ============================================================================

target_compile_options(rdma_traffic_monitor PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:Debug>:-O0 -g>
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
)

# ============================================================================
# 示例程序
# ============================================================================

option(BUILD_EXAMPLES "Build example programs" ON)

if(BUILD_EXAMPLES)
    # 需要链接 mooncake-transfer-engine
    # 这里假设已经安装或者作为子项目存在
    
    add_executable(rdma_monitor_example
        examples/rdma_monitor_example.cpp
    )
    
    target_link_libraries(rdma_monitor_example
        PRIVATE
        rdma_traffic_monitor
        # mooncake-transfer-engine  # 如果可用
        glog::glog
    )
endif()

# ============================================================================
# 测试（可选）
# ============================================================================

option(BUILD_TESTS "Build test programs" OFF)

if(BUILD_TESTS)
    enable_testing()
    
    add_executable(rdma_monitor_test
        tests/rdma_monitor_test.cpp
    )
    
    target_link_libraries(rdma_monitor_test
        PRIVATE
        rdma_traffic_monitor
        glog::glog
    )
    
    add_test(NAME rdma_monitor_test COMMAND rdma_monitor_test)
endif()

# ============================================================================
# 安装
# ============================================================================

install(TARGETS rdma_traffic_monitor
    EXPORT rdma_traffic_monitor-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES
    include/rdma_traffic_monitor.h
    DESTINATION include/mooncake
)

install(EXPORT rdma_traffic_monitor-targets
    FILE rdma_traffic_monitor-targets.cmake
    NAMESPACE mooncake::
    DESTINATION lib/cmake/rdma_traffic_monitor
)

# ============================================================================
# 生成 CMake 配置文件
# ============================================================================

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/rdma_traffic_monitor-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/rdma_traffic_monitor-config.cmake
    INSTALL_DESTINATION lib/cmake/rdma_traffic_monitor
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/rdma_traffic_monitor-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/rdma_traffic_monitor-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/rdma_traffic_monitor-config-version.cmake
    DESTINATION lib/cmake/rdma_traffic_monitor
)
```

## 编译步骤

### 方式 1: 集成到 Mooncake

```bash
cd /home/jiahuawang/test/Mooncake
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### 方式 2: 独立编译

```bash
# 创建构建目录
mkdir -p /tmp/rdma_monitor_build && cd /tmp/rdma_monitor_build

# 复制源文件
cp -r /home/jiahuawang/test/Mooncake/mooncake-transfer-engine/include/rdma_traffic_monitor.h .
cp -r /home/jiahuawang/test/Mooncake/mooncake-transfer-engine/src/rdma_traffic_monitor.cpp .

# 创建简单的 CMakeLists.txt
cat > CMakeLists.txt << 'EOF'
cmake_minimum_required(VERSION 3.18)
project(rdma_monitor)

set(CMAKE_CXX_STANDARD 17)
find_package(glog REQUIRED)

add_library(rdma_monitor STATIC rdma_traffic_monitor.cpp)
target_include_directories(rdma_monitor PUBLIC .)
target_link_libraries(rdma_monitor PUBLIC glog::glog rt pthread)
EOF

# 编译
cmake . -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## 链接到其他项目

### 使用 find_package

```cmake
find_package(rdma_traffic_monitor REQUIRED)

target_link_libraries(your_target
    PRIVATE
    mooncake::rdma_traffic_monitor
)
```

### 直接链接

```cmake
target_link_libraries(your_target
    PRIVATE
    /path/to/librdma_traffic_monitor.a
    glog::glog
    rt
    pthread
)
```

## 依赖说明

| 依赖 | 用途 | 必需 |
|------|------|------|
| glog | 日志记录 | 是 |
| rt | POSIX 共享内存 (shm_open, shm_unlink) | 是 |
| pthread | 线程和信号量 (sem_open, std::thread) | 是 |
| C++17 | std::atomic, std::unique_ptr 等 | 是 |

## 系统要求

- Linux 系统（使用 POSIX 共享内存和信号量）
- 支持 RDMA 的网卡（libibverbs）
- CMake >= 3.18
- GCC >= 7.0 或 Clang >= 6.0

## 编译选项

```bash
# Debug 构建（包含调试符号）
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Release 构建（优化）
cmake .. -DCMAKE_BUILD_TYPE=Release

# 启用示例
cmake .. -DBUILD_EXAMPLES=ON

# 启用测试
cmake .. -DBUILD_TESTS=ON

# 指定安装路径
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/mooncake
```

## 验证编译

```bash
# 检查库是否生成
ls -lh build/librdma_traffic_monitor.a

# 检查符号
nm build/librdma_traffic_monitor.a | grep -i "rdmatraffic"

# 运行示例（如果编译了）
./build/rdma_monitor_example 1
```
