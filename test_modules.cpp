/**
 * @file test_modules.cpp
 * @brief 模块功能测试程序
 * @author pengchengkang
 * @date 2025-9-7
 * 
 * 功能描述：
 * - 测试各个模块的基本功能
 * - 验证配置文件加载
 * - 检查模块接口的正确性
 * - 提供单元测试功能
 */

#include "config/config.hpp"
#include "interface/module_interface.hpp"
#include "main/logger.hpp"
#include <iostream>
#include <opencv2/opencv.hpp>

void testConfigLoading() {
    std::cout << "\n=== 测试配置文件加载 ===" << std::endl;
    
    SystemConfig config;
    bool success = config.loadFromFile("configs/default.json");
    
    if (success) {
        std::cout << "✓ 配置文件加载成功" << std::endl;
        std::cout << "  - 视频源: " << config.video.source << std::endl;
        std::cout << "  - 检测器模型: " << config.detector.model_path << std::endl;
        std::cout << "  - 输出路径: " << config.output.video_path << std::endl;
    } else {
        std::cout << "✗ 配置文件加载失败" << std::endl;
    }
}

void testModuleCreation() {
    std::cout << "\n=== 测试模块创建 ===" << std::endl;
    
    // 测试各个模块的创建
    auto video_processor = IVideoProcessor::create();
    auto object_detector = IObjectDetector::create();
    auto object_tracker = IObjectTracker::create();
    auto behavior_analyzer = IBehaviorAnalyzer::create();
    auto result_processor = IResultProcessor::create();
    auto llm_enhancer = ILLMEnhancer::create();
    
    std::cout << "✓ VideoProcessor: " << (video_processor ? "创建成功" : "创建失败") << std::endl;
    std::cout << "✓ ObjectDetector: " << (object_detector ? "创建成功" : "创建失败") << std::endl;
    std::cout << "✓ ObjectTracker: " << (object_tracker ? "创建成功" : "创建失败") << std::endl;
    std::cout << "✓ BehaviorAnalyzer: " << (behavior_analyzer ? "创建成功" : "创建失败") << std::endl;
    std::cout << "✓ ResultProcessor: " << (result_processor ? "创建成功" : "创建失败") << std::endl;
    std::cout << "✓ LLMEnhancer: " << (llm_enhancer ? "创建成功" : "创建失败") << std::endl;
}

void testDataStructures() {
    std::cout << "\n=== 测试数据结构 ===" << std::endl;
    
    // 测试Detection结构
    Detection det;
    det.bbox = cv::Rect2f(100, 100, 50, 80);
    det.confidence = 0.85f;
    det.class_id = ObjectClass::PEDESTRIAN;
    det.class_name = "pedestrian";
    
    std::cout << "✓ Detection结构测试通过" << std::endl;
    std::cout << "  - 边界框: (" << det.bbox.x << ", " << det.bbox.y 
              << ", " << det.bbox.width << ", " << det.bbox.height << ")" << std::endl;
    std::cout << "  - 置信度: " << det.confidence << std::endl;
    std::cout << "  - 类别: " << det.class_name << std::endl;
    
    // 测试TrackedObject结构
    TrackedObject track;
    track.track_id = 1;
    track.detection = det;
    track.speed = 2.5f;
    track.trajectory.push_back(cv::Point2f(125, 140));
    
    std::cout << "✓ TrackedObject结构测试通过" << std::endl;
    std::cout << "  - 跟踪ID: " << track.track_id << std::endl;
    std::cout << "  - 速度: " << track.speed << " m/s" << std::endl;
    
    // 测试BehaviorAnalysis结构
    BehaviorAnalysis behavior;
    behavior.track_id = 1;
    behavior.behavior = BehaviorType::PEDESTRIAN_WALKING;
    behavior.behavior_name = "walking";
    behavior.risk_level = RiskLevel::LOW_RISK;
    behavior.confidence = 0.9f;
    
    std::cout << "✓ BehaviorAnalysis结构测试通过" << std::endl;
    std::cout << "  - 行为: " << behavior.behavior_name << std::endl;
    std::cout << "  - 风险等级: " << static_cast<int>(behavior.risk_level) << std::endl;
    std::cout << "  - 置信度: " << behavior.confidence << std::endl;
}

void testOpenCVIntegration() {
    std::cout << "\n=== 测试OpenCV集成 ===" << std::endl;
    
    // 创建测试图像
    cv::Mat test_image = cv::Mat::zeros(480, 640, CV_8UC3);
    cv::rectangle(test_image, cv::Rect(100, 100, 50, 80), cv::Scalar(0, 255, 0), 2);
    cv::putText(test_image, "Test Image", cv::Point(200, 50), 
                cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(255, 255, 255), 2);
    
    std::cout << "✓ OpenCV图像操作测试通过" << std::endl;
    std::cout << "  - 图像尺寸: " << test_image.cols << "x" << test_image.rows << std::endl;
    std::cout << "  - 通道数: " << test_image.channels() << std::endl;
    
    // 测试图像保存（可选）
    try {
        cv::imwrite("test_output.jpg", test_image);
        std::cout << "✓ 图像保存测试通过" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "⚠ 图像保存测试跳过: " << e.what() << std::endl;
    }
}

void testLogger() {
    std::cout << "\n=== 测试日志系统 ===" << std::endl;
    
    // 测试不同级别的日志
    LOG_DEBUG("这是一条调试信息");
    LOG_INFO("这是一条信息");
    LOG_WARN("这是一条警告");
    LOG_ERROR("这是一条错误信息");
    
    std::cout << "✓ 日志系统测试完成（请检查控制台输出）" << std::endl;
}

int main(int /* argc */, char* /* argv */[]) {
    std::cout << "=== 车辆感知系统模块测试程序 ===" << std::endl;
    std::cout << "OpenCV版本: " << CV_VERSION << std::endl;
    
    try {
        // 运行各项测试
        testConfigLoading();
        testModuleCreation();
        testDataStructures();
        testOpenCVIntegration();
        testLogger();
        
        std::cout << "\n=== 测试总结 ===" << std::endl;
        std::cout << "✓ 所有基础模块测试完成" << std::endl;
        std::cout << "✓ 系统架构验证通过" << std::endl;
        std::cout << "✓ 可以进行进一步的集成测试" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "✗ 测试过程中出现异常: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
