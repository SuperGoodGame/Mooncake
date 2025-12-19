file(GLOB RDMA_SOURCES "*.cpp")

# Monitor library path
set(MONITOR_LIB_DIR "/home/jiahuawang/test/myfile/monitor")
set(MONITOR_INCLUDE_DIR "/home/jiahuawang/test/myfile/monitor")

add_library(rdma_transport OBJECT ${RDMA_SOURCES})
target_include_directories(rdma_transport PRIVATE ${MONITOR_INCLUDE_DIR})
target_link_libraries(rdma_transport PRIVATE JsonCpp::JsonCpp glog::glog pthread)
target_link_directories(rdma_transport PRIVATE ${MONITOR_LIB_DIR})
target_link_libraries(rdma_transport PRIVATE rdma_traffic_monitor rt)
