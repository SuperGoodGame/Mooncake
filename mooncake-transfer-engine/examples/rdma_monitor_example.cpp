// Copyright 2024 KVCache.AI
//
// RDMA Traffic Monitor Example
// 
// This example demonstrates how to use the RDMA traffic monitor
// for traffic-aware device selection in Mooncake transfer engine.

#include <iostream>
#include <thread>
#include <chrono>
#include <glog/logging.h>
#include "rdma_traffic_monitor.h"
#include "topology.h"

using namespace mooncake;

// Example 1: Standalone monitor process
void runMonitorProcess() {
    // Create and initialize monitor
    RdmaTrafficMonitor monitor;
    
    // Specify the RDMA device names to monitor
    // These should match your system's RDMA device names (e.g., mlx5_0, mlx5_1)
    std::vector<std::string> device_names = {"mlx5_0", "mlx5_1", "mlx5_2", "mlx5_3"};
    
    if (!monitor.initialize(device_names)) {
        LOG(ERROR) << "Failed to initialize RDMA traffic monitor";
        return;
    }
    
    // Start monitoring
    monitor.start();
    LOG(INFO) << "RDMA traffic monitor started, monitoring " 
              << monitor.getDeviceCount() << " devices";
    
    // Simulate some traffic recording
    // In real scenario, this would be called by the transfer engine
    for (int i = 0; i < 100; ++i) {
        // Simulate different traffic on different devices
        monitor.recordTransmit(0, 1024 * 1024);      // 1MB on device 0
        monitor.recordTransmit(1, 512 * 1024);       // 512KB on device 1
        monitor.recordTransmit(2, 2 * 1024 * 1024);  // 2MB on device 2
        monitor.recordTransmit(3, 256 * 1024);       // 256KB on device 3
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Keep monitor running (in real scenario, this would run indefinitely)
    std::this_thread::sleep_for(std::chrono::seconds(60));
    
    monitor.stop();
    LOG(INFO) << "RDMA traffic monitor stopped";
}

// Example 2: Query traffic from client
void queryTrafficExample() {
    // Create monitor client
    RdmaTrafficMonitorClient client;
    
    if (!client.isMonitorRunning()) {
        LOG(WARNING) << "Monitor is not running. Please start the monitor first.";
        return;
    }
    
    int device_count = client.getDeviceCount();
    LOG(INFO) << "Connected to monitor, " << device_count << " devices available";
    
    // Query traffic for each device
    for (int i = 0; i < device_count; ++i) {
        uint64_t tx_rate = client.getTxBytesRate(i);
        uint64_t rx_rate = client.getRxBytesRate(i);
        uint64_t total_rate = client.getTotalBytesRate(i);
        
        LOG(INFO) << "Device " << i << ": "
                  << "TX=" << (tx_rate / 1024.0 / 1024.0) << " MB/s, "
                  << "RX=" << (rx_rate / 1024.0 / 1024.0) << " MB/s, "
                  << "Total=" << (total_rate / 1024.0 / 1024.0) << " MB/s";
    }
    
    // Find device with minimum traffic
    std::vector<int> candidates = {0, 1, 2, 3};
    int best_device = client.findMinTrafficDevice(candidates);
    
    if (best_device != -1) {
        LOG(INFO) << "Device with minimum traffic: " << best_device
                  << " (rate: " << (client.getTotalBytesRate(best_device) / 1024.0 / 1024.0) 
                  << " MB/s)";
    }
}

// Example 3: Integrate with Topology for traffic-aware device selection
void topologyIntegrationExample() {
    // Create topology
    Topology topology;
    
    // Discover devices or parse from JSON
    topology.discover();
    
    // Enable traffic monitoring
    topology.enableTrafficMonitor(true);
    
    if (topology.isTrafficMonitorEnabled()) {
        LOG(INFO) << "Traffic-aware device selection is enabled";
    } else {
        LOG(WARNING) << "Traffic monitor not available, using default selection";
    }
    
    // Select device - will automatically use traffic-aware selection
    // This will prefer devices from preferred_hca with lowest traffic
    std::string storage_type = "cuda:0";  // Example: GPU 0
    int selected_device = topology.selectDevice(storage_type);
    
    LOG(INFO) << "Selected device: " << selected_device 
              << " for storage type: " << storage_type;
    
    // For retry scenarios
    int retry_device = topology.selectDevice(storage_type, 1);
    LOG(INFO) << "Retry device: " << retry_device;
}

int main(int argc, char* argv[]) {
    google::InitGoogleLogging(argv[0]);
    FLAGS_logtostderr = 1;
    
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <mode>" << std::endl;
        std::cout << "Modes:" << std::endl;
        std::cout << "  1 - Run monitor process" << std::endl;
        std::cout << "  2 - Query traffic from client" << std::endl;
        std::cout << "  3 - Topology integration example" << std::endl;
        return 1;
    }
    
    int mode = std::stoi(argv[1]);
    
    switch (mode) {
        case 1:
            LOG(INFO) << "Running monitor process...";
            runMonitorProcess();
            break;
        case 2:
            LOG(INFO) << "Querying traffic...";
            queryTrafficExample();
            break;
        case 3:
            LOG(INFO) << "Running topology integration example...";
            topologyIntegrationExample();
            break;
        default:
            LOG(ERROR) << "Invalid mode: " << mode;
            return 1;
    }
    
    return 0;
}
