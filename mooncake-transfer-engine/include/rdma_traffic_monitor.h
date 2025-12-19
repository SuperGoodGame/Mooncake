#ifndef RDMA_TRAFFIC_MONITOR_H
#define RDMA_TRAFFIC_MONITOR_H

#include <atomic>
#include <thread>
#include <string>
#include <vector>
#include <cstdint>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>

#define MAX_RDMA_DEVICES 16
#define SHM_NAME "/rdma_traffic_monitor_shm"
#define SEM_NAME "/rdma_traffic_monitor_sem"

// Shared memory structure for IPC
struct RdmaTrafficSharedMemory {
    // 应用层流量计数（由应用写入，atomic 保证并发安全）
    std::atomic<uint64_t> nccl_tx_bytes[MAX_RDMA_DEVICES];
    std::atomic<uint64_t> nccl_rx_bytes[MAX_RDMA_DEVICES];
    std::atomic<uint64_t> mooncake_tx_bytes[MAX_RDMA_DEVICES];
    std::atomic<uint64_t> mooncake_rx_bytes[MAX_RDMA_DEVICES];
    
    // 硬件层流量（由 Monitor 从硬件读取）
    std::atomic<uint64_t> hw_tx_bytes[MAX_RDMA_DEVICES];
    std::atomic<uint64_t> hw_rx_bytes[MAX_RDMA_DEVICES];
    
    // 计算出的传输速率（bytes/sec）（由 Monitor 计算）
    std::atomic<uint64_t> nccl_tx_rate[MAX_RDMA_DEVICES];
    std::atomic<uint64_t> nccl_rx_rate[MAX_RDMA_DEVICES];
    std::atomic<uint64_t> mooncake_tx_rate[MAX_RDMA_DEVICES];
    std::atomic<uint64_t> mooncake_rx_rate[MAX_RDMA_DEVICES];
    std::atomic<uint64_t> hw_tx_rate[MAX_RDMA_DEVICES];
    std::atomic<uint64_t> hw_rx_rate[MAX_RDMA_DEVICES];
    
    // 设备信息
    char device_names[MAX_RDMA_DEVICES][32];
    std::atomic<int> num_devices;
    
    // 控制标志
    std::atomic<bool> running;
    std::atomic<uint64_t> last_update_timestamp_ms;
};

// RDMA 硬件计数器读取器
class RdmaHardwareCounter {
public:
    RdmaHardwareCounter(const std::string& device_name);
    ~RdmaHardwareCounter();
    
    bool read(uint64_t& tx_bytes, uint64_t& rx_bytes);
    const std::string& getDeviceName() const { return device_name_; }
    
private:
    std::string device_name_;
    int tx_fd_;
    int rx_fd_;
    bool valid_;
};

// Monitor 服务端（独立线程，定期读取硬件并计算速率）
class RdmaTrafficMonitor {
public:
    RdmaTrafficMonitor();
    ~RdmaTrafficMonitor();
    
    // 初始化（检测设备，创建共享内存）
    bool initialize(const std::vector<std::string>& device_names = {});
    
    // 启动监控线程
    bool start();
    
    // 停止监控
    void stop();
    
    // 获取设备数量
    int getDeviceCount() const;
    
    // 获取设备名称
    std::string getDeviceName(int device_id) const;
    
private:
    void monitorThread();
    bool setupSharedMemory();
    bool detectDevices(const std::vector<std::string>& device_names);
    uint64_t getTimestampMs();
    
    RdmaTrafficSharedMemory* shm_;
    int shm_fd_;
    sem_t* semaphore_;
    
    std::thread monitor_thread_;
    std::atomic<bool> running_;
    
    std::vector<RdmaHardwareCounter*> hw_counters_;
    
    // 用于计算速率的历史数据
    struct HistoryData {
        uint64_t nccl_tx_bytes;
        uint64_t nccl_rx_bytes;
        uint64_t mooncake_tx_bytes;
        uint64_t mooncake_rx_bytes;
        uint64_t hw_tx_bytes;
        uint64_t hw_rx_bytes;
        uint64_t timestamp_ms;
    };
    std::vector<HistoryData> last_snapshot_;
};

// Monitor Client（用于 Mooncake 等应用查询流量）
class RdmaTrafficMonitorClient {
public:
    RdmaTrafficMonitorClient();
    ~RdmaTrafficMonitorClient();
    
    // 连接到共享内存
    bool connect();
    
    // 获取设备数量
    int getDeviceCount() const;
    
    // 获取设备名称
    std::string getDeviceName(int device_id) const;
    
    // 查询流量速率（bytes/sec）
    uint64_t getNcclTxRate(int device_id) const;
    uint64_t getNcclRxRate(int device_id) const;
    uint64_t getMooncakeTxRate(int device_id) const;
    uint64_t getMooncakeRxRate(int device_id) const;
    uint64_t getHwTxRate(int device_id) const;
    uint64_t getHwRxRate(int device_id) const;
    
    // 查询累计流量（bytes）
    uint64_t getNcclTxBytes(int device_id) const;
    uint64_t getMooncakeTxBytes(int device_id) const;
    uint64_t getHwTxBytes(int device_id) const;
    
    // 记录应用层流量（由 NCCL/Mooncake 调用）
    void recordNcclTx(int device_id, uint64_t bytes);
    void recordNcclRx(int device_id, uint64_t bytes);
    void recordMooncakeTx(int device_id, uint64_t bytes);
    void recordMooncakeRx(int device_id, uint64_t bytes);
    
    // 选择最空闲的设备（根据流量速率）
    int selectLeastLoadedDevice(bool use_nccl_only = false) const;
    
    // 从给定设备列表中选择流量最小的设备（返回 device_id）
    int selectLeastLoadedDeviceFrom(const std::vector<std::string>& device_names) const;
    
    // 获取上次更新时间
    uint64_t getLastUpdateTimestamp() const;
    
    // 检查 monitor 是否在运行
    bool isMonitorRunning() const;
    
private:
    RdmaTrafficSharedMemory* shm_;
    int shm_fd_;
    sem_t* semaphore_;
};

#endif // RDMA_TRAFFIC_MONITOR_H
