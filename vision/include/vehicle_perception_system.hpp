/**
 * @file vehicle_perception_system.hpp
 * @brief 车辆感知系统主类定义
 * @author pengchengkang
 * @date 2025-9-6
 * 
 * 本文件定义了车辆感知系统的核心类VehiclePerceptionSystem，该系统是一个集成了
 * 视频处理、目标检测、目标跟踪、行为分析和结果处理等功能的综合性感知系统。
 * 
 * 主要功能：
 * 1. 视频流处理：支持多种视频源输入，包括摄像头、视频文件等
 * 2. 目标检测：实时检测车辆、行人、非机动车等目标
 * 3. 目标跟踪：对检测到的目标进行持续跟踪，维护目标轨迹
 * 4. 行为分析：分析目标行为模式，评估风险等级
 * 5. 结果处理：可视化分析结果，支持多种输出格式
 * 6. 性能监控：实时监控系统运行状态和性能指标
 * 
 * 系统特点：
 * - 模块化设计，各功能模块松耦合
 * - 支持多线程处理，提高系统效率
 * - 提供完整的生命周期管理
 * - 支持动态配置更新
 * - 提供丰富的回调接口
 * - 完善的性能统计和监控
 */
#ifndef VEHICLE_PERCEPTION_SYSTEM_HPP
#define VEHICLE_PERCEPTION_SYSTEM_HPP

#include <memory>
#include <vector>
#include <functional>
#include <string>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include "config.hpp"
#include "data_structs.hpp"
#include "module_interface.hpp"
#include "logger.hpp"
#include "thread_pool.hpp"

// 系统状态枚举
enum class SystemState {
    STOPPED,       // 已停止
    INITIALIZING,  // 初始化中
    RUNNING,       // 运行中
    PAUSED,        // 已暂停
    ERROR          // 错误状态
};

// 系统性能统计
struct SystemPerformance {
    float fps;                       // 帧率
    float detection_time_ms;         // 检测时间(毫秒)
    float tracking_time_ms;          // 跟踪时间(毫秒)
    float analysis_time_ms;          // 分析时间(毫秒)
    float total_latency_ms;          // 总延迟(毫秒)
    float cpu_usage;                 // CPU使用率(%)
    float gpu_usage;                 // GPU使用率(%)
    size_t memory_usage_mb;          // 内存使用(MB)
};

// 系统主类
class VehiclePerceptionSystem {
public:
    // 构造函数与析构函数
    VehiclePerceptionSystem();
    ~VehiclePerceptionSystem();
    
    // 禁止拷贝和移动
    VehiclePerceptionSystem(const VehiclePerceptionSystem&) = delete;
    VehiclePerceptionSystem& operator=(const VehiclePerceptionSystem&) = delete;
    VehiclePerceptionSystem(VehiclePerceptionSystem&&) = delete;
    VehiclePerceptionSystem& operator=(VehiclePerceptionSystem&&) = delete;
    
    // 初始化系统
    bool initialize(const SystemConfig& config);
    
    // 启动系统
    bool start();
    
    // 停止系统
    void stop();
    
    // 暂停系统
    void pause();
    
    // 恢复系统运行
    void resume();
    
    // 获取系统状态
    SystemState getState() const;
    
    // 获取系统配置
    const SystemConfig& getConfig() const;
    
    // 更新系统配置
    bool updateConfig(const SystemConfig& config);
    
    // 获取最后一帧的分析结果
    std::vector<BehaviorAnalysis> getLastResults() const;
    
    // 注册结果回调函数
    void registerResultCallback(
        std::function<void(const std::vector<BehaviorAnalysis>&)> callback);
    
    // 注册状态变化回调函数
    void registerStateCallback(
        std::function<void(SystemState)> callback);
    
    // 获取系统性能统计
    SystemPerformance getPerformanceStats() const;
    
    // 重置系统
    bool reset();
    
private:
    // 系统配置
    SystemConfig config_;
    
    // 系统状态
    std::atomic<SystemState> state_;
    
    // 核心模块
    std::unique_ptr<IVideoProcessor> video_processor_;
    std::unique_ptr<IObjectDetector> object_detector_;
    std::unique_ptr<IObjectTracker> object_tracker_;
    std::unique_ptr<IBehaviorAnalyzer> behavior_analyzer_;
    std::unique_ptr<IResultProcessor> result_processor_;
    std::unique_ptr<ILLMEnhancer> llm_enhancer_;
    
    // 线程池
    std::unique_ptr<ThreadPool> thread_pool_;
    
    // 回调函数
    std::function<void(const std::vector<BehaviorAnalysis>&)> result_callback_;
    std::function<void(SystemState)> state_callback_;
    mutable std::mutex callback_mutex_;
    
    // 最后结果缓存
    std::vector<BehaviorAnalysis> last_results_;
    mutable std::mutex results_mutex_;
    
    // 性能统计
    SystemPerformance performance_stats_;
    mutable std::mutex performance_mutex_;
    
    // 控制标志
    std::atomic<bool> running_;
    std::atomic<bool> paused_;
    std::condition_variable pause_cv_;
    std::mutex pause_mutex_;
    
    // 处理帧的函数
    void processFrame(const cv::Mat& frame, uint64_t timestamp);
    
    // 更新系统状态
    void setState(SystemState new_state);
    
    // 初始化模块
    bool initializeModules();
    
    // 重置性能统计
    void resetPerformanceStats();
    
    // 更新性能统计
    void updatePerformanceStats(float detection_ms, float tracking_ms, 
                               float analysis_ms, float total_ms);
};

#endif // VEHICLE_PERCEPTION_SYSTEM_HPP
