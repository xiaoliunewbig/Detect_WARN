/**
 * time: 2025-09-06
 * file-name: main.cpp
 * name: pengchengkang
 */

#include "vehicle_perception_system.h"
#include "config.h"
#include <iostream>
#include <signal.h>
#include <filesystem>

// 全局系统实例指针
std::unique_ptr<VehiclePerceptionSystem> system_instance;

// 信号处理函数
void signalHandler(int signum) {
    std::cout << "\nReceived signal " << signum << ". Stopping system..." << std::endl;
    
    if (system_instance) {
        system_instance->stop();
    }
    
    exit(signum);
}

// 打印系统状态
void printSystemState(SystemState state) {
    std::string state_str;
    switch (state) {
        case SystemState::STOPPED: state_str = "STOPPED"; break;
        case SystemState::INITIALIZING: state_str = "INITIALIZING"; break;
        case SystemState::RUNNING: state_str = "RUNNING"; break;
        case SystemState::PAUSED: state_str = "PAUSED"; break;
        case SystemState::ERROR: state_str = "ERROR"; break;
        default: state_str = "UNKNOWN";
    }
    std::cout << "System state: " << state_str << std::endl;
}

// 打印性能统计
void printPerformanceStats(const SystemPerformance& stats) {
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Performance stats - ";
    std::cout << "FPS: " << stats.fps << ", ";
    std::cout << "Latency: " << stats.total_latency_ms << "ms, ";
    std::cout << "Detection: " << stats.detection_time_ms << "ms, ";
    std::cout << "Tracking: " << stats.tracking_time_ms << "ms, ";
    std::cout << "Analysis: " << stats.analysis_time_ms << "ms" << std::endl;
}

int main(int argc, char* argv[]) {
    // 注册信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    std::cout << "Vehicle Perception System starting..." << std::endl;
    
    // 解析命令行参数
    std::string config_path = "configs/default.json";
    if (argc > 1) {
        config_path = argv[1];
    }
    
    // 检查配置文件是否存在
    if (!std::filesystem::exists(config_path)) {
        std::cerr << "Config file not found: " << config_path << std::endl;
        return 1;
    }
    
    // 加载配置
    SystemConfig config;
    if (!config.loadFromFile(config_path)) {
        std::cerr << "Failed to load config file: " << config_path << std::endl;
        return 1;
    }
    
    // 创建并初始化系统
    system_instance = std::make_unique<VehiclePerceptionSystem>();
    
    // 注册状态回调
    system_instance->registerStateCallback(printSystemState);
    
    // 初始化系统
    if (!system_instance->initialize(config)) {
        std::cerr << "Failed to initialize system" << std::endl;
        return 1;
    }
    
    // 启动系统
    if (!system_instance->start()) {
        std::cerr << "Failed to start system" << std::endl;
        return 1;
    }
    
    // 主循环，定期打印性能统计
    while (system_instance->getState() != SystemState::STOPPED && 
           system_instance->getState() != SystemState::ERROR) {
        printPerformanceStats(system_instance->getPerformanceStats());
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    // 停止系统
    system_instance->stop();
    std::cout << "System exited normally" << std::endl;
    
    return 0;
}
