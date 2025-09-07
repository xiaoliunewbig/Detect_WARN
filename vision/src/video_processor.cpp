/**
 * @file video_processor.cpp
 * @brief 视频处理器实现
 * @author Detect_WARN Team
 * @date 2024
 * 
 * 该文件实现了视频处理器模块，负责从各种视频源（摄像头、文件、网络流）
 * 读取视频帧，进行预处理（畸变校正、ROI裁剪），并通过回调函数将处理后的
 * 帧传递给后续模块。支持摄像头连接重试机制和超时控制。
 */

#include "../include/vehicle_perception_system.hpp"
#include "../../interface/module_interface.hpp"
#include "../../config/config.hpp"
#include "../../data/data_structs.hpp"
#include "../../main/logger.hpp"
#include <opencv2/opencv.hpp>
#include <thread>
#include <chrono>
#include <filesystem>
#include <atomic>

/**
 * @brief 视频处理器实现类
 * 继承自IVideoProcessor接口，提供完整的视频处理功能
 */
class VideoProcessor : public IVideoProcessor {
private:
    cv::VideoCapture cap_;
    SystemConfig::VideoSourceConfig config_;
    ProcessingState state_;
    VideoProperties properties_;
    std::function<void(const cv::Mat&, uint64_t)> frame_callback_;
    std::atomic<bool> running_;
    std::thread processing_thread_;
    std::mutex callback_mutex_;
    
    // ROI and distortion correction
    cv::Rect roi_rect_;
    bool roi_enabled_;
    CameraParams camera_params_;
    cv::Mat camera_matrix_;
    cv::Mat distortion_coeffs_;
    bool distortion_correction_enabled_;
    
    // Processing control
    std::atomic<bool> paused_;

public:
    /**
     * @brief 构造函数，初始化视频处理器状态
     */
    VideoProcessor() : state_(ProcessingState::IDLE), running_(false), 
                      roi_enabled_(false), distortion_correction_enabled_(false), paused_(false) {}
    
    ~VideoProcessor() override {
        stop();
    }
    
    /**
     * @brief 初始化视频处理器
     * @param config 视频源配置参数
     * @param camera_params 相机标定参数
     * @return bool 初始化是否成功
     */
    bool initialize(const SystemConfig::VideoSourceConfig& config, 
                   const CameraParams& camera_params) override {
        config_ = config;
        camera_params_ = camera_params;
        
        // 设置相机矩阵和畸变系数
        if (camera_params_.fx > 0 && camera_params_.fy > 0) {
            camera_matrix_ = (cv::Mat_<double>(3,3) << 
                camera_params_.fx, 0, camera_params_.cx,
                0, camera_params_.fy, camera_params_.cy,
                0, 0, 1);
        }
        if (!camera_params_.distortion.empty()) {
            distortion_coeffs_ = camera_params_.distortion.clone();
        }
        
        // 尝试打开视频源，支持等待和重试
        if (!openVideoSourceWithRetry()) {
            LOG_ERROR("Failed to open video source after retries: {}", config_.source);
            return false;
        }
        
        // 设置视频参数
        if (config_.width > 0 && config_.height > 0) {
            cap_.set(cv::CAP_PROP_FRAME_WIDTH, config_.width);
            cap_.set(cv::CAP_PROP_FRAME_HEIGHT, config_.height);
        }
        
        if (config_.fps > 0) {
            cap_.set(cv::CAP_PROP_FPS, config_.fps);
        }
        
        // 获取实际的视频属性
        properties_.width = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_WIDTH));
        properties_.height = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_HEIGHT));
        properties_.fps = cap_.get(cv::CAP_PROP_FPS);
        int total_frames = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_COUNT));
        properties_.is_stream = (total_frames <= 0);
        
        state_ = ProcessingState::IDLE;
        LOG_INFO("Video processor initialized successfully. Resolution: {}x{}, FPS: {}", 
                properties_.width, properties_.height, properties_.fps);
        
        return true;
    }
    
    bool start() override {
        if (state_ == ProcessingState::PROCESSING) {
            return true;
        }
        
        if (!cap_.isOpened()) {
            LOG_ERROR("Cannot start: video source not opened");
            return false;
        }
        
        running_ = true;
        paused_ = false;
        state_ = ProcessingState::PROCESSING;
        processing_thread_ = std::thread(&VideoProcessor::processLoop, this);
        LOG_INFO("Video processing started");
        return true;
    }
    
    void stop() override {
        if (state_ == ProcessingState::IDLE) {
            return;
        }
        
        running_ = false;
        state_ = ProcessingState::IDLE;
        
        if (processing_thread_.joinable()) {
            processing_thread_.join();
        }
        
        if (cap_.isOpened()) {
            cap_.release();
        }
        
        LOG_INFO("Video processor stopped");
    }
    
    void pause() override {
        if (state_ == ProcessingState::PROCESSING) {
            paused_ = true;
            state_ = ProcessingState::PAUSED;
            LOG_INFO("Video processor paused");
        }
    }
    
    void resume() override {
        if (state_ == ProcessingState::PAUSED) {
            paused_ = false;
            state_ = ProcessingState::PROCESSING;
            LOG_INFO("Video processor resumed");
        }
    }
    
    ProcessingState getState() const override {
        return state_;
    }
    
    VideoProperties getVideoProperties() const override {
        return properties_;
    }
    
    bool seek(double timestamp) override {
        if (!cap_.isOpened()) {
            return false;
        }
        
        // 对于摄像头和流，不支持seek操作
        if (properties_.is_stream) {
            LOG_WARN("Seek operation not supported for live streams");
            return false;
        }
        
        // 计算目标帧号
        double fps = cap_.get(cv::CAP_PROP_FPS);
        if (fps <= 0) fps = 30.0; // 默认帧率
        
        int target_frame = static_cast<int>(timestamp * fps);
        return cap_.set(cv::CAP_PROP_POS_FRAMES, target_frame);
    }
    
    double getCurrentTimestamp() const override {
        if (!cap_.isOpened()) {
            return -1.0;
        }
        
        if (properties_.is_stream) {
            // 对于流，返回当前系统时间戳
            auto now = std::chrono::steady_clock::now();
            auto duration = now.time_since_epoch();
            return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() / 1000.0;
        }
        
        // 对于文件，返回当前播放位置
        double fps = cap_.get(cv::CAP_PROP_FPS);
        if (fps <= 0) fps = 30.0;
        
        int current_frame = static_cast<int>(cap_.get(cv::CAP_PROP_POS_FRAMES));
        return current_frame / fps;
    }
    
    void registerFrameCallback(std::function<void(const cv::Mat&, uint64_t)> callback) override {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        frame_callback_ = callback;
    }
    
    void setROI(const cv::Rect& roi) override {
        roi_rect_ = roi;
        roi_enabled_ = !roi.empty();
        LOG_INFO("ROI set to ({}, {}, {}, {})", roi.x, roi.y, roi.width, roi.height);
    }
    
    cv::Rect getROI() const override {
        return roi_rect_;
    }
    
    void setDistortionCorrection(bool enable) override {
        if (enable && camera_matrix_.empty()) {
            LOG_WARN("Cannot enable distortion correction: camera parameters not set");
            return;
        }
        distortion_correction_enabled_ = enable;
        LOG_INFO("Distortion correction {}", enable ? "enabled" : "disabled");
    }

private:
    /**
     * @brief 带重试机制的视频源打开函数
     * @return bool 是否成功打开视频源
     */
    bool openVideoSourceWithRetry() {
        if (!config_.wait_for_device) {
            // 如果不等待设备，直接尝试一次
            return openVideoSource();
        }
        
        LOG_INFO("Attempting to open video source with retry mechanism: {}", config_.source);
        LOG_INFO("Timeout: {}s, Retry interval: {}s, Max attempts: {}", 
                config_.connection_timeout_sec, config_.retry_interval_sec, config_.max_retry_attempts);
        
        auto start_time = std::chrono::steady_clock::now();
        int attempt = 0;
        
        while (attempt < config_.max_retry_attempts) {
            attempt++;
            LOG_INFO("Connection attempt {}/{}", attempt, config_.max_retry_attempts);
            
            if (openVideoSource()) {
                LOG_INFO("Successfully connected to video source on attempt {}", attempt);
                return true;
            }
            
            // 检查是否超时
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time);
            
            if (elapsed.count() >= config_.connection_timeout_sec) {
                LOG_ERROR("Connection timeout after {} seconds", elapsed.count());
                break;
            }
            
            // 如果不是最后一次尝试，等待重试间隔
            if (attempt < config_.max_retry_attempts) {
                LOG_INFO("Waiting {} seconds before next attempt...", config_.retry_interval_sec);
                std::this_thread::sleep_for(std::chrono::seconds(config_.retry_interval_sec));
            }
        }
        
        LOG_ERROR("Failed to connect to video source after {} attempts", attempt);
        return false;
    }
    
    /**
     * @brief 单次尝试打开视频源
     * @return bool 是否成功打开
     */
    bool openVideoSource() {
        try {
            // 检查source是否为数字（摄像头索引）
            if (isNumeric(config_.source)) {
                int camera_id = std::stoi(config_.source);
                LOG_DEBUG("Attempting to open camera with ID: {}", camera_id);
                return cap_.open(camera_id);
            } else {
                // 文件路径或网络流
                LOG_DEBUG("Attempting to open video source: {}", config_.source);
                return cap_.open(config_.source);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Exception while opening video source: {}", e.what());
            return false;
        }
    }
    
    /**
     * @brief 检查字符串是否为数字
     * @param str 待检查的字符串
     * @return bool 是否为数字
     */
    bool isNumeric(const std::string& str) {
        if (str.empty()) return false;
        
        for (char c : str) {
            if (!std::isdigit(c)) {
                return false;
            }
        }
        return true;
    }
    
    /**
     * @brief 视频处理主循环
     */
    void processLoop() {
        cv::Mat frame;
        auto last_frame_time = std::chrono::steady_clock::now();
        double frame_interval = 1000.0 / properties_.fps; // 毫秒
        
        while (running_) {
            if (paused_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            
            if (!cap_.read(frame)) {
                LOG_WARN("Failed to read frame from video source");
                if (properties_.is_stream) {
                    // 对于流，尝试重新连接
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                } else {
                    // 对于文件，到达末尾
                    LOG_INFO("Reached end of video file");
                    break;
                }
            }
            
            if (frame.empty()) {
                continue;
            }
            
            // 应用畸变校正
            if (distortion_correction_enabled_ && !camera_matrix_.empty() && !distortion_coeffs_.empty()) {
                cv::Mat undistorted;
                cv::undistort(frame, undistorted, camera_matrix_, distortion_coeffs_);
                frame = undistorted;
            }
            
            // 应用ROI裁剪
            if (roi_enabled_ && !roi_rect_.empty()) {
                // 确保ROI在图像范围内
                cv::Rect safe_roi = roi_rect_ & cv::Rect(0, 0, frame.cols, frame.rows);
                if (!safe_roi.empty()) {
                    frame = frame(safe_roi);
                }
            }
            
            // 调用帧回调函数
            if (frame_callback_) {
                uint64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count();
                
                std::lock_guard<std::mutex> lock(callback_mutex_);
                frame_callback_(frame, timestamp);
            }
            
            // 控制帧率
            if (!properties_.is_stream) {
                auto current_time = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    current_time - last_frame_time).count();
                
                if (elapsed < frame_interval) {
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(static_cast<int>(frame_interval - elapsed)));
                }
                last_frame_time = std::chrono::steady_clock::now();
            }
        }
        
        LOG_INFO("Video processing loop ended");
    }
};

// 静态工厂函数实现
std::unique_ptr<IVideoProcessor> IVideoProcessor::create() {
    return std::make_unique<VideoProcessor>();
}
