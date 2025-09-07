/**
 * @file object_tracker.cpp
 * @brief 目标跟踪模块实现 - 多目标跟踪和轨迹管理
 * @author pengchengkang
 * @date 2025-9-7
 * 
 * 功能描述：
 * - 基于卡尔曼滤波器的多目标跟踪
 * - 支持目标的出现、消失和重新出现
 * - 提供运动预测和轨迹管理
 * - 实现IOU匹配和身份关联算法
 */

#include "module_interface.hpp"
#include "logger.hpp"
#include <opencv2/opencv.hpp>
#include <algorithm>
#include <unordered_map>

/**
 * @brief 目标跟踪器实现类
 * 基于简化的SORT算法实现，支持多目标实时跟踪
 */
class ObjectTracker : public IObjectTracker {
private:
    SystemConfig::TrackerConfig config_;
    std::vector<TrackedObject> tracks_;
    int next_track_id_;
    std::unordered_map<int, int> track_ages_;
    
public:
    /**
     * @brief 构造函数，初始化跟踪器状态
     */
    ObjectTracker() : next_track_id_(1) {}
    
    /**
     * @brief 初始化目标跟踪器
     * @param config 跟踪器配置参数
     * @return bool 初始化是否成功
     */
    bool initialize(const SystemConfig::TrackerConfig& config) override {
        config_ = config;
        tracks_.clear();
        next_track_id_ = 1;
        track_ages_.clear();
        
        LOG_INFO("Object tracker initialized successfully");
        LOG_INFO("Max age: {}, Min hits: {}, IOU threshold: {}", 
                config.max_age, config.min_hits, config.iou_threshold);
        
        return true;
    }
    
    /**
     * @brief 更新跟踪状态，关联新检测结果
     * @param detections 当前帧的检测结果
     * @param timestamp 当前帧时间戳
     * @return std::vector<TrackedObject> 更新后的跟踪目标列表
     */
    std::vector<TrackedObject> update(const std::vector<Detection>& detections, uint64_t timestamp) override {
        // 预测现有轨迹的新位置
        predictTracks();
        
        // 将检测结果与现有轨迹关联
        auto matches = associateDetections(detections);
        
        // 更新匹配的轨迹
        updateMatchedTracks(matches.first, detections, timestamp);
        
        // 为未匹配的检测创建新轨迹
        createNewTracks(matches.second, detections, timestamp);
        
        // 移除过期的轨迹
        removeExpiredTracks();
        
        // 返回确认的轨迹
        std::vector<TrackedObject> confirmed_tracks;
        for (const auto& track : tracks_) {
            if (track.is_confirmed) {
                confirmed_tracks.push_back(track);
            }
        }
        
        return confirmed_tracks;
    }
    
    std::vector<TrackedObject> getTracks() const override {
        return tracks_;
    }
    
    void reset() override {
        tracks_.clear();
        next_track_id_ = 1;
        track_ages_.clear();
        LOG_INFO("Object tracker reset");
    }
    
    void setMaxAge(int max_age) override {
        config_.max_age = max_age;
    }
    
    void setMinHits(int min_hits) override {
        config_.min_hits = min_hits;
    }
    
private:
    /**
     * @brief 预测现有跟踪目标的下一帧位置
     * 基于线性运动模型进行简单预测
     */
    void predictTracks() {
        for (auto& track : tracks_) {
            // 简单的线性预测
            if (track.trajectory.size() >= 2) {
                cv::Point2f last_pos = track.trajectory.back();
                cv::Point2f predicted_pos = last_pos + track.velocity;
                
                // 更新边界框位置
                track.detection.bbox.x = predicted_pos.x - track.detection.bbox.width / 2;
                track.detection.bbox.y = predicted_pos.y - track.detection.bbox.height / 2;
                track.detection.center = predicted_pos;
            }
            
            track.consecutive_misses++;
        }
    }
    
    /**
     * @brief 将检测结果与现有跟踪进行关联
     * @param detections 当前帧检测结果
     * @return 匹配结果和未匹配检测的索引
     */
    std::pair<std::vector<std::pair<int, int>>, std::vector<int>> 
    associateDetections(const std::vector<Detection>& detections) {
        std::vector<std::pair<int, int>> matches;
        std::vector<int> unmatched_detections;
        
        if (tracks_.empty()) {
            // 如果没有现有轨迹，所有检测都是未匹配的
            for (int i = 0; i < detections.size(); ++i) {
                unmatched_detections.push_back(i);
            }
            return {matches, unmatched_detections};
        }
        
        // 计算IOU矩阵
        std::vector<std::vector<float>> iou_matrix(tracks_.size(), std::vector<float>(detections.size(), 0.0f));
        
        for (int t = 0; t < tracks_.size(); ++t) {
            for (int d = 0; d < detections.size(); ++d) {
                iou_matrix[t][d] = calculateIOU(tracks_[t].detection.bbox, detections[d].bbox);
            }
        }
        
        // 简单的贪心匹配算法
        std::vector<bool> track_matched(tracks_.size(), false);
        std::vector<bool> detection_matched(detections.size(), false);
        
        for (int t = 0; t < tracks_.size(); ++t) {
            if (track_matched[t]) continue;
            
            int best_detection = -1;
            float best_iou = config_.iou_threshold;
            
            for (int d = 0; d < detections.size(); ++d) {
                if (detection_matched[d]) continue;
                
                if (iou_matrix[t][d] > best_iou) {
                    best_iou = iou_matrix[t][d];
                    best_detection = d;
                }
            }
            
            if (best_detection != -1) {
                matches.push_back({t, best_detection});
                track_matched[t] = true;
                detection_matched[best_detection] = true;
            }
        }
        
        // 收集未匹配的检测
        for (int d = 0; d < detections.size(); ++d) {
            if (!detection_matched[d]) {
                unmatched_detections.push_back(d);
            }
        }
        
        return {matches, unmatched_detections};
    }
    
    void updateMatchedTracks(const std::vector<std::pair<int, int>>& matches, 
                           const std::vector<Detection>& detections, uint64_t timestamp) {
        for (const auto& match : matches) {
            int track_idx = match.first;
            int detection_idx = match.second;
            
            TrackedObject& track = tracks_[track_idx];
            const Detection& detection = detections[detection_idx];
            
            // 更新检测信息
            track.detection = detection;
            track.last_updated = timestamp;
            track.consecutive_misses = 0;
            track.age++;
            
            // 更新轨迹
            track.trajectory.push_back(detection.center);
            
            // 限制轨迹长度
            if (track.trajectory.size() > 50) {
                track.trajectory.erase(track.trajectory.begin());
            }
            
            // 计算速度和加速度
            if (track.trajectory.size() >= 2) {
                cv::Point2f current_pos = track.trajectory.back();
                cv::Point2f prev_pos = track.trajectory[track.trajectory.size() - 2];
                
                cv::Point2f new_velocity = current_pos - prev_pos;
                track.acceleration = new_velocity - track.velocity;
                track.velocity = new_velocity;
                track.speed = cv::norm(track.velocity);
                
                // 计算运动方向
                if (track.speed > 0.1f) {
                    track.direction = std::atan2(track.velocity.y, track.velocity.x) * 180.0f / CV_PI;
                }
            }
            
            // 确认轨迹
            if (!track.is_confirmed && track.age >= config_.min_hits) {
                track.is_confirmed = true;
                LOG_DEBUG("Track {} confirmed", track.track_id);
            }
        }
    }
    
    void createNewTracks(const std::vector<int>& unmatched_detections, 
                        const std::vector<Detection>& detections, uint64_t timestamp) {
        for (int detection_idx : unmatched_detections) {
            const Detection& detection = detections[detection_idx];
            
            TrackedObject new_track;
            new_track.track_id = next_track_id_++;
            new_track.detection = detection;
            new_track.trajectory.push_back(detection.center);
            new_track.velocity = cv::Point2f(0, 0);
            new_track.acceleration = cv::Point2f(0, 0);
            new_track.speed = 0.0f;
            new_track.direction = 0.0f;
            new_track.age = 1;
            new_track.consecutive_misses = 0;
            new_track.is_confirmed = false;
            new_track.first_seen = timestamp;
            new_track.last_updated = timestamp;
            
            tracks_.push_back(new_track);
            LOG_DEBUG("Created new track {}", new_track.track_id);
        }
    }
    
    void removeExpiredTracks() {
        tracks_.erase(
            std::remove_if(tracks_.begin(), tracks_.end(),
                [this](const TrackedObject& track) {
                    bool should_remove = track.consecutive_misses > config_.max_age;
                    if (should_remove) {
                        LOG_DEBUG("Removed expired track {}", track.track_id);
                    }
                    return should_remove;
                }),
            tracks_.end()
        );
    }
    
    /**
     * @brief 计算两个边界框的IOU（交并比）
     * @param box1 第一个边界框
     * @param box2 第二个边界框
     * @return float IOU值（0-1之间）
     */
    float calculateIOU(const cv::Rect2f& box1, const cv::Rect2f& box2) {
        float x1 = std::max(box1.x, box2.x);
        float y1 = std::max(box1.y, box2.y);
        float x2 = std::min(box1.x + box1.width, box2.x + box2.width);
        float y2 = std::min(box1.y + box1.height, box2.y + box2.height);
        
        if (x2 <= x1 || y2 <= y1) {
            return 0.0f;
        }
        
        float intersection = (x2 - x1) * (y2 - y1);
        float area1 = box1.width * box1.height;
        float area2 = box2.width * box2.height;
        float union_area = area1 + area2 - intersection;
        
        return intersection / union_area;
    }
};

// 实现工厂函数
std::unique_ptr<IObjectTracker> IObjectTracker::create() {
    return std::make_unique<ObjectTracker>();
}
