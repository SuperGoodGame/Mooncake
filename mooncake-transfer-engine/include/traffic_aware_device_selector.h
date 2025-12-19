// Copyright 2024 KVCache.AI
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef TRAFFIC_AWARE_DEVICE_SELECTOR_H
#define TRAFFIC_AWARE_DEVICE_SELECTOR_H

#include <memory>
#include <string>
#include <vector>
#include "rdma_traffic_monitor.h"
#include "topology.h"

namespace mooncake {

/**
 * @brief Traffic-aware device selector
 * 
 * This class combines Topology (static device mapping) with RdmaTrafficMonitor
 * (dynamic traffic statistics) to select the best RDMA device with minimum traffic.
 * 
 * Usage:
 *   Topology topo;
 *   topo.discover();
 *   TrafficAwareDeviceSelector selector(topo);
 *   int device = selector.selectDevice("cuda:0");
 */
class TrafficAwareDeviceSelector {
public:
    /**
     * @brief Constructor
     * @param topology The topology object describing device relationships
     */
    explicit TrafficAwareDeviceSelector(const Topology& topology);
    
    ~TrafficAwareDeviceSelector() = default;
    
    /**
     * @brief Select best device for a storage type
     * @param storage_type Storage type (e.g., "cuda:0", "cpu:1")
     * @param retry_count Retry count for fallback selection
     * @return Device ID, or negative on error
     * 
     * Selection strategy:
     * 1. Get candidate devices from topology (preferred_hca first)
     * 2. If traffic monitor available, select device with minimum traffic
     * 3. Otherwise, fallback to random/round-robin selection
     */
    int selectDevice(const std::string& storage_type, int retry_count = 0);
    
    /**
     * @brief Select best device with hint
     * @param storage_type Storage type
     * @param hint Device name hint (e.g., "mlx5_0")
     * @param retry_count Retry count
     * @return Device ID
     */
    int selectDevice(const std::string& storage_type, 
                    const std::string& hint, 
                    int retry_count = 0);
    
    /**
     * @brief Check if traffic monitor is enabled and running
     */
    bool isTrafficMonitorAvailable() const;
    
    /**
     * @brief Get traffic statistics for a device
     * @param device_id Device ID
     * @return Traffic rate in bytes/s, or 0 if monitor not available
     */
    uint64_t getDeviceTrafficRate(int device_id) const;
    
    /**
     * @brief Get the underlying topology
     */
    const Topology& getTopology() const { return topology_; }

private:
    /**
     * @brief Select device with traffic awareness
     * @param preferred_devices Preferred device list
     * @param avail_devices Available device list
     * @param retry_count Retry count
     * @return Selected device ID
     */
    int selectDeviceWithTraffic(const std::vector<int>& preferred_devices,
                               const std::vector<int>& avail_devices,
                               int retry_count);
    
    /**
     * @brief Fallback selection without traffic monitor
     */
    int selectDeviceFallback(const std::vector<int>& preferred_devices,
                            const std::vector<int>& avail_devices,
                            int retry_count);

private:
    Topology topology_;  // Device topology (copyable, no unique_ptr)
    std::unique_ptr<RdmaTrafficMonitorClient> traffic_monitor_client_;
    bool monitor_available_;
};

}  // namespace mooncake

#endif  // TRAFFIC_AWARE_DEVICE_SELECTOR_H
