/**
 * @file result_processor.cpp
 * @brief 结果处理模块实现 - 检测结果可视化和输出管理
 * @author pengchengkang
 * @date 2025-9-7
 * 
 * 功能描述：
 * - 在视频帧上绘制检测框、跟踪轨迹和行为分析结果
 * - 支持多种输出格式（视频、图像、JSON日志）
 * - 提供实时性能统计和结果保存功能
 * - 可配置的可视化样式和输出选项
 */

#include "module_interface.hpp"
#include "logger.hpp"
#include <opencv2/opencv.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iomanip>
#include <sstream>

/**
 * @brief 结果处理器实现类
 * 负责检测结果的可视化显示和多格式输出
 */
class ResultProcessor : public IResultProcessor {
private:
    SystemConfig::OutputConfig config_;
    cv::Mat processed_frame_;
    std::vector<BehaviorAnalysis> current_results_;
    cv::VideoWriter video_writer_;
    std::ofstream results_file_;
    std::string session_id_;
    int frame_count_;
    double total_processing_time_;
    
    // 颜色定义
    const cv::Scalar COLOR_SAFE = cv::Scalar(0, 255, 0);        // 绿色
    const cv::Scalar COLOR_LOW_RISK = cv::Scalar(0, 255, 255);  // 黄色
    const cv::Scalar COLOR_MEDIUM_RISK = cv::Scalar(0, 165, 255); // 橙色
    const cv::Scalar COLOR_HIGH_RISK = cv::Scalar(0, 0, 255);   // 红色
    const cv::Scalar COLOR_CRITICAL = cv::Scalar(255, 0, 255);  // 紫色
    
public:
    /**
     * @brief 构造函数，初始化结果处理器
     */
    ResultProcessor() : frame_count_(0), total_processing_time_(0.0) {
        // 生成会话ID
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);
        
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y%m%d_%H%M%S");
        session_id_ = oss.str();
    }
    
    ~ResultProcessor() {
        if (video_writer_.isOpened()) {
            video_writer_.release();
        }
        if (results_file_.is_open()) {
            results_file_.close();
        }
    }
    
    /**
     * @brief 初始化结果处理器
     * @param config 输出配置参数
     * @return bool 初始化是否成功
     */
    bool initialize(const SystemConfig::OutputConfig& config) override {
        config_ = config;
        
        // 创建输出目录
        if (config.save_video) {
            std::filesystem::create_directories(config.video_path);
        }
        
        if (config.save_results) {
            std::filesystem::create_directories(config.results_path);
            
            // 打开结果文件
            std::string results_filename = config.results_path + "results_" + session_id_ + ".json";
            results_file_.open(results_filename);
            if (results_file_.is_open()) {
                results_file_ << "[\n";
            }
        }
        
        LOG_INFO("Result processor initialized successfully");
        LOG_INFO("Save video: {}, Save results: {}", config.save_video, config.save_results);
        
        return true;
    }
    
    /**
     * @brief 处理和可视化检测结果
     * @param frame 原始视频帧
     * @param results 检测结果列表
     * @param timestamp 时间戳
     */
    void process(const std::vector<BehaviorAnalysis>& results,
                const cv::Mat& frame, uint64_t timestamp) override {
        if (frame.empty()) return;
        
        current_results_ = results;
        processed_frame_ = frame.clone();
        
        // 绘制检测结果
        if (config_.draw_bboxes || config_.draw_labels || config_.draw_trails) {
            drawResults(processed_frame_, results);
        }
        
        // 保存视频帧
        if (config_.save_video) {
            saveVideoFrame(processed_frame_, timestamp);
        }
        
        // 保存分析结果
        if (config_.save_results) {
            saveResults(results, timestamp);
        }
    }
    
    cv::Mat getProcessedFrame() const override {
        return processed_frame_;
    }
    
    bool saveResults(const std::string& path) const override {
        try {
            std::ofstream file(path);
            if (!file.is_open()) {
                return false;
            }
            
            file << "[\n";
            for (size_t i = 0; i < current_results_.size(); ++i) {
                file << current_results_[i].toJson().dump(2);
                if (i < current_results_.size() - 1) {
                    file << ",";
                }
                file << "\n";
            }
            file << "]\n";
            
            return true;
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to save results: {}", e.what());
            return false;
        }
    }
    
private:
    void drawResults(cv::Mat& frame, const std::vector<BehaviorAnalysis>& results) {
        for (const auto& result : results) {
            cv::Scalar color = getRiskColor(result.risk_level);
            
            // 绘制边界框（需要从跟踪结果中获取）
            if (config_.draw_bboxes) {
                // 这里简化处理，实际需要从跟踪器获取边界框信息
                cv::Point2f center = result.location;
                cv::Rect bbox(center.x - 50, center.y - 50, 100, 100);
                cv::rectangle(frame, bbox, color, 2);
            }
            
            // 绘制标签
            if (config_.draw_labels) {
                std::string label = result.behavior_name + " (" + 
                                  std::to_string(static_cast<int>(result.confidence * 100)) + "%)";
                
                cv::Point2f label_pos(result.location.x, result.location.y - 10);
                
                // 绘制背景
                int baseline = 0;
                cv::Size text_size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);
                cv::rectangle(frame, 
                            cv::Point(label_pos.x, label_pos.y - text_size.height - baseline),
                            cv::Point(label_pos.x + text_size.width, label_pos.y + baseline),
                            color, -1);
                
                // 绘制文字
                cv::putText(frame, label, label_pos, cv::FONT_HERSHEY_SIMPLEX, 0.5, 
                           cv::Scalar(255, 255, 255), 1);
                
                // 绘制风险描述
                if (!result.risk_description.empty()) {
                    cv::Point2f risk_pos(result.location.x, result.location.y + 20);
                    cv::putText(frame, result.risk_description, risk_pos, 
                               cv::FONT_HERSHEY_SIMPLEX, 0.4, color, 1);
                }
                
                // 绘制距离和TTC信息
                if (result.distance_to_vehicle > 0) {
                    std::string distance_text = "Dist: " + 
                        std::to_string(static_cast<int>(result.distance_to_vehicle)) + "m";
                    cv::Point2f dist_pos(result.location.x, result.location.y + 35);
                    cv::putText(frame, distance_text, dist_pos, 
                               cv::FONT_HERSHEY_SIMPLEX, 0.4, color, 1);
                }
                
                if (result.time_to_collision > 0) {
                    std::string ttc_text = "TTC: " + 
                        std::to_string(static_cast<int>(result.time_to_collision)) + "s";
                    cv::Point2f ttc_pos(result.location.x, result.location.y + 50);
                    cv::putText(frame, ttc_text, ttc_pos, 
                               cv::FONT_HERSHEY_SIMPLEX, 0.4, color, 1);
                }
            }
        }
        
        // 绘制统计信息
        drawStatistics(frame, results);
    }
    
    void drawStatistics(cv::Mat& frame, const std::vector<BehaviorAnalysis>& results) {
        // 统计各风险等级的目标数量
        std::map<RiskLevel, int> risk_counts;
        for (const auto& result : results) {
            risk_counts[result.risk_level]++;
        }
        
        // 绘制统计信息
        int y_offset = 30;
        cv::putText(frame, "Detection Statistics:", cv::Point(10, y_offset), 
                   cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 2);
        
        y_offset += 25;
        for (const auto& pair : risk_counts) {
            if (pair.second > 0) {
                std::string text = getRiskLevelName(pair.first) + ": " + std::to_string(pair.second);
                cv::Scalar color = getRiskColor(pair.first);
                cv::putText(frame, text, cv::Point(10, y_offset), 
                           cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 1);
                y_offset += 20;
            }
        }
    }
    
    cv::Scalar getRiskColor(RiskLevel risk) {
        switch (risk) {
            case RiskLevel::SAFE: return COLOR_SAFE;
            case RiskLevel::LOW_RISK: return COLOR_LOW_RISK;
            case RiskLevel::MEDIUM_RISK: return COLOR_MEDIUM_RISK;
            case RiskLevel::HIGH_RISK: return COLOR_HIGH_RISK;
            case RiskLevel::CRITICAL_RISK: return COLOR_CRITICAL;
            default: return cv::Scalar(128, 128, 128);
        }
    }
    
    std::string getRiskLevelName(RiskLevel risk) {
        switch (risk) {
            case RiskLevel::SAFE: return "Safe";
            case RiskLevel::LOW_RISK: return "Low Risk";
            case RiskLevel::MEDIUM_RISK: return "Medium Risk";
            case RiskLevel::HIGH_RISK: return "High Risk";
            case RiskLevel::CRITICAL_RISK: return "Critical";
            default: return "Unknown";
        }
    }
    
    void saveVideoFrame(const cv::Mat& frame, uint64_t timestamp) {
        if (!video_writer_.isOpened()) {
            // 初始化视频写入器
            std::string video_filename = config_.video_path + "output_" + session_id_ + ".mp4";
            int fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
            video_writer_.open(video_filename, fourcc, 30.0, frame.size());
            
            if (!video_writer_.isOpened()) {
                LOG_ERROR("Failed to open video writer: {}", video_filename);
                return;
            }
        }
        
        video_writer_.write(frame);
    }
    
    void saveResults(const std::vector<BehaviorAnalysis>& results, uint64_t timestamp) {
        if (!results_file_.is_open()) return;
        
        nlohmann::json frame_data;
        frame_data["timestamp"] = timestamp;
        frame_data["results"] = nlohmann::json::array();
        
        for (const auto& result : results) {
            frame_data["results"].push_back(result.toJson());
        }
        
        static bool first_frame = true;
        if (!first_frame) {
            results_file_ << ",\n";
        }
        first_frame = false;
        
        results_file_ << frame_data.dump(2);
        results_file_.flush();
    }
};

// 实现工厂函数
std::unique_ptr<IResultProcessor> IResultProcessor::create() {
    return std::make_unique<ResultProcessor>();
}
