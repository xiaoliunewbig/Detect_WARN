/**
 * @file module_interface.hpp
 * @brief 定义智能驾驶系统中各个核心模块的接口
 * @author pengchengkang
 * @date 2025-9-6
 * 
 * 本文件定义了智能驾驶系统的核心模块接口，包括：
 * 1. IVideoProcessor: 视频处理接口，负责视频流的读取、预处理和帧回调
 *    - 支持视频文件和实时流处理
 *    - 提供ROI设置和畸变矫正功能
 *    - 支持暂停/恢复等控制操作
 * 
 * 2. IObjectDetector: 目标检测接口，负责图像中的目标检测
 *    - 支持单帧和批量检测
 *    - 可配置置信度和NMS阈值
 *    - 提供性能统计功能
 * 
 * 3. IObjectTracker: 目标跟踪接口，负责多目标跟踪
 *    - 基于检测结果更新目标轨迹
 *    - 支持跟踪参数配置
 *    - 维护目标生命周期
 * 
 * 4. IBehaviorAnalyzer: 行为分析接口，负责目标行为识别和风险评估
 *    - 分析目标运动模式
 *    - 评估风险等级
 *    - 预测潜在碰撞
 * 
 * 5. IResultProcessor: 结果处理接口，负责分析结果的处理和输出
 *    - 可视化分析结果
 *    - 支持结果保存
 *    - 生成处理后的视频流
 * 
 * 6. ILLMEnhancer: 大语言模型增强接口，提供智能分析增强
 *    - 增强行为分析结果
 *    - 提供更智能的风险评估
 *    - 生成自然语言描述
 * 
 * 所有接口均采用工厂模式创建实例，支持模块化设计和松耦合架构
 */

#ifndef MODULE_INTERFACES_HPP
#define MODULE_INTERFACES_HPP

#include <vector>
#include <memory>
#include <functional>
#include <opencv2/opencv.hpp>
#include "data_structs.hpp"
#include "config.hpp"

// 视频处理器接口
class IVideoProcessor {
public:
    enum class ProcessingState {
        IDLE,          // 空闲
        PROCESSING,    // 处理中
        PAUSED,        // 已暂停
        ERROR          // 错误
    };
    
    struct VideoProperties {
        int width;        // 宽度
        int height;       // 高度
        float fps;        // 帧率
        std::string codec;// 编码格式
        bool is_stream;   // 是否为流
    };
    
    virtual ~IVideoProcessor() = default;
    
    // 静态工厂函数
    static std::unique_ptr<IVideoProcessor> create();
    
    // 初始化视频处理器
    virtual bool initialize(const SystemConfig::VideoSourceConfig& config, 
                          const CameraParams& camera_params) = 0;
    
    // 开始处理
    virtual bool start() = 0;
    
    // 停止处理
    virtual void stop() = 0;
    
    // 暂停处理
    virtual void pause() = 0;
    
    // 恢复处理
    virtual void resume() = 0;
    
    // 获取当前处理状态
    virtual ProcessingState getState() const = 0;
    
    // 获取视频属性
    virtual VideoProperties getVideoProperties() const = 0;
    
    // 跳转到指定时间点(秒)
    virtual bool seek(double timestamp) = 0;
    
    // 获取当前时间点(秒)
    virtual double getCurrentTimestamp() const = 0;
    
    // 注册帧处理回调
    virtual void registerFrameCallback(
        std::function<void(const cv::Mat&, uint64_t)> callback) = 0;
    
    // 设置ROI区域
    virtual void setROI(const cv::Rect& roi) = 0;
    
    // 获取当前ROI区域
    virtual cv::Rect getROI() const = 0;
    
    // 启用/禁用畸变矫正
    virtual void setDistortionCorrection(bool enable) = 0;
};

// 目标检测器接口
class IObjectDetector {
public:
    virtual ~IObjectDetector() = default;
    
    // 初始化检测器
    virtual bool initialize(const SystemConfig::DetectorConfig& config) = 0;
    
    // 检测目标
    virtual std::vector<Detection> detect(const cv::Mat& image) = 0;
    
    // 批量检测目标
    virtual std::vector<std::vector<Detection>> detectBatch(const std::vector<cv::Mat>& images) = 0;
    
    // 获取类别名称列表
    virtual const std::vector<std::string>& getClassNames() const = 0;
    
    // 设置置信度阈值
    virtual void setConfidenceThreshold(float threshold) = 0;
    
    // 设置NMS阈值
    virtual void setNmsThreshold(float threshold) = 0;
    
    // 获取性能统计信息
    virtual const DetectionPerformance& getPerformanceStats() const = 0;
    
    // 创建实例
    static std::unique_ptr<IObjectDetector> create();
};

// 目标跟踪器接口
class IObjectTracker {
public:
    virtual ~IObjectTracker() = default;
    
    // 初始化跟踪器
    virtual bool initialize(const SystemConfig::TrackerConfig& config) = 0;
    
    // 更新跟踪
    virtual std::vector<TrackedObject> update(
        const std::vector<Detection>& detections, uint64_t timestamp) = 0;
    
    // 获取所有跟踪目标
    virtual std::vector<TrackedObject> getTracks() const = 0;
    
    // 重置跟踪器
    virtual void reset() = 0;
    
    // 设置最大跟踪年龄
    virtual void setMaxAge(int max_age) = 0;
    
    // 设置最小检测数
    virtual void setMinHits(int min_hits) = 0;
    
    // 创建实例
    static std::unique_ptr<IObjectTracker> create();
};

// 行为分析器接口
class IBehaviorAnalyzer {
public:
    virtual ~IBehaviorAnalyzer() = default;
    
    // 初始化行为分析器
    virtual bool initialize(const SystemConfig::BehaviorConfig& config,
                          const CameraParams& camera_params,
                          const VehicleParams& vehicle_params) = 0;
    
    // 分析目标行为
    virtual std::vector<BehaviorAnalysis> analyze(
        const std::vector<TrackedObject>& tracked_objects) = 0;
    
    // 设置车辆当前速度(km/h)
    virtual void setVehicleSpeed(float speed_kmh) = 0;
    
    // 获取车辆当前速度
    virtual float getVehicleSpeed() const = 0;
    
    // 创建实例
    static std::unique_ptr<IBehaviorAnalyzer> create();
};

// 结果处理器接口
class IResultProcessor {
public:
    virtual ~IResultProcessor() = default;
    
    // 初始化结果处理器
    virtual bool initialize(const SystemConfig::OutputConfig& config) = 0;
    
    // 处理分析结果
    virtual void process(const std::vector<BehaviorAnalysis>& results,
                       const cv::Mat& frame, uint64_t timestamp) = 0;
    
    // 获取处理后的帧(带叠加)
    virtual cv::Mat getProcessedFrame() const = 0;
    
    // 保存结果到文件
    virtual bool saveResults(const std::string& path) const = 0;
    
    // 创建实例
    static std::unique_ptr<IResultProcessor> create();
};

// LLM增强器接口
class ILLMEnhancer {
public:
    virtual ~ILLMEnhancer() = default;
    
    // 初始化LLM增强器
    virtual bool initialize(const SystemConfig::LLMConfig& config) = 0;
    
    // 增强行为分析结果
    virtual std::vector<BehaviorAnalysis> enhanceAnalysis(
        const std::vector<BehaviorAnalysis>& basic_analysis,
        const std::vector<TrackedObject>& tracked_objects) = 0;
    
    // 设置车辆当前速度(km/h)
    virtual void setVehicleSpeed(float speed_kmh) = 0;
    
    // 创建实例
    static std::unique_ptr<ILLMEnhancer> create();
};

#endif // MODULE_INTERFACES_HPP
