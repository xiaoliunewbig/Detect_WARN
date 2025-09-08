#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <opencv2/opencv.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <iostream>

using json = nlohmann::json;

// 摄像头参数
struct CameraParams {
    float fx = 0.0f;               // 焦距x
    float fy = 0.0f;               // 焦距y
    float cx = 0.0f;               // 主点x
    float cy = 0.0f;               // 主点y
    cv::Mat distortion;            // 畸变系数
    float height = 1.5f;           // 安装高度(米)
    float pitch = 0.0f;            // 俯仰角(度)
    float yaw = 0.0f;              // 偏航角(度)
    float fov_h = 0.0f;            // 水平视场角(度)
    float fov_v = 0.0f;            // 垂直视场角(度)
    
    // 从JSON加载
    void fromJson(const json& j) {
        if (j.contains("fx")) fx = j["fx"];
        if (j.contains("fy")) fy = j["fy"];
        if (j.contains("cx")) cx = j["cx"];
        if (j.contains("cy")) cy = j["cy"];
        
        if (j.contains("distortion") && j["distortion"].is_array()) {
            std::vector<double> dist_coeffs = j["distortion"].get<std::vector<double>>();
            distortion = cv::Mat(dist_coeffs).reshape(1, 1);
        }
        
        if (j.contains("height")) height = j["height"];
        if (j.contains("pitch")) pitch = j["pitch"];
        if (j.contains("yaw")) yaw = j["yaw"];
        if (j.contains("fov_h")) fov_h = j["fov_h"];
        if (j.contains("fov_v")) fov_v = j["fov_v"];
    }
    
    // 转换为JSON
    json toJson() const {
        json j;
        j["fx"] = fx;
        j["fy"] = fy;
        j["cx"] = cx;
        j["cy"] = cy;
        
        if (!distortion.empty()) {
            std::vector<double> dist_vec;
            if (distortion.isContinuous()) {
                dist_vec.assign((double*)distortion.data, (double*)distortion.data + distortion.total());
            }
            j["distortion"] = dist_vec;
        }
        
        j["height"] = height;
        j["pitch"] = pitch;
        j["yaw"] = yaw;
        j["fov_h"] = fov_h;
        j["fov_v"] = fov_v;
        
        return j;
    }
};

// 车辆参数
struct VehicleParams {
    float width = 1.8f;            // 宽度(米)
    float length = 4.5f;           // 长度(米)
    float height = 1.5f;           // 高度(米)
    float front_overhang = 0.9f;   // 前悬长度(米)
    float wheelbase = 2.7f;        // 轴距(米)
    float max_speed = 120.0f;      // 最大速度(km/h)
    
    // 从JSON加载
    void fromJson(const json& j) {
        if (j.contains("width")) width = j["width"];
        if (j.contains("length")) length = j["length"];
        if (j.contains("height")) height = j["height"];
        if (j.contains("front_overhang")) front_overhang = j["front_overhang"];
        if (j.contains("wheelbase")) wheelbase = j["wheelbase"];
        if (j.contains("max_speed")) max_speed = j["max_speed"];
    }
    
    // 转换为JSON
    json toJson() const {
        return {
            {"width", width},
            {"length", length},
            {"height", height},
            {"front_overhang", front_overhang},
            {"wheelbase", wheelbase},
            {"max_speed", max_speed}
        };
    }
};

// 系统配置结构
struct SystemConfig {
    // 视频源配置
    struct VideoSourceConfig {
        std::string source = "0";     // 视频源路径或摄像头ID
        int width = 640;              // 视频宽度
        int height = 480;             // 视频高度
        float fps = 30.0f;            // 帧率
        bool enable_roi = false;      // 启用ROI
        cv::Rect roi_rect;            // ROI区域
        bool correct_distortion = false; // 畸变矫正
        
        // 连接和重试配置
        int connection_timeout_sec = 60;    // 连接超时时间（秒）
        int retry_interval_sec = 5;         // 重试间隔（秒）
        int max_retry_attempts = 12;        // 最大重试次数（60秒/5秒=12次）
        bool wait_for_device = true;        // 等待设备连接
        std::string decode_mode = "cuda";  // 解码模式: cpu, cuda, vaapi
        
        // 从JSON加载
        void fromJson(const json& j) {
            if (j.contains("source")) source = j["source"];
            if (j.contains("width")) width = j["width"];
            if (j.contains("height")) height = j["height"];
            if (j.contains("fps")) fps = j["fps"];
            if (j.contains("enable_roi")) enable_roi = j["enable_roi"];
            if (j.contains("correct_distortion")) correct_distortion = j["correct_distortion"];
            
            // 连接配置
            if (j.contains("connection_timeout_sec")) connection_timeout_sec = j["connection_timeout_sec"];
            if (j.contains("retry_interval_sec")) retry_interval_sec = j["retry_interval_sec"];
            if (j.contains("max_retry_attempts")) max_retry_attempts = j["max_retry_attempts"];
            if (j.contains("wait_for_device")) wait_for_device = j["wait_for_device"];
        }
        
        // 转换为JSON
        json toJson() const {
            json j;
            j["source"] = source;
            j["width"] = width;
            j["height"] = height;
            j["fps"] = fps;
            j["enable_roi"] = enable_roi;
            j["correct_distortion"] = correct_distortion;
            
            // 连接配置
            j["connection_timeout_sec"] = connection_timeout_sec;
            j["retry_interval_sec"] = retry_interval_sec;
            j["max_retry_attempts"] = max_retry_attempts;
            j["wait_for_device"] = wait_for_device;
            return j;
        }
    } video;
    
    // 检测配置
    struct DetectorConfig {
        std::string model_path = "models/yolov8n.engine"; // 模型路径
        std::string model_type = "yolov8";                // 模型类型
        int input_width = 640;                            // 输入宽度
        int input_height = 640;                           // 输入高度
        float confidence_threshold = 0.5f;                 // 置信度阈值
        float nms_threshold = 0.45f;                       // NMS阈值
        std::string precision = "fp16";                    // 精度: fp32, fp16, int8
        std::string calibration_path = "data/calibration"; // 校准数据路径(INT8时使用)
        
        // 从JSON加载
        void fromJson(const json& j) {
            if (j.contains("model_path")) model_path = j["model_path"];
            if (j.contains("model_type")) model_type = j["model_type"];
            if (j.contains("input_width")) input_width = j["input_width"];
            if (j.contains("input_height")) input_height = j["input_height"];
            if (j.contains("confidence_threshold")) confidence_threshold = j["confidence_threshold"];
            if (j.contains("nms_threshold")) nms_threshold = j["nms_threshold"];
            if (j.contains("precision")) precision = j["precision"];
            if (j.contains("calibration_path")) calibration_path = j["calibration_path"];
        }
        
        // 转换为JSON
        json toJson() const {
            return {
                {"model_path", model_path},
                {"model_type", model_type},
                {"input_width", input_width},
                {"input_height", input_height},
                {"confidence_threshold", confidence_threshold},
                {"nms_threshold", nms_threshold},
                {"precision", precision},
                {"calibration_path", calibration_path}
            };
        }
    } detector;
    
    // 跟踪配置
    struct TrackerConfig {
        std::string type = "deepsort";      // 跟踪器类型
        int max_age = 30;                   // 最大未检测到帧数
        int min_hits = 3;                   // 确认跟踪所需检测数
        float iou_threshold = 0.3f;         // IOU阈值
        bool use_appearance = true;         // 是否使用外观特征
        std::string reid_model_path = "models/reid.engine"; // ReID模型路径
        
        // 从JSON加载
        void fromJson(const json& j) {
            if (j.contains("type")) type = j["type"];
            if (j.contains("max_age")) max_age = j["max_age"];
            if (j.contains("min_hits")) min_hits = j["min_hits"];
            if (j.contains("iou_threshold")) iou_threshold = j["iou_threshold"];
            if (j.contains("use_appearance")) use_appearance = j["use_appearance"];
            if (j.contains("reid_model_path")) reid_model_path = j["reid_model_path"];
        }
        
        // 转换为JSON
        json toJson() const {
            return {
                {"type", type},
                {"max_age", max_age},
                {"min_hits", min_hits},
                {"iou_threshold", iou_threshold},
                {"use_appearance", use_appearance},
                {"reid_model_path", reid_model_path}
            };
        }
    } tracker;
    
    // 行为分析配置
    struct BehaviorConfig {
        float high_risk_distance = 10.0f;   // 高风险距离(米)
        float collision_risk_ttc = 3.0f;    // 碰撞风险时间阈值(秒)
        int trajectory_history_length = 30; // 轨迹历史长度
        float pedestrian_running_threshold = 2.5f; // 行人奔跑速度阈值(米/秒)
        float non_motor_speeding_threshold = 5.0f; // 非机动车超速阈值(米/秒)
        
        // 从JSON加载
        void fromJson(const json& j) {
            if (j.contains("high_risk_distance")) high_risk_distance = j["high_risk_distance"];
            if (j.contains("collision_risk_ttc")) collision_risk_ttc = j["collision_risk_ttc"];
            if (j.contains("trajectory_history_length")) trajectory_history_length = j["trajectory_history_length"];
            if (j.contains("pedestrian_running_threshold")) pedestrian_running_threshold = j["pedestrian_running_threshold"];
            if (j.contains("non_motor_speeding_threshold")) non_motor_speeding_threshold = j["non_motor_speeding_threshold"];
        }
        
        // 转换为JSON
        json toJson() const {
            return {
                {"high_risk_distance", high_risk_distance},
                {"collision_risk_ttc", collision_risk_ttc},
                {"trajectory_history_length", trajectory_history_length},
                {"pedestrian_running_threshold", pedestrian_running_threshold},
                {"non_motor_speeding_threshold", non_motor_speeding_threshold}
            };
        }
    } behavior;
    
    // LLM配置
    struct LLMConfig {
        bool enable = false;               // 是否启用
        std::string type = "api";          // 类型: local, api
        std::string server_address = "http://localhost:8000"; // 服务器地址
        int analysis_interval = 10;        // 分析间隔(帧)
        int max_tokens = 100;              // 最大生成tokens
        float temperature = 0.3f;          // 温度参数
        
        // 从JSON加载
        void fromJson(const json& j) {
            if (j.contains("enable")) enable = j["enable"];
            if (j.contains("type")) type = j["type"];
            if (j.contains("server_address")) server_address = j["server_address"];
            if (j.contains("analysis_interval")) analysis_interval = j["analysis_interval"];
            if (j.contains("max_tokens")) max_tokens = j["max_tokens"];
            if (j.contains("temperature")) temperature = j["temperature"];
        }
        
        // 转换为JSON
        json toJson() const {
            return {
                {"enable", enable},
                {"type", type},
                {"server_address", server_address},
                {"analysis_interval", analysis_interval},
                {"max_tokens", max_tokens},
                {"temperature", temperature}
            };
        }
    } llm;
    
    // 输出配置
    struct OutputConfig {
        bool save_video = false;           // 是否保存视频
        std::string video_path = "output/videos/"; // 视频保存路径
        bool save_results = true;          // 是否保存结果
        std::string results_path = "output/results/"; // 结果保存路径
        bool draw_bboxes = true;           // 是否绘制边界框
        bool draw_trails = true;           // 是否绘制轨迹
        bool draw_labels = true;           // 是否绘制标签
        bool log_to_file = true;           // 是否记录日志到文件
        std::string log_path = "logs/";    // 日志路径
        int log_level = 2;                 // 日志级别: 0(trace)-5(critical)
        
        // 从JSON加载
        void fromJson(const json& j) {
            if (j.contains("save_video")) save_video = j["save_video"];
            if (j.contains("video_path")) video_path = j["video_path"];
            if (j.contains("save_results")) save_results = j["save_results"];
            if (j.contains("results_path")) results_path = j["results_path"];
            if (j.contains("draw_bboxes")) draw_bboxes = j["draw_bboxes"];
            if (j.contains("draw_trails")) draw_trails = j["draw_trails"];
            if (j.contains("draw_labels")) draw_labels = j["draw_labels"];
            if (j.contains("log_to_file")) log_to_file = j["log_to_file"];
            if (j.contains("log_path")) log_path = j["log_path"];
            if (j.contains("log_level")) log_level = j["log_level"];
        }
        
        // 转换为JSON
        json toJson() const {
            return {
                {"save_video", save_video},
                {"video_path", video_path},
                {"save_results", save_results},
                {"results_path", results_path},
                {"draw_bboxes", draw_bboxes},
                {"draw_trails", draw_trails},
                {"draw_labels", draw_labels},
                {"log_to_file", log_to_file},
                {"log_path", log_path},
                {"log_level", log_level}
            };
        }
    } output;
    
    // 摄像头参数
    CameraParams camera;
    
    // 车辆参数
    VehicleParams vehicle;
    
    // 从JSON文件加载配置
    bool loadFromFile(const std::string& path) {
        try {
            std::ifstream ifs(path);
            if (!ifs.is_open()) {
                return false;
            }
            
            json j;
            ifs >> j;
            
            if (j.contains("video")) video.fromJson(j["video"]);
            if (j.contains("detector")) detector.fromJson(j["detector"]);
            if (j.contains("tracker")) tracker.fromJson(j["tracker"]);
            if (j.contains("behavior")) behavior.fromJson(j["behavior"]);
            if (j.contains("llm")) llm.fromJson(j["llm"]);
            if (j.contains("output")) output.fromJson(j["output"]);
            if (j.contains("camera")) camera.fromJson(j["camera"]);
            if (j.contains("vehicle")) vehicle.fromJson(j["vehicle"]);
            
            return true;
        } catch (...) {
            return false;
        }
    }
    
    // 保存配置到JSON文件
    bool saveToFile(const std::string& path) const {
        try {
            // 创建目录
            auto parent_path = std::filesystem::path(path).parent_path();
            if (!parent_path.empty() && !std::filesystem::exists(parent_path)) {
                std::filesystem::create_directories(parent_path);
            }
            
            std::ofstream ofs(path);
            if (!ofs.is_open()) {
                return false;
            }
            
            json j;
            j["video"] = video.toJson();
            j["detector"] = detector.toJson();
            j["tracker"] = tracker.toJson();
            j["behavior"] = behavior.toJson();
            j["llm"] = llm.toJson();
            j["output"] = output.toJson();
            j["camera"] = camera.toJson();
            j["vehicle"] = vehicle.toJson();
            
            ofs << std::setw(4) << j << std::endl;
            return true;
        } catch (...) {
            return false;
        }
    }
};

#endif // CONFIG_HPP
