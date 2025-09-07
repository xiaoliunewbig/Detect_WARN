# Vehicle Perception System (Detect_WARN)

## 项目概述

车辆前方目标检测与行为分析系统（Detect_WARN）是一个基于C++17开发的高性能实时系统，专门用于检测车辆前方的行人、非机动车和动物，并进行行为分析和风险评估。

## 主要功能

### 核心模块
- **视频处理模块**：支持摄像头、视频文件和RTSP流输入
- **目标检测模块**：基于深度学习的目标检测（支持ONNX、TensorFlow、Darknet模型）
- **多目标跟踪模块**：基于IOU的实时目标跟踪
- **行为分析模块**：分析目标行为模式和风险等级评估
- **结果处理模块**：实时可视化和多格式输出
- **LLM增强模块**：可选的大语言模型行为分析增强

### 检测目标类型
- 行人（站立、行走、跑步、横穿等行为）
- 非机动车（自行车、电动车等）
- 动物（各种动物检测）

### 风险评估
- 实时碰撞时间计算（TTC）
- 5级风险等级评估（安全、低风险、中风险、高风险、危险）
- 基于距离、速度和行为的综合风险分析

## 技术栈

- **核心语言**：C++17
- **图像处理**：OpenCV 4.x
- **深度学习部署**：OpenCV DNN（支持TensorRT加速）
- **配置管理**：nlohmann/json
- **构建系统**：CMake 3.16+
- **并发处理**：std::thread, std::atomic
- **日志系统**：自定义多级日志系统

## 系统要求

### 最低要求
- Ubuntu 18.04+ 或其他Linux发行版
- GCC 7+ 或 Clang 6+
- OpenCV 4.0+
- CMake 3.16+
- 至少4GB RAM

### 推荐配置
- Ubuntu 20.04+
- GCC 9+ 或 Clang 10+
- OpenCV 4.6+
- 8GB+ RAM
- NVIDIA GPU（可选，用于加速）

## 安装依赖

### Ubuntu/Debian
```bash
# 更新包管理器
sudo apt update

# 安装基础依赖
sudo apt install -y build-essential cmake git

# 安装OpenCV
sudo apt install -y libopencv-dev libopencv-contrib-dev

# 安装nlohmann-json（可选，CMake会自动下载）
sudo apt install -y nlohmann-json3-dev
```

## 构建说明

### 1. 克隆项目
```bash
git clone <repository-url>
cd Detect_WARN
```

### 2. 创建构建目录
```bash
mkdir build
cd build
```

### 3. 配置和编译
```bash
# 配置项目
cmake ..

# 编译（使用所有CPU核心）
make -j$(nproc)

# 或者编译特定目标
make VehiclePerceptionSystem  # 主程序
make TestModules             # 测试程序
```

### 4. 验证构建
```bash
# 运行测试程序验证基本功能
./bin/TestModules

# 检查主程序
./bin/VehiclePerceptionSystem --help
```

## 配置文件

系统使用JSON格式的配置文件，默认配置文件位于 `configs/default.json`。

### 主要配置项

```json
{
  "video": {
    "source": "0",              // 视频源（摄像头ID、文件路径或RTSP URL）
    "width": 640,               // 视频宽度
    "height": 480,              // 视频高度
    "fps": 30.0                 // 帧率
  },
  "detector": {
    "model_path": "models/yolo.onnx",  // 模型文件路径
    "confidence_threshold": 0.5,        // 置信度阈值
    "nms_threshold": 0.4,              // NMS阈值
    "input_size": [640, 640]           // 输入尺寸
  },
  "tracker": {
    "max_age": 30,              // 最大跟踪帧数
    "min_hits": 3,              // 最小命中次数
    "iou_threshold": 0.3        // IOU阈值
  },
  "behavior": {
    "enable": true,             // 启用行为分析
    "high_risk_distance": 10.0, // 高风险距离（米）
    "collision_risk_ttc": 3.0   // 碰撞风险时间（秒）
  },
  "output": {
    "save_video": true,         // 保存视频
    "save_results": true,       // 保存结果
    "video_path": "output/",    // 视频输出路径
    "results_path": "results/"  // 结果输出路径
  }
}
```

## 运行说明

### 1. 基本运行
```bash
# 使用默认配置
./bin/VehiclePerceptionSystem configs/default.json

# 使用自定义配置
./bin/VehiclePerceptionSystem path/to/your/config.json
```

### 2. 测试模式
```bash
# 运行模块测试
./bin/TestModules
```

### 3. 常见运行场景

#### 摄像头实时检测
```json
{
  "video": {
    "source": "0",  // 使用默认摄像头
    "width": 1280,
    "height": 720,
    "fps": 30.0
  }
}
```

#### 视频文件分析
```json
{
  "video": {
    "source": "path/to/video.mp4",
    "fps": 0  // 使用视频原始帧率
  }
}
```

#### RTSP流处理
```json
{
  "video": {
    "source": "rtsp://username:password@ip:port/stream"
  }
}
```

## 项目结构

```
Detect_WARN/
├── CMakeLists.txt          # 构建配置
├── README.md               # 项目说明
├── Design.md               # 设计文档
├── test_modules.cpp        # 测试程序
├── config/                 # 配置头文件
│   └── config.hpp
├── configs/                # 配置文件
│   └── default.json
├── data/                   # 数据结构定义
│   └── data_structs.hpp
├── interface/              # 模块接口定义
│   └── module_interface.hpp
├── main/                   # 主程序
│   ├── main.cpp
│   ├── logger.hpp
│   ├── thread_pool.hpp
│   └── global.hpp
└── vision/                 # 视觉处理模块
    ├── include/
    │   └── vehicle_perception_system.hpp
    └── src/
        ├── vehicle_perception_system.cpp
        ├── video_processor.cpp
        ├── object_detector.cpp
        ├── object_tracker.cpp
        ├── behavior_analyzer.cpp
        ├── result_processor.cpp
        └── llm_enhancer.cpp
```

## 输出格式

### 1. 视频输出
- 带有检测框和跟踪轨迹的可视化视频
- 实时风险等级显示
- 性能统计信息叠加

### 2. JSON结果
```json
{
  "timestamp": 1694123456789,
  "frame_id": 1234,
  "detections": [
    {
      "track_id": 1,
      "class_name": "pedestrian",
      "bbox": [100, 100, 50, 80],
      "confidence": 0.85,
      "behavior": "walking",
      "risk_level": "low_risk",
      "distance": 15.2,
      "ttc": 8.5
    }
  ],
  "performance": {
    "fps": 28.5,
    "detection_time": 35.2,
    "tracking_time": 12.1,
    "total_time": 52.3
  }
}
```

### 3. 日志输出
- 多级日志系统（DEBUG、INFO、WARN、ERROR）
- 控制台和文件双重输出
- 时间戳和模块标识

## 性能优化

### 1. 编译优化
```bash
# Release模式编译
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

### 2. 运行时优化
- 调整检测模型输入尺寸
- 优化置信度和NMS阈值
- 使用GPU加速（如果可用）
- 调整线程池大小

### 3. 内存优化
- 启用对象池
- 调整缓存大小
- 优化图像处理流水线

## 故障排除

### 常见问题

1. **编译错误：找不到OpenCV**
   ```bash
   # 检查OpenCV安装
   pkg-config --modversion opencv4
   
   # 设置环境变量
   export OpenCV_DIR=/usr/local/lib/cmake/opencv4
   ```

2. **运行时错误：无法打开摄像头**
   - 检查摄像头权限：`ls -l /dev/video*`
   - 测试摄像头：`v4l2-ctl --list-devices`
   - 尝试不同的设备ID：0, 1, 2...

3. **性能问题：帧率过低**
   - 降低输入分辨率
   - 调整检测间隔
   - 使用更快的模型
   - 启用GPU加速

4. **内存泄漏**
   - 使用valgrind检测：`valgrind --leak-check=full ./bin/VehiclePerceptionSystem config.json`
   - 检查智能指针使用
   - 确保资源正确释放

### 调试模式
```bash
# Debug模式编译
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)

# 使用GDB调试
gdb ./bin/VehiclePerceptionSystem
(gdb) run configs/default.json
```

## 扩展开发

### 添加新的检测模型
1. 实现`IObjectDetector`接口
2. 在工厂函数中注册新模型
3. 更新配置文件格式

### 添加新的行为分析
1. 扩展`BehaviorType`枚举
2. 在`BehaviorAnalyzer`中实现新的分析逻辑
3. 更新风险评估算法

### 集成新的LLM服务
1. 实现`ILLMEnhancer`接口
2. 添加API调用逻辑
3. 处理异步响应和错误

## 许可证

本项目采用MIT许可证，详见LICENSE文件。

## 贡献指南

1. Fork项目
2. 创建特性分支
3. 提交更改
4. 推送到分支
5. 创建Pull Request

## 联系方式

- 作者：pengchengkang
- 邮箱：[2801164069@qq.com]

## 更新日志

### v1.0.0 (2025-09-07)
- 初始版本发布
- 实现核心检测和跟踪功能
- 添加行为分析模块
- 完成基础测试框架_WARN