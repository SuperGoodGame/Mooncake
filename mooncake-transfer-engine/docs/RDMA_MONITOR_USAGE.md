# RDMA Traffic Monitor - 使用文档

## 概述

RDMA Traffic Monitor 是一个用于监控 RDMA 网卡流量并实现负载感知设备选择的系统。它通过共享内存实现进程间通信，支持多进程访问流量统计信息。

## 架构设计

### 核心组件

1. **RdmaTrafficMonitor** - 监控器主进程
   - 维护共享内存中的流量统计
   - 定期更新流量速率（每100ms）
   - 记录每个设备的 TX/RX 字节数和速率

2. **RdmaTrafficMonitorClient** - 客户端接口
   - 查询实时流量统计
   - 查找最小流量设备（用于负载均衡）
   - 无锁读取（使用 atomic 操作）

3. **Topology 集成** - 拓扑感知选择
   - 自动连接到流量监控器
   - 优先选择 preferred_hca 中流量最小的设备
   - 如果 preferred_hca 不可用，则从 avail_hca 中选择

### 流量选择策略

```
选择优先级：
1. preferred_hca 中流量最小的设备（优先）
2. 如果 retry_count > 0，则考虑 avail_hca 中流量最小的设备
3. 回退到原有的随机/轮询策略
```

## 使用方法

### 1. 启动监控器进程

监控器应该作为独立进程运行，在 transfer engine 启动之前或同时启动。

```cpp
#include "rdma_traffic_monitor.h"

using namespace mooncake;

int main() {
    RdmaTrafficMonitor monitor;
    
    // 指定要监控的 RDMA 设备名称
    std::vector<std::string> devices = {"mlx5_0", "mlx5_1", "mlx5_2", "mlx5_3"};
    
    if (!monitor.initialize(devices)) {
        LOG(ERROR) << "Failed to initialize monitor";
        return 1;
    }
    
    monitor.start();
    
    // 保持运行
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return 0;
}
```

### 2. Transfer Engine 中记录流量

在发送/接收数据时，调用监控器记录流量：

```cpp
// 假设在 transfer engine 的发送函数中
void sendData(int device_id, const void* data, size_t size) {
    // ... 实际发送逻辑 ...
    
    // 记录流量
    if (traffic_monitor_) {
        traffic_monitor_->recordTransmit(device_id, size);
    }
}

void receiveData(int device_id, void* buffer, size_t size) {
    // ... 实际接收逻辑 ...
    
    // 记录流量
    if (traffic_monitor_) {
        traffic_monitor_->recordReceive(device_id, size);
    }
}
```

### 3. 在 Topology 中启用流量感知选择

```cpp
#include "topology.h"

using namespace mooncake;

int main() {
    Topology topology;
    
    // 发现设备
    topology.discover();
    
    // 启用流量监控（如果监控器正在运行，会自动连接）
    topology.enableTrafficMonitor(true);
    
    // 选择设备 - 会自动使用流量感知策略
    std::string storage_type = "cuda:0";
    int device_id = topology.selectDevice(storage_type);
    
    LOG(INFO) << "Selected device: " << device_id;
    
    return 0;
}
```

### 4. 独立查询流量统计

```cpp
#include "rdma_traffic_monitor.h"

using namespace mooncake;

void printTrafficStats() {
    RdmaTrafficMonitorClient client;
    
    if (!client.isMonitorRunning()) {
        LOG(WARNING) << "Monitor is not running";
        return;
    }
    
    int num_devices = client.getDeviceCount();
    
    for (int i = 0; i < num_devices; ++i) {
        uint64_t tx_rate = client.getTxBytesRate(i);
        uint64_t rx_rate = client.getRxBytesRate(i);
        
        std::cout << "Device " << i << ": "
                  << "TX=" << (tx_rate / 1024.0 / 1024.0) << " MB/s, "
                  << "RX=" << (rx_rate / 1024.0 / 1024.0) << " MB/s"
                  << std::endl;
    }
    
    // 查找流量最小的设备
    std::vector<int> candidates = {0, 1, 2, 3};
    int best_device = client.findMinTrafficDevice(candidates);
    std::cout << "Device with minimum traffic: " << best_device << std::endl;
}
```

## 编译配置

### CMakeLists.txt 修改

```cmake
# 添加监控器源文件
add_library(mooncake_rdma_monitor
    src/rdma_traffic_monitor.cpp
)

target_link_libraries(mooncake_rdma_monitor
    PRIVATE
    rt          # 共享内存支持
    pthread     # 线程支持
    glog
)

# 链接到 transfer engine
target_link_libraries(mooncake-transfer-engine
    PRIVATE
    mooncake_rdma_monitor
    # ... 其他依赖 ...
)

# 编译示例
add_executable(rdma_monitor_example
    examples/rdma_monitor_example.cpp
)

target_link_libraries(rdma_monitor_example
    PRIVATE
    mooncake_rdma_monitor
    mooncake-transfer-engine
    glog
)
```

## 部署建议

### 单机多 GPU 场景

```bash
# Terminal 1: 启动监控器
./rdma_monitor_example 1

# Terminal 2: 运行 transfer engine
./your_transfer_engine_app
```

### 分布式场景

在每台机器上：

```bash
# 1. 启动监控器（每台机器独立运行）
./rdma_monitor_example 1 &

# 2. 启动 transfer engine
./your_transfer_engine_app
```

## 配置参数

### 监控器配置

- `MAX_RDMA_DEVICES`: 最大支持的 RDMA 设备数量（默认 16）
- `MONITOR_INTERVAL_MS`: 监控更新间隔（默认 100ms）
- `SHM_RDMA_MONITOR_NAME`: 共享内存名称
- `SEM_RDMA_MONITOR_NAME`: 信号量名称

修改这些参数需要在 `rdma_traffic_monitor.h` 中修改宏定义。

### Topology 配置

```cpp
// 在代码中动态启用/禁用
topology.enableTrafficMonitor(true);   // 启用
topology.enableTrafficMonitor(false);  // 禁用

// 检查状态
if (topology.isTrafficMonitorEnabled()) {
    LOG(INFO) << "Traffic-aware selection enabled";
}
```

## 性能考虑

### 共享内存访问

- 使用 `std::atomic` 实现无锁读取
- 信号量仅在写入和 `findMinTrafficDevice` 时使用
- 监控器更新间隔 100ms，对性能影响极小

### 流量统计开销

```cpp
// recordTransmit/recordReceive 的开销
// - 1 次 atomic fetch_add（TX bytes）
// - 1 次 atomic fetch_add（TX packets）  
// - 1 次 atomic store（timestamp）
// 总开销约 < 100ns
```

### 设备选择开销

```cpp
// selectDevice 的额外开销（启用流量监控时）
// - 遍历 preferred_hca 列表
// - 每个设备 1 次 atomic load（TX rate）
// - 每个设备 1 次 atomic load（RX rate）
// 4 个设备约 < 1us
```

## 故障处理

### 监控器未运行

如果监控器未运行，Topology 会自动回退到原有的随机/轮询策略：

```cpp
topology.enableTrafficMonitor(true);

if (!topology.isTrafficMonitorEnabled()) {
    LOG(WARNING) << "Traffic monitor not available, using default selection";
    // 仍然可以正常工作，只是不使用流量感知策略
}
```

### 共享内存清理

如果程序异常退出，共享内存可能需要手动清理：

```bash
# 列出共享内存
ls /dev/shm/

# 清理监控器的共享内存
rm /dev/shm/mooncake_rdma_monitor_shm

# 清理信号量
rm /dev/shm/sem.mooncake_rdma_monitor_sem
```

## 调试技巧

### 1. 查看流量统计

```cpp
// 添加日志查看选择的设备和流量
int device = topology.selectDevice(storage_type);
LOG(INFO) << "Selected device " << device 
          << " with traffic rate: " 
          << client.getTotalBytesRate(device) / (1024.0 * 1024.0) 
          << " MB/s";
```

### 2. 验证监控器运行状态

```bash
# 检查共享内存是否存在
ls -lh /dev/shm/ | grep mooncake_rdma_monitor

# 检查进程
ps aux | grep rdma_monitor
```

### 3. 性能分析

```cpp
// 添加计时器测量选择开销
auto start = std::chrono::high_resolution_clock::now();
int device = topology.selectDevice(storage_type);
auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
LOG(INFO) << "Device selection took " << duration.count() << " us";
```

## 示例场景

### 场景 1: LMCache KV 存储

```python
# 在 Python 中通过 C++ 扩展使用

# 1. 启动监控器
# 2. LMCache engine 自动启用流量监控
# 3. 每次 put/get 操作时，自动选择流量最小的 RDMA 网卡
```

### 场景 2: 多租户负载均衡

```cpp
// Tenant A 在 cuda:0 上运行
// Tenant B 在 cuda:1 上运行
// 每个 tenant 的流量会自动分配到各自最优的 RDMA 网卡

Topology topology_a, topology_b;
topology_a.discover();
topology_b.discover();

topology_a.enableTrafficMonitor(true);
topology_b.enableTrafficMonitor(true);

// Tenant A 的请求
int device_a = topology_a.selectDevice("cuda:0");

// Tenant B 的请求
int device_b = topology_b.selectDevice("cuda:1");

// 如果 cuda:0 和 cuda:1 对应的 preferred_hca 不同
// 则会自动选择各自负载最小的网卡
```

## 与 FuseLink 的对比

| 特性 | FuseLink Monitor | Mooncake RDMA Monitor |
|------|------------------|----------------------|
| 监控粒度 | Channel 级别 | Device 级别 |
| 状态类型 | IDLE/BUSY/BORROWED | 流量速率统计 |
| 更新方式 | 超时检测 | 实时累加 + 周期性速率计算 |
| 选择策略 | 状态检查 | 流量最小优先 |
| 应用场景 | 通道调度 | 负载均衡 |

## 总结

RDMA Traffic Monitor 提供了：

✅ **流量感知的设备选择** - 自动选择负载最低的 RDMA 网卡  
✅ **无侵入式集成** - 兼容现有的 Topology 接口  
✅ **低开销设计** - 使用 atomic 操作和共享内存，性能影响 < 1us  
✅ **自动降级** - 监控器不可用时自动回退到默认策略  
✅ **灵活配置** - 支持动态启用/禁用

通过这个监控系统，Mooncake transfer engine 可以在多 RDMA 网卡环境中实现更好的负载均衡和更高的吞吐量。
