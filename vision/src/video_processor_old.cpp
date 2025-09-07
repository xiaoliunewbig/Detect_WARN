/**
 * @file video_processor.cpp
 * @brief 视频处理模块实现 - 负责视频输入、解码和预处理
 * @author pengchengkang
 * @date 2025-9-7
 * 
 * 功能描述：
 * - 支持多种视频源（摄像头、文件、RTSP流）
 * - 提供畸变矫正和ROI裁剪功能
 * - 异步视频帧处理和回调机制
 * - 自动帧率控制和错误恢复
 */

#include "module_interface.hpp"
#include "logger.hpp"
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
    VideoSourceConfig config_;
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
            
            // 获取实际视频属性
            properties_.width = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_WIDTH));
            properties_.height = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_HEIGHT));
            properties_.fps = static_cast<float>(cap_.get(cv::CAP_PROP_FPS));
            properties_.is_stream = (config.type == "rtsp" || config.type == "camera");
            
            // 设置相机参数用于畸变矫正
            if (camera_params.fx > 0 && camera_params.fy > 0) {
                camera_matrix_ = (cv::Mat_<double>(3, 3) << 
                    camera_params.fx, 0, camera_params.cx,
                    0, camera_params.fy, camera_params.cy,
                    0, 0, 1);
                
                if (!camera_params.distortion.empty()) {
                    dist_coeffs_ = camera_params.distortion.clone();
                    distortion_correction_enabled_ = true;
                }
            }
            
            // 初始化ROI为全图
            roi_ = cv::Rect(0, 0, properties_.width, properties_.height);
            
            state_ = ProcessingState::IDLE;
            LOG_INFO("Video processor initialized successfully. Resolution: {}x{}, FPS: {}", 
                    properties_.width, properties_.height, properties_.fps);
            
            return true;
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to initialize video processor: {}", e.what());
            return false;
        }
    }
    
    /**
     * @brief 启动视频处理
     * @return bool 启动是否成功
     */
    bool start() override {
        if (state_ == ProcessingState::PROCESSING) {
            LOG_WARN("Video processor is already running");
            return true;
        }
        
        if (!cap_.isOpened()) {
            LOG_ERROR("Video capture is not opened");
            return false;
        }
        
        running_ = true;
        paused_ = false;
        state_ = ProcessingState::PROCESSING;
        
        // 启动处理线程
        processing_thread_ = std::thread(&VideoProcessor::processLoop, this);
        
        LOG_INFO("Video processor started");
        return true;
    }
    
    /**
     * @brief 停止视频处理
     */
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
        
        LOG_INFO("Attempting to open video source: {}", config_.source);
        LOG_INFO("Will wait up to {} seconds with {} second intervals", 
                config_.connection_timeout_sec, config_.retry_interval_sec);
        
        auto start_time = std::chrono::steady_clock::now();
        int attempt = 0;
        
        while (attempt < config_.max_retry_attempts) {
            attempt++;
            
            LOG_INFO("Connection attempt {}/{}", attempt, config_.max_retry_attempts);
            
            if (openVideoSource()) {
                LOG_INFO("Successfully connected to video source after {} attempts", attempt);
                return true;
            }
            
            // 检查是否超时
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start_time).count();
            
            if (elapsed >= config_.connection_timeout_sec) {
                LOG_WARN("Connection timeout after {} seconds", elapsed);
                break;
            }
            
            // 等待重试间隔
            if (attempt < config_.max_retry_attempts) {
                LOG_INFO("Waiting {} seconds before next attempt...", config_.retry_interval_sec);
                std::this_thread::sleep_for(std::chrono::seconds(config_.retry_interval_sec));
            }
        }
        
        LOG_ERROR("Failed to connect to video source after {} attempts", attempt);
        return false;
    }
    
    /**
     * @brief 打开视频源（单次尝试）
     * @return bool 是否成功打开
     */
    bool openVideoSource() {
        // 判断视频源类型并打开
        if (isNumeric(config_.source)) {
            // 摄像头设备
            int camera_id = std::stoi(config_.source);
            if (!cap_.open(camera_id)) {
                LOG_DEBUG("Failed to open camera {}", camera_id);
                return false;
            }
        } else if (config_.source.find("rtsp://") == 0 || 
                   config_.source.find("http://") == 0 ||
                   config_.source.find("https://") == 0) {
            // 网络流
            if (!cap_.open(config_.source)) {
                LOG_DEBUG("Failed to open stream {}", config_.source);
                return false;
            }
        } else {
            // 视频文件
            if (!cap_.open(config_.source)) {
                LOG_DEBUG("Failed to open video file {}", config_.source);
                return false;
            }
        }
        
        // 验证是否真正打开
        if (!cap_.isOpened()) {
            LOG_DEBUG("Video capture is not opened");
            return false;
        }
        
        // 尝试读取一帧来验证
        cv::Mat test_frame;
        if (!cap_.read(test_frame) || test_frame.empty()) {
            LOG_DEBUG("Cannot read frame from video source");
            cap_.release();
            return false;
        }
        
        LOG_DEBUG("Successfully opened video source: {}", config_.source);
        return true;
    }
    
    /**
     * @brief 检查字符串是否为数字
     * @param str 输入字符串
     * @return bool 是否为数字
     */
    bool isNumeric(const std::string& str) {
        if (str.empty()) return false;
        return std::all_of(str.begin(), str.end(), ::isdigit);
    }
    
    /**
     * @brief 视频处理主循环
     * 在独立线程中运行，负责连续读取和处理视频帧
     */
    void processLoop() {
        cv::Mat frame, processed_frame;
        auto last_frame_time = std::chrono::steady_clock::now();
        double target_interval_ms = 1000.0 / properties_.fps;
        
        while (running_) {
            if (paused_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            
            // 读取帧
            if (!cap_.read(frame)) {
                if (properties_.is_stream) {
                    LOG_WARN("Failed to read frame from stream, retrying...");
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                } else {
                    LOG_INFO("Reached end of video file");
                    break;
                }
            }
            
            if (frame.empty()) {
                continue;
            }
            
            // 处理帧
            processed_frame = frame.clone();
            
            // 畸变矫正
            if (distortion_correction_enabled_ && !camera_matrix_.empty() && !dist_coeffs_.empty()) {
                cv::undistort(frame, processed_frame, camera_matrix_, dist_coeffs_);
            }
            
            // ROI裁剪
            if (roi_.width > 0 && roi_.height > 0 && 
                roi_.x >= 0 && roi_.y >= 0 &&
                roi_.x + roi_.width <= processed_frame.cols &&
                roi_.y + roi_.height <= processed_frame.rows) {
                processed_frame = processed_frame(roi_);
            }
            
            // 生成时间戳
            auto now = std::chrono::steady_clock::now();
            uint64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count();
            
            // 调用回调函数
            {
                std::lock_guard<std::mutex> lock(callback_mutex_);
                if (frame_callback_) {
                    frame_callback_(processed_frame, timestamp);
                }
            }
            
            // 帧率控制
            auto frame_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_frame_time).count();
            
            if (frame_duration < target_interval_ms) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(static_cast<int>(target_interval_ms - frame_duration)));
            }
            
            last_frame_time = now;
        }
        
        state_ = ProcessingState::IDLE;
    }
};

// 实现工厂函数
std::unique_ptr<IVideoProcessor> IVideoProcessor::create() {
    return std::make_unique<VideoProcessor>();
}
