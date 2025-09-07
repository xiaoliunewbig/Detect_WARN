/**
 * @file behavior_analyzer.cpp
 * @brief 行为分析模块实现 - 目标行为识别和风险评估
 * @author pengchengkang
 * @date 2025-9-7
 * 
 * 功能描述：
 * - 分析行人、非机动车和动物的行为模式
 * - 基于运动特征进行行为分类
 * - 实时风险等级评估和碰撞时间预测
 * - 支持多种危险行为的检测和预警
 */

#include "module_interface.hpp"
#include "logger.hpp"
#include <opencv2/opencv.hpp>
#include <cmath>

/**
 * @brief 行为分析器实现类
 * 基于轨迹分析和运动模式识别的行为分析系统
 */
class BehaviorAnalyzer : public IBehaviorAnalyzer {
private:
    SystemConfig::BehaviorConfig config_;
    CameraParams camera_params_;
    VehicleParams vehicle_params_;
    float vehicle_speed_kmh_;
    
public:
    /**
     * @brief 构造函数，初始化行为分析器
     */
    BehaviorAnalyzer() : vehicle_speed_kmh_(0.0f) {}
    
    /**
     * @brief 初始化行为分析器
     * @param config 行为分析配置参数
     * @param camera_params 相机标定参数
     * @param vehicle_params 车辆参数
     * @return bool 初始化是否成功
     */
    bool initialize(const SystemConfig::BehaviorConfig& config,
                   const CameraParams& camera_params,
                   const VehicleParams& vehicle_params) override {
        config_ = config;
        camera_params_ = camera_params;
        vehicle_params_ = vehicle_params;
        
        LOG_INFO("Behavior analyzer initialized successfully");
        LOG_INFO("High risk distance: {}m, Collision TTC: {}s", 
                config.high_risk_distance, config.collision_risk_ttc);
        
        return true;
    }
    
    /**
     * @brief 分析跟踪目标的行为模式
     * @param tracked_objects 跟踪目标列表
     * @return std::vector<BehaviorAnalysis> 行为分析结果列表
     */
    std::vector<BehaviorAnalysis> analyze(const std::vector<TrackedObject>& tracked_objects) override {
        std::vector<BehaviorAnalysis> results;
        results.reserve(tracked_objects.size());
        
        for (const auto& obj : tracked_objects) {
            BehaviorAnalysis analysis = analyzeObject(obj);
            results.push_back(analysis);
        }
        
        return results;
    }
    
    void setVehicleSpeed(float speed_kmh) override {
        vehicle_speed_kmh_ = speed_kmh;
    }
    
    float getVehicleSpeed() const override {
        return vehicle_speed_kmh_;
    }
    
private:
    /**
     * @brief 分析单个目标的行为
     * @param obj 跟踪目标对象
     * @return BehaviorAnalysis 行为分析结果
     */
    BehaviorAnalysis analyzeObject(const TrackedObject& obj) {
        BehaviorAnalysis analysis;
        analysis.track_id = obj.track_id;
        analysis.location = obj.detection.center;
        analysis.timestamp = obj.last_updated;
        
        // 估算距离（简化计算）
        analysis.distance_to_vehicle = estimateDistance(obj.detection.bbox);
        
        // 根据目标类别分析行为
        switch (obj.detection.class_id) {
            case ObjectClass::PEDESTRIAN:
                analysis = analyzePedestrianBehavior(obj, analysis);
                break;
            case ObjectClass::CYCLIST:
            case ObjectClass::MOTORCYCLIST:
            case ObjectClass::BICYCLE:
            case ObjectClass::MOTORCYCLE:
            case ObjectClass::TRICYCLE:
                analysis = analyzeNonMotorBehavior(obj, analysis);
                break;
            case ObjectClass::ANIMAL:
                analysis = analyzeAnimalBehavior(obj, analysis);
                break;
            default:
                analysis.behavior = BehaviorType::PEDESTRIAN_STANDING;
                analysis.behavior_name = "unknown";
                analysis.confidence = 0.5f;
                break;
        }
        
        // 评估风险等级
        analysis.risk_level = assessRiskLevel(obj, analysis);
        analysis.risk_description = getRiskDescription(analysis.risk_level);
        
        // 计算碰撞时间
        analysis.time_to_collision = calculateTimeToCollision(obj);
        
        return analysis;
    }
    
    /**
     * @brief 分析行人行为模式
     * @param obj 跟踪目标对象
     * @param analysis 初始分析结果
     * @return BehaviorAnalysis 更新后的分析结果
     */
    BehaviorAnalysis analyzePedestrianBehavior(const TrackedObject& obj, BehaviorAnalysis analysis) {
        float speed_ms = obj.speed; // 像素/帧转换为实际速度需要标定
        
        if (speed_ms < 0.5f) {
            analysis.behavior = BehaviorType::PEDESTRIAN_STANDING;
            analysis.behavior_name = "standing";
            analysis.confidence = 0.9f;
        } else if (speed_ms < config_.pedestrian_running_threshold) {
            analysis.behavior = BehaviorType::PEDESTRIAN_WALKING;
            analysis.behavior_name = "walking";
            analysis.confidence = 0.8f;
        } else {
            analysis.behavior = BehaviorType::PEDESTRIAN_RUNNING;
            analysis.behavior_name = "running";
            analysis.confidence = 0.8f;
        }
        
        // 检测横穿马路行为
        if (obj.trajectory.size() >= 3) {
            cv::Point2f movement = obj.trajectory.back() - obj.trajectory.front();
            float horizontal_movement = std::abs(movement.x);
            float vertical_movement = std::abs(movement.y);
            
            if (horizontal_movement > vertical_movement * 2 && horizontal_movement > 20) {
                analysis.behavior = BehaviorType::PEDESTRIAN_CROSSING;
                analysis.behavior_name = "crossing";
                analysis.confidence = 0.7f;
            }
        }
        
        return analysis;
    }
    
    BehaviorAnalysis analyzeNonMotorBehavior(const TrackedObject& obj, BehaviorAnalysis analysis) {
        float speed_ms = obj.speed;
        
        if (speed_ms < 0.5f) {
            analysis.behavior = BehaviorType::NON_MOTOR_STOPPED;
            analysis.behavior_name = "stopped";
            analysis.confidence = 0.9f;
        } else if (speed_ms < config_.non_motor_speeding_threshold) {
            analysis.behavior = BehaviorType::NON_MOTOR_MOVING;
            analysis.behavior_name = "moving";
            analysis.confidence = 0.8f;
        } else {
            analysis.behavior = BehaviorType::NON_MOTOR_SPEEDING;
            analysis.behavior_name = "speeding";
            analysis.confidence = 0.8f;
        }
        
        // 检测急刹车
        if (obj.trajectory.size() >= 3) {
            float deceleration = cv::norm(obj.acceleration);
            if (deceleration > 5.0f) {
                analysis.behavior = BehaviorType::NON_MOTOR_SUDEN_BRAKE;
                analysis.behavior_name = "sudden_brake";
                analysis.confidence = 0.7f;
            }
        }
        
        // 检测突然转向
        if (obj.trajectory.size() >= 5) {
            std::vector<float> directions;
            for (int i = 1; i < obj.trajectory.size(); ++i) {
                cv::Point2f movement = obj.trajectory[i] - obj.trajectory[i-1];
                float direction = std::atan2(movement.y, movement.x) * 180.0f / CV_PI;
                directions.push_back(direction);
            }
            
            if (directions.size() >= 3) {
                float direction_change = std::abs(directions.back() - directions[directions.size()-3]);
                if (direction_change > 45.0f) {
                    analysis.behavior = BehaviorType::NON_MOTOR_SUDEN_TURN;
                    analysis.behavior_name = "sudden_turn";
                    analysis.confidence = 0.6f;
                }
            }
        }
        
        return analysis;
    }
    
    BehaviorAnalysis analyzeAnimalBehavior(const TrackedObject& obj, BehaviorAnalysis analysis) {
        float speed_ms = obj.speed;
        
        if (speed_ms < 0.5f) {
            analysis.behavior = BehaviorType::ANIMAL_STATIONARY;
            analysis.behavior_name = "stationary";
            analysis.confidence = 0.9f;
        } else {
            analysis.behavior = BehaviorType::ANIMAL_MOVING;
            analysis.behavior_name = "moving";
            analysis.confidence = 0.8f;
            
            // 检测是否闯入道路
            if (analysis.distance_to_vehicle < config_.high_risk_distance) {
                analysis.behavior = BehaviorType::ANIMAL_ENTERING_ROAD;
                analysis.behavior_name = "entering_road";
                analysis.confidence = 0.7f;
            }
        }
        
        return analysis;
    }
    
    RiskLevel assessRiskLevel(const TrackedObject& obj, const BehaviorAnalysis& analysis) {
        float distance = analysis.distance_to_vehicle;
        float ttc = analysis.time_to_collision;
        
        // 基于距离的风险评估
        if (distance < 5.0f) {
            return RiskLevel::CRITICAL_RISK;
        } else if (distance < config_.high_risk_distance) {
            return RiskLevel::HIGH_RISK;
        } else if (distance < config_.high_risk_distance * 2) {
            return RiskLevel::MEDIUM_RISK;
        }
        
        // 基于碰撞时间的风险评估
        if (ttc > 0 && ttc < config_.collision_risk_ttc) {
            return RiskLevel::HIGH_RISK;
        } else if (ttc > 0 && ttc < config_.collision_risk_ttc * 2) {
            return RiskLevel::MEDIUM_RISK;
        }
        
        // 基于行为的风险评估
        switch (analysis.behavior) {
            case BehaviorType::PEDESTRIAN_RUNNING:
            case BehaviorType::PEDESTRIAN_CROSSING:
            case BehaviorType::NON_MOTOR_SPEEDING:
            case BehaviorType::NON_MOTOR_SUDEN_BRAKE:
            case BehaviorType::NON_MOTOR_SUDEN_TURN:
            case BehaviorType::ANIMAL_ENTERING_ROAD:
                return RiskLevel::MEDIUM_RISK;
            default:
                return RiskLevel::LOW_RISK;
        }
    }
    
    std::string getRiskDescription(RiskLevel risk) {
        switch (risk) {
            case RiskLevel::SAFE: return "Safe";
            case RiskLevel::LOW_RISK: return "Low risk";
            case RiskLevel::MEDIUM_RISK: return "Medium risk - attention required";
            case RiskLevel::HIGH_RISK: return "High risk - caution advised";
            case RiskLevel::CRITICAL_RISK: return "Critical risk - immediate action required";
            default: return "Unknown risk";
        }
    }
    
    /**
     * @brief 估算目标距离
     * @param bbox 目标边界框
     * @return float 估算距离（米）
     */
    float estimateDistance(const cv::Rect2f& bbox) {
        // 简化的距离估算，基于边界框大小
        // 实际应用中需要相机标定和更精确的计算
        float bbox_height = bbox.height;
        float estimated_distance = 1000.0f / (bbox_height + 1.0f); // 简单的反比例关系
        return std::max(1.0f, std::min(50.0f, estimated_distance));
    }
    
    /**
     * @brief 计算碰撞时间（TTC）
     * @param obj 跟踪目标对象
     * @return float 碰撞时间（秒），-1表示无碰撞风险
     */
    float calculateTimeToCollision(const TrackedObject& obj) {
        if (obj.speed <= 0.1f || vehicle_speed_kmh_ <= 0.1f) {
            return -1.0f; // 无碰撞风险
        }
        
        // 简化的TTC计算
        float distance = estimateDistance(obj.detection.bbox);
        float relative_speed = vehicle_speed_kmh_ / 3.6f; // 转换为m/s
        
        // 考虑目标的运动方向
        float approach_speed = relative_speed;
        if (obj.direction > -45 && obj.direction < 45) {
            // 目标向右移动，可能远离
            approach_speed -= obj.speed * 0.1f; // 简化转换
        } else if (obj.direction > 135 || obj.direction < -135) {
            // 目标向左移动，可能接近
            approach_speed += obj.speed * 0.1f;
        }
        
        if (approach_speed <= 0) {
            return -1.0f;
        }
        
        return distance / approach_speed;
    }
};

// 实现工厂函数
std::unique_ptr<IBehaviorAnalyzer> IBehaviorAnalyzer::create() {
    return std::make_unique<BehaviorAnalyzer>();
}
