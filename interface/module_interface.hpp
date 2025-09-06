#ifndef MODULE_INTERFACES_HPP
#define MODULE_INTERFACES_HPP

#include <vector>
#include <memory>
#include <functional>
#include <opencv2/opencv.hpp>
#include "data_structures.h"

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
    
    // 初始化视频处理器
    virtual bool initialize(const VideoSourceConfig& config, 
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
    
    // 创建实例
    static std::unique_ptr<IVideoProcessor> create();
};

// 目标检测器接口
class IObjectDetector {
public:
    virtual ~IObjectDetector() = default;
    
    // 初始化检测器
    virtual bool initialize(const DetectorConfig& config) = 0;
    
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
    virtual bool initialize(const TrackerConfig& config) = 0;
    
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
    
    // 初始化分析器
    virtual bool initialize(const BehaviorConfig& config,
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
    virtual bool initialize(const OutputConfig& config) = 0;
    
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
    virtual bool initialize(const LLMConfig& config) = 0;
    
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
