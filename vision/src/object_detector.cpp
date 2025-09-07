/**
 * @file object_detector.cpp
 * @brief 目标检测模块实现 - 基于深度学习的实时目标检测
 * @author pengchengkang
 * @date 2025-9-7
 * 
 * 功能描述：
 * - 支持多种深度学习模型格式（ONNX、TensorFlow、Darknet）
 * - 提供GPU/CPU自适应推理加速
 * - 实现非极大值抑制和置信度过滤
 * - 支持批量检测和性能统计
 */

#include "module_interface.hpp"
#include "logger.hpp"
#include <opencv2/dnn.hpp>
#include <opencv2/opencv.hpp>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <chrono>

/**
 * @brief 目标检测器实现类
 * 基于OpenCV DNN模块实现的通用目标检测器，支持多种模型格式
 */
class ObjectDetector : public IObjectDetector {
private:
    cv::dnn::Net net_;
    std::vector<std::string> class_names_;
    SystemConfig::DetectorConfig config_;
    DetectionPerformance perf_stats_;
    cv::Size input_size_;
    std::vector<std::string> output_names_;
    
public:
    /**
     * @brief 构造函数，初始化默认类别名称
     */
    ObjectDetector() {
        // 默认类别名称（COCO数据集的相关类别）
        class_names_ = {
            "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck",
            "boat", "traffic light", "fire hydrant", "stop sign", "parking meter", "bench",
            "bird", "cat", "dog", "horse", "sheep", "cow", "elephant", "bear", "zebra",
            "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee",
            "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove",
            "skateboard", "surfboard", "tennis racket", "bottle", "wine glass", "cup",
            "fork", "knife", "spoon", "bowl", "banana", "apple", "sandwich", "orange",
            "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair", "couch",
            "potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse",
            "remote", "keyboard", "cell phone", "microwave", "oven", "toaster", "sink",
            "refrigerator", "book", "clock", "vase", "scissors", "teddy bear", "hair drier",
            "toothbrush"
        };
    }
    
    /**
     * @brief 初始化目标检测器
     * @param config 检测器配置参数
     * @return bool 初始化是否成功
     */
    bool initialize(const SystemConfig::DetectorConfig& config) override {
        try {
            config_ = config;
            input_size_ = cv::Size(config.input_width, config.input_height);
            
            // 检查模型文件是否存在
            if (!std::filesystem::exists(config.model_path)) {
                LOG_ERROR("Model file not found: {}", config.model_path);
                return false;
            }
            
            // 根据文件扩展名确定模型格式
            std::string ext = config.model_path.substr(config.model_path.find_last_of(".") + 1);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            
            if (ext == "onnx") {
                net_ = cv::dnn::readNetFromONNX(config.model_path);
            } else if (ext == "pb") {
                net_ = cv::dnn::readNetFromTensorflow(config.model_path);
            } else if (ext == "weights") {
                // YOLO Darknet格式需要配置文件
                std::string cfg_path = config.model_path;
                size_t pos = cfg_path.find_last_of(".");
                if (pos != std::string::npos) {
                    cfg_path = cfg_path.substr(0, pos) + ".cfg";
                }
                if (std::filesystem::exists(cfg_path)) {
                    net_ = cv::dnn::readNetFromDarknet(cfg_path, config.model_path);
                } else {
                    LOG_ERROR("YOLO config file not found: {}", cfg_path);
                    return false;
                }
            } else {
                LOG_ERROR("Unsupported model format: {}", ext);
                return false;
            }
            
            if (net_.empty()) {
                LOG_ERROR("Failed to load model: {}", config.model_path);
                return false;
            }
            
            // 设置计算后端
            if (cv::cuda::getCudaEnabledDeviceCount() > 0) {
                net_.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
                net_.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
                LOG_INFO("Using CUDA backend for inference");
            } else {
                net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
                net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
                LOG_INFO("Using CPU backend for inference");
            }
            
            // 获取输出层名称
            output_names_ = net_.getUnconnectedOutLayersNames();
            
            LOG_INFO("Object detector initialized successfully");
            LOG_INFO("Model: {}", config.model_path);
            LOG_INFO("Input size: {}x{}", config.input_width, config.input_height);
            LOG_INFO("Confidence threshold: {}", config.confidence_threshold);
            LOG_INFO("NMS threshold: {}", config.nms_threshold);
            
            return true;
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to initialize object detector: {}", e.what());
            return false;
        }
    }
    
    /**
     * @brief 检测单张图像中的目标
     * @param image 输入图像
     * @return std::vector<Detection> 检测结果列表
     */
    std::vector<Detection> detect(const cv::Mat& image) override {
        if (net_.empty() || image.empty()) {
            return {};
        }
        
        auto start_time = std::chrono::steady_clock::now();
        
        // 预处理
        auto preprocess_start = std::chrono::steady_clock::now();
        cv::Mat blob;
        cv::dnn::blobFromImage(image, blob, 1.0/255.0, input_size_, cv::Scalar(0,0,0), true, false);
        net_.setInput(blob);
        auto preprocess_end = std::chrono::steady_clock::now();
        
        // 推理
        auto inference_start = std::chrono::steady_clock::now();
        std::vector<cv::Mat> outputs;
        net_.forward(outputs, output_names_);
        auto inference_end = std::chrono::steady_clock::now();
        
        // 后处理
        auto postprocess_start = std::chrono::steady_clock::now();
        std::vector<Detection> detections = postprocess(outputs, image.size());
        auto postprocess_end = std::chrono::steady_clock::now();
        
        // 更新性能统计
        float preprocess_ms = std::chrono::duration<float, std::milli>(preprocess_end - preprocess_start).count();
        float inference_ms = std::chrono::duration<float, std::milli>(inference_end - inference_start).count();
        float postprocess_ms = std::chrono::duration<float, std::milli>(postprocess_end - postprocess_start).count();
        
        updatePerformanceStats(preprocess_ms, inference_ms, postprocess_ms);
        
        return detections;
    }
    
    std::vector<std::vector<Detection>> detectBatch(const std::vector<cv::Mat>& images) override {
        std::vector<std::vector<Detection>> results;
        results.reserve(images.size());
        
        for (const auto& image : images) {
            results.push_back(detect(image));
        }
        
        return results;
    }
    
    const std::vector<std::string>& getClassNames() const override {
        return class_names_;
    }
    
    void setConfidenceThreshold(float threshold) override {
        config_.confidence_threshold = threshold;
    }
    
    void setNmsThreshold(float threshold) override {
        config_.nms_threshold = threshold;
    }
    
    const DetectionPerformance& getPerformanceStats() const override {
        return perf_stats_;
    }
    
private:
    /**
     * @brief 后处理网络输出，生成最终检测结果
     * @param outputs 网络输出张量列表
     * @param image_size 原始图像尺寸
     * @return std::vector<Detection> 处理后的检测结果
     */
    std::vector<Detection> postprocess(const std::vector<cv::Mat>& outputs, const cv::Size& image_size) {
        std::vector<int> class_ids;
        std::vector<float> confidences;
        std::vector<cv::Rect> boxes;
        
        // 解析网络输出
        for (const auto& output : outputs) {
            const float* data = (float*)output.data;
            
            for (int i = 0; i < output.rows; ++i) {
                const float* row = data + i * output.cols;
                
                // 跳过边界框坐标
                const float* scores = row + 5;
                
                cv::Point class_id_point;
                double max_class_score;
                cv::minMaxLoc(cv::Mat(1, class_names_.size(), CV_32F, (void*)scores), 
                             0, &max_class_score, 0, &class_id_point);
                
                if (max_class_score > config_.confidence_threshold) {
                    float center_x = row[0] * image_size.width;
                    float center_y = row[1] * image_size.height;
                    float width = row[2] * image_size.width;
                    float height = row[3] * image_size.height;
                    
                    int left = static_cast<int>(center_x - width / 2);
                    int top = static_cast<int>(center_y - height / 2);
                    
                    class_ids.push_back(class_id_point.x);
                    confidences.push_back(static_cast<float>(max_class_score));
                    boxes.push_back(cv::Rect(left, top, static_cast<int>(width), static_cast<int>(height)));
                }
            }
        }
        
        // 非极大值抑制
        std::vector<int> indices;
        cv::dnn::NMSBoxes(boxes, confidences, config_.confidence_threshold, config_.nms_threshold, indices);
        
        // 构建检测结果
        std::vector<Detection> detections;
        detections.reserve(indices.size());
        
        for (int idx : indices) {
            Detection det;
            det.class_id = static_cast<ObjectClass>(class_ids[idx]);
            det.class_name = (class_ids[idx] < class_names_.size()) ? class_names_[class_ids[idx]] : "unknown";
            det.confidence = confidences[idx];
            det.bbox = cv::Rect2f(boxes[idx]);
            det.center = cv::Point2f(det.bbox.x + det.bbox.width / 2, det.bbox.y + det.bbox.height / 2);
            det.area = det.bbox.width * det.bbox.height;
            det.aspect_ratio = det.bbox.width / det.bbox.height;
            det.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            
            detections.push_back(det);
        }
        
        return detections;
    }
    
    /**
     * @brief 更新性能统计信息
     * @param preprocess_ms 预处理耗时（毫秒）
     * @param inference_ms 推理耗时（毫秒）
     * @param postprocess_ms 后处理耗时（毫秒）
     */
    void updatePerformanceStats(float preprocess_ms, float inference_ms, float postprocess_ms) {
        const float alpha = 0.1f; // 平滑因子
        
        perf_stats_.preprocess_time_ms = alpha * preprocess_ms + (1 - alpha) * perf_stats_.preprocess_time_ms;
        perf_stats_.inference_time_ms = alpha * inference_ms + (1 - alpha) * perf_stats_.inference_time_ms;
        perf_stats_.postprocess_time_ms = alpha * postprocess_ms + (1 - alpha) * perf_stats_.postprocess_time_ms;
        
        perf_stats_.frame_count++;
        
        float total_time = preprocess_ms + inference_ms + postprocess_ms;
        if (total_time > 0) {
            perf_stats_.fps = alpha * (1000.0f / total_time) + (1 - alpha) * perf_stats_.fps;
        }
    }
};

// 实现工厂函数
std::unique_ptr<IObjectDetector> IObjectDetector::create() {
    return std::make_unique<ObjectDetector>();
}
