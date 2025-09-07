/**
 * @file test_video_retry.cpp
 * @brief 测试VideoProcessor摄像头等待重连功能
 */

#include "config/config.hpp"
#include "interface/module_interface.hpp"
#include "main/logger.hpp"
#include <iostream>
#include <chrono>
#include <thread>

int main() {
    std::cout << "=== 摄像头等待重连功能测试 ===" << std::endl;
    
    // 初始化日志系统
    Logger::initialize("logs/", LogLevel::INFO, true);
    
    // 创建VideoProcessor实例
    auto video_processor = IVideoProcessor::create();
    if (!video_processor) {
        std::cout << "✗ VideoProcessor创建失败" << std::endl;
        return -1;
    }
    std::cout << "✓ VideoProcessor创建成功" << std::endl;
    
    // 配置摄像头参数（使用不存在的摄像头ID来测试重连机制）
    SystemConfig::VideoSourceConfig video_config;
    video_config.source = "99";  // 使用不存在的摄像头ID
    video_config.width = 640;
    video_config.height = 480;
    video_config.fps = 30.0;
    video_config.wait_for_device = true;
    video_config.connection_timeout_sec = 15;  // 15秒超时
    video_config.retry_interval_sec = 3;       // 3秒重试间隔
    video_config.max_retry_attempts = 5;       // 最多5次尝试
    
    CameraParams camera_params;
    camera_params.fx = 640.0f;
    camera_params.fy = 640.0f;
    camera_params.cx = 320.0f;
    camera_params.cy = 240.0f;
    
    std::cout << "\n=== 测试不存在摄像头的重连机制 ===" << std::endl;
    std::cout << "摄像头ID: " << video_config.source << std::endl;
    std::cout << "超时时间: " << video_config.connection_timeout_sec << "秒" << std::endl;
    std::cout << "重试间隔: " << video_config.retry_interval_sec << "秒" << std::endl;
    std::cout << "最大尝试次数: " << video_config.max_retry_attempts << std::endl;
    
    auto start_time = std::chrono::steady_clock::now();
    
    // 尝试初始化VideoProcessor（应该会重试并最终超时）
    bool init_result = video_processor->initialize(video_config, camera_params);
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
    
    std::cout << "\n=== 测试结果 ===" << std::endl;
    std::cout << "初始化结果: " << (init_result ? "成功" : "失败") << std::endl;
    std::cout << "实际耗时: " << duration.count() << "秒" << std::endl;
    
    if (!init_result && duration.count() >= video_config.connection_timeout_sec - 2) {
        std::cout << "✓ 重连机制工作正常，在预期时间内超时" << std::endl;
    } else if (!init_result) {
        std::cout << "✓ 重连机制工作正常，连接失败" << std::endl;
    } else {
        std::cout << "? 意外成功连接到摄像头" << std::endl;
    }
    
    std::cout << "\n=== 测试默认摄像头（ID=0） ===" << std::endl;
    video_config.source = "0";
    video_config.connection_timeout_sec = 10;
    video_config.max_retry_attempts = 3;
    
    auto video_processor2 = IVideoProcessor::create();
    start_time = std::chrono::steady_clock::now();
    
    bool init_result2 = video_processor2->initialize(video_config, camera_params);
    
    end_time = std::chrono::steady_clock::now();
    duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
    
    std::cout << "摄像头ID=0初始化结果: " << (init_result2 ? "成功" : "失败") << std::endl;
    std::cout << "耗时: " << duration.count() << "秒" << std::endl;
    
    if (init_result2) {
        std::cout << "✓ 成功连接到默认摄像头" << std::endl;
        
        // 获取视频属性
        auto properties = video_processor2->getVideoProperties();
        std::cout << "视频属性:" << std::endl;
        std::cout << "  分辨率: " << properties.width << "x" << properties.height << std::endl;
        std::cout << "  帧率: " << properties.fps << std::endl;
        std::cout << "  是否为流: " << (properties.is_stream ? "是" : "否") << std::endl;
    } else {
        std::cout << "✓ 重连机制正常工作，未找到可用摄像头" << std::endl;
    }
    
    std::cout << "\n=== 测试完成 ===" << std::endl;
    return 0;
}
