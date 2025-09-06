#include "vehicle_perception_system.hpp"
#include <chrono>
#include <thread>
#include <sstream>
#include <iomanip>

VehiclePerceptionSystem::VehiclePerceptionSystem()
    : state_(SystemState::STOPPED),
      running_(false),
      paused_(false) {
    // 初始化日志
    Logger::initialize();
}

VehiclePerceptionSystem::~VehiclePerceptionSystem() {
    stop();
}

bool VehiclePerceptionSystem::initialize(const SystemConfig& config) {
    setState(SystemState::INITIALIZING);
    
    try {
        // 保存配置
        config_ = config;
        
        // 初始化线程池
        thread_pool_ = std::make_unique<ThreadPool>(4); // 4个工作线程
        
        // 初始化各个模块
        if (!initializeModules()) {
            LOG_ERROR("Failed to initialize modules");
            setState(SystemState::ERROR);
            return false;
        }
        
        // 重置性能统计
        resetPerformanceStats();
        
        setState(SystemState::STOPPED);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Initialization error: {}", e.what());
        setState(SystemState::ERROR);
        return false;
    }
}

bool VehiclePerceptionSystem::initializeModules() {
    // 初始化视频处理器
    video_processor_ = IVideoProcessor::create();
    if (!video_processor_ || !video_processor_->initialize(config_.video, config_.camera)) {
        LOG_ERROR("Failed to initialize video processor");
        return false;
    }
    
    // 设置视频帧回调
    auto frame_callback = [this](const cv::Mat& frame, uint64_t timestamp) {
        if (state_ == SystemState::RUNNING && !paused_) {
            thread_pool_->submit(&VehiclePerceptionSystem::processFrame, this, frame, timestamp);
        }
    };
    video_processor_->registerFrameCallback(frame_callback);
    
    // 初始化目标检测器
    object_detector_ = IObjectDetector::create();
    if (!object_detector_ || !object_detector_->initialize(config_.detector)) {
        LOG_ERROR("Failed to initialize object detector");
        return false;
    }
    
    // 初始化目标跟踪器
    object_tracker_ = IObjectTracker::create();
    if (!object_tracker_ || !object_tracker_->initialize(config_.tracker)) {
        LOG_ERROR("Failed to initialize object tracker");
        return false;
    }
    
    // 初始化行为分析器
    behavior_analyzer_ = IBehaviorAnalyzer::create();
    if (!behavior_analyzer_ || !behavior_analyzer_->initialize(config_.behavior, config_.camera, config_.vehicle)) {
        LOG_ERROR("Failed to initialize behavior analyzer");
        return false;
    }
    
    // 初始化结果处理器
    result_processor_ = IResultProcessor::create();
    if (!result_processor_ || !result_processor_->initialize(config_.output)) {
        LOG_ERROR("Failed to initialize result processor");
        return false;
    }
    
    // 初始化LLM增强器(如果启用)
    if (config_.llm.enable) {
        llm_enhancer_ = ILLMEnhancer::create();
        if (!llm_enhancer_ || !llm_enhancer_->initialize(config_.llm)) {
            LOG_WARN("Failed to initialize LLM enhancer, proceeding without it");
            llm_enhancer_.reset();
        }
    }
    
    return true;
}

bool VehiclePerceptionSystem::start() {
    if (state_ != SystemState::STOPPED && state_ != SystemState::PAUSED) {
        LOG_WARN("System is not in a state that can be started");
        return false;
    }
    
    try {
        setState(SystemState::RUNNING);
        paused_ = false;
        
        // 启动视频处理器
        if (!video_processor_->start()) {
            LOG_ERROR("Failed to start video processor");
            setState(SystemState::ERROR);
            return false;
        }
        
        LOG_INFO("System started successfully");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to start system: {}", e.what());
        setState(SystemState::ERROR);
        return false;
    }
}

void VehiclePerceptionSystem::stop() {
    if (state_ == SystemState::STOPPED) {
        return;
    }
    
    setState(SystemState::STOPPED);
    running_ = false;
    paused_ = false;
    
    // 停止视频处理器
    if (video_processor_) {
        video_processor_->stop();
    }
    
    // 等待所有任务完成
    if (thread_pool_) {
        thread_pool_->stop();
        thread_pool_->start(); // 重新启动以清理线程
    }
    
    LOG_INFO("System stopped");
}

void VehiclePerceptionSystem::pause() {
    if (state_ != SystemState::RUNNING) {
        return;
    }
    
    paused_ = true;
    setState(SystemState::PAUSED);
    LOG_INFO("System paused");
}

void VehiclePerceptionSystem::resume() {
    if (state_ != SystemState::PAUSED) {
        return;
    }
    
    paused_ = false;
    setState(SystemState::RUNNING);
    pause_cv_.notify_all();
    LOG_INFO("System resumed");
}

SystemState VehiclePerceptionSystem::getState() const {
    return state_;
}

const SystemConfig& VehiclePerceptionSystem::getConfig() const {
    return config_;
}

bool VehiclePerceptionSystem::updateConfig(const SystemConfig& config) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    
    // 保存旧状态
    SystemState old_state = state_;
    
    // 如果系统正在运行，先暂停
    bool was_running = (old_state == SystemState::RUNNING);
    if (was_running) {
        pause();
    }
    
    // 更新配置
    config_ = config;
    
    // 重新初始化模块
    bool success = initializeModules();
    
    // 恢复之前的状态
    if (was_running && success) {
        resume();
    }
    
    return success;
}

std::vector<BehaviorAnalysis> VehiclePerceptionSystem::getLastResults() const {
    std::lock_guard<std::mutex> lock(results_mutex_);
    return last_results_;
}

void VehiclePerceptionSystem::registerResultCallback(
    std::function<void(const std::vector<BehaviorAnalysis>&)> callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    result_callback_ = callback;
}

void VehiclePerceptionSystem::registerStateCallback(
    std::function<void(SystemState)> callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    state_callback_ = callback;
}

SystemPerformance VehiclePerceptionSystem::getPerformanceStats() const {
    std::lock_guard<std::mutex> lock(performance_mutex_);
    return performance_stats_;
}

bool VehiclePerceptionSystem::reset() {
    // 保存当前状态
    SystemState current_state = state_;
    
    // 停止系统
    stop();
    
    // 重新初始化
    bool success = initialize(config_);
    
    // 如果之前是运行状态，重新启动
    if (success && current_state == SystemState::RUNNING) {
        success = start();
    }
    
    return success;
}

void VehiclePerceptionSystem::processFrame(const cv::Mat& frame, uint64_t timestamp) {
    if (paused_) {
        std::unique_lock<std::mutex> lock(pause_mutex_);
        pause_cv_.wait(lock, [this]() { return !paused_ || state_ != SystemState::RUNNING; });
        
        if (state_ != SystemState::RUNNING) {
            return;
        }
    }
    
    auto total_start = std::chrono::steady_clock::now();
    
    try {
        // 1. 目标检测
        auto detect_start = std::chrono::steady_clock::now();
        auto detections = object_detector_->detect(frame);
        auto detect_end = std::chrono::steady_clock::now();
        float detect_ms = std::chrono::duration<float, std::milli>(detect_end - detect_start).count();
        
        // 2. 目标跟踪
        auto track_start = std::chrono::steady_clock::now();
        auto tracked_objects = object_tracker_->update(detections, timestamp);
        auto track_end = std::chrono::steady_clock::now();
        float track_ms = std::chrono::duration<float, std::milli>(track_end - track_start).count();
        
        // 3. 行为分析
        auto analysis_start = std::chrono::steady_clock::now();
        auto behaviors = behavior_analyzer_->analyze(tracked_objects);
        auto analysis_end = std::chrono::steady_clock::now();
        float analysis_ms = std::chrono::duration<float, std::milli>(analysis_end - analysis_start).count();
        
        // 4. LLM增强分析(如果启用)
        if (llm_enhancer_ && timestamp % (config_.llm.analysis_interval * 1000) == 0) {
            behaviors = llm_enhancer_->enhanceAnalysis(behaviors, tracked_objects);
        }
        
        // 5. 结果处理
        result_processor_->process(behaviors, frame, timestamp);
        
        // 6. 缓存结果并触发回调
        {
            std::lock_guard<std::mutex> lock(results_mutex_);
            last_results_ = behaviors;
        }
        
        {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            if (result_callback_) {
                result_callback_(behaviors);
            }
        }
        
        // 7. 更新性能统计
        auto total_end = std::chrono::steady_clock::now();
        float total_ms = std::chrono::duration<float, std::milli>(total_end - total_start).count();
        updatePerformanceStats(detect_ms, track_ms, analysis_ms, total_ms);
        
    } catch (const std::exception& e) {
        LOG_ERROR("Error processing frame: {}", e.what());
        setState(SystemState::ERROR);
    }
}

void VehiclePerceptionSystem::setState(SystemState new_state) {
    state_ = new_state;
    
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (state_callback_) {
        state_callback_(new_state);
    }
}

void VehiclePerceptionSystem::resetPerformanceStats() {
    std::lock_guard<std::mutex> lock(performance_mutex_);
    performance_stats_ = SystemPerformance();
}

void VehiclePerceptionSystem::updatePerformanceStats(float detection_ms, float tracking_ms, 
                                                   float analysis_ms, float total_ms) {
    std::lock_guard<std::mutex> lock(performance_mutex_);
    
    // 简单的滑动平均更新
    const float alpha = 0.2f; // 平滑因子
    
    performance_stats_.detection_time_ms = 
        alpha * detection_ms + (1 - alpha) * performance_stats_.detection_time_ms;
        
    performance_stats_.tracking_time_ms = 
        alpha * tracking_ms + (1 - alpha) * performance_stats_.tracking_time_ms;
        
    performance_stats_.analysis_time_ms = 
        alpha * analysis_ms + (1 - alpha) * performance_stats_.analysis_time_ms;
        
    performance_stats_.total_latency_ms = 
        alpha * total_ms + (1 - alpha) * performance_stats_.total_latency_ms;
        
    // 计算帧率
    if (total_ms > 0) {
        performance_stats_.fps = alpha * (1000.0f / total_ms) + (1 - alpha) * performance_stats_.fps;
    }
    
    // 这里可以添加CPU/GPU使用率和内存使用的更新逻辑
}
