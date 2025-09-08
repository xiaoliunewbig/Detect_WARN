/**
 * @file data_structs.hpp
 * @brief 定义智能驾驶系统中目标检测、跟踪和行为分析相关的数据结构
 * @author pengchengkang
 * @date 2025-9-6
 * 
 * -功能描述:
 * 定义了智能驾驶系统中用于目标检测、跟踪和行为分析的核心数据结构，包括：
 * 1. 目标类别枚举(ObjectClass)：定义行人、非机动车、动物等目标类型
 * 2. 行为类型枚举(BehaviorType)：定义各类目标的行为模式
 * 3. 风险等级枚举(RiskLevel)：定义不同级别的风险程度
 * 4. 检测结果结构(Detection)：存储单帧目标检测结果
 * 5. 跟踪目标结构(TrackedObject)：存储目标跟踪状态和历史轨迹
 * 6. 行为分析结构(BehaviorAnalysis)：存储目标行为分析和风险评估结果
 * 7. 性能统计结构(DetectionPerformance)：统计检测系统性能指标
 * 所有结构均支持JSON序列化，便于数据传输和存储
 */
#ifndef DATA_STRUCTURES_HPP
#define DATA_STRUCTURES_HPP

#include <vector>
#include <string>
#include <opencv2/opencv.hpp>
#include <nlohmann/json.hpp>

// 使用nlohmann/json库进行JSON序列化
using json = nlohmann::json;

// 目标类别定义
enum class ObjectClass {
    UNKNOWN = 0,
    PEDESTRIAN = 1,      // 行人
    CYCLIST = 2,         // 骑自行车的人
    MOTORCYCLIST = 3,    // 骑摩托车的人
    BICYCLE = 4,         // 自行车
    MOTORCYCLE = 5,      // 摩托车
    TRICYCLE = 6,        // 三轮车
    ANIMAL = 7           // 动物
};

// 行为类型
enum class BehaviorType {
    // 行人行为
    PEDESTRIAN_STANDING,       // 站立
    PEDESTRIAN_WALKING,        // 行走
    PEDESTRIAN_RUNNING,        // 奔跑
    PEDESTRIAN_CROSSING,       // 横穿马路
    PEDESTRIAN_LOITERING,      // 徘徊
    
    // 非机动车行为
    NON_MOTOR_STOPPED,         // 停止
    NON_MOTOR_MOVING,          // 正常行驶
    NON_MOTOR_SPEEDING,        // 超速
    NON_MOTOR_SUDEN_BRAKE,     // 急刹车
    NON_MOTOR_SUDEN_TURN,      // 突然转向
    NON_MOTOR_REVERSING,       // 逆行
    
    // 动物行为
    ANIMAL_STATIONARY,         // 静止
    ANIMAL_MOVING,             // 移动
    ANIMAL_ENTERING_ROAD       // 闯入道路
};

// 风险等级
enum class RiskLevel {
    SAFE = 0,            // 安全
    LOW_RISK = 1,        // 低风险
    MEDIUM_RISK = 2,     // 中风险
    HIGH_RISK = 3,       // 高风险
    CRITICAL_RISK = 4    // 极高风险
};

// 目标检测结果
struct Detection {
    int id = -1;                      // 检测ID
    ObjectClass class_id = ObjectClass::UNKNOWN; // 类别ID
    std::string class_name;           // 类别名称
    float confidence = 0.0f;          // 置信度(0-1)
    cv::Rect2f bbox;                  // 边界框(x, y, width, height)
    cv::Point2f center;               // 中心点坐标
    float area = 0.0f;                // 面积
    float aspect_ratio = 0.0f;        // 宽高比
    uint64_t timestamp = 0;           // 时间戳(毫秒)
    
    // 序列化函数
    json toJson() const {
        return {
            {"id", id},
            {"class_id", static_cast<int>(class_id)},
            {"class_name", class_name},
            {"confidence", confidence},
            {"bbox", {bbox.x, bbox.y, bbox.width, bbox.height}},
            {"center", {center.x, center.y}},
            {"area", area},
            {"aspect_ratio", aspect_ratio},
            {"timestamp", timestamp}
        };
    }
};

// 跟踪目标
struct TrackedObject {
    int track_id = -1;                // 跟踪ID
    Detection detection;              // 最新检测结果
    std::vector<cv::Point2f> trajectory; // 轨迹历史
    cv::Point2f velocity;             // 速度矢量(像素/帧)
    float speed = 0.0f;               // 速度大小(像素/帧)
    cv::Point2f acceleration;         // 加速度矢量
    float direction = 0.0f;           // 运动方向(角度，度)
    int age = 0;                      // 跟踪年龄(帧)
    int consecutive_misses = 0;       // 连续未检测到的帧数
    bool is_confirmed = false;        // 是否确认跟踪
    uint64_t first_seen = 0;          // 首次出现时间戳
    uint64_t last_updated = 0;        // 最后更新时间戳
    
    // 序列化函数
    json toJson() const {
        json j = detection.toJson();
        j["track_id"] = track_id;
        j["velocity"] = {velocity.x, velocity.y};
        j["speed"] = speed;
        j["direction"] = direction;
        j["age"] = age;
        j["is_confirmed"] = is_confirmed;
        j["first_seen"] = first_seen;
        j["last_updated"] = last_updated;
        
        // 只序列化部分轨迹点以减少数据量
        json trajectory_json = json::array();
        size_t step = std::max(1, static_cast<int>(trajectory.size() / 10)); // 最多10个点
        for (size_t i = 0; i < trajectory.size(); i += step) {
            trajectory_json.push_back({trajectory[i].x, trajectory[i].y});
        }
        j["trajectory"] = trajectory_json;
        
        return j;
    }
};

// 行为分析结果
struct BehaviorAnalysis {
    int track_id = -1;                  // 跟踪ID
    BehaviorType behavior = BehaviorType::PEDESTRIAN_STANDING; // 行为类型
    std::string behavior_name;          // 行为名称
    float confidence = 0.0f;            // 行为置信度(0-1)
    RiskLevel risk_level = RiskLevel::SAFE; // 风险等级
    std::string risk_description;       // 风险描述
    cv::Point2f location;               // 位置
    float distance_to_vehicle = 0.0f;   // 与车辆距离(米)
    float time_to_collision = 0.0f;     // 碰撞时间预测(秒)
    uint64_t timestamp = 0;             // 时间戳(毫秒)
    std::string llm_analysis;           // 大模型分析结果
    
    // 序列化函数
    json toJson() const {
        return {
            {"track_id", track_id},
            {"behavior", static_cast<int>(behavior)},
            {"behavior_name", behavior_name},
            {"confidence", confidence},
            {"risk_level", static_cast<int>(risk_level)},
            {"risk_description", risk_description},
            {"location", {location.x, location.y}},
            {"distance_to_vehicle", distance_to_vehicle},
            {"time_to_collision", time_to_collision},
            {"timestamp", timestamp},
            {"llm_analysis", llm_analysis}
        };
    }
};

// 检测性能统计
struct DetectionPerformance {
    float preprocess_time_ms = 0.0f;    // 预处理时间(毫秒)
    float inference_time_ms = 0.0f;     // 推理时间(毫秒)
    float postprocess_time_ms = 0.0f;   // 后处理时间(毫秒)
    int frame_count = 0;                // 处理帧数
    float fps = 0.0f;                   // 帧率
};

#endif // DATA_STRUCTURES_HPP
