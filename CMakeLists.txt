# ============================================================================
# Vehicle Perception System - CMake Configuration
# Author: pengchengkang
# Date: 2025-9-7
# Description: 车辆前方目标检测与行为分析系统构建配置
# ============================================================================

cmake_minimum_required(VERSION 3.16)
project(VehiclePerceptionSystem VERSION 1.0.0 LANGUAGES CXX)

# ---------- C++ 标准 ----------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------- 编译选项 ----------
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_DEBUG "-g -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -Wall -Wextra -Wpedantic")

# ---------- 输出目录 ----------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ---------- 源文件 ----------
# 主程序源文件
set(MAIN_SOURCES
    ${PROJECT_SOURCE_DIR}/main/main.cpp
)

# 视觉模块源文件
set(VISION_SOURCES
    ${PROJECT_SOURCE_DIR}/vision/src/vehicle_perception_system.cpp
    ${PROJECT_SOURCE_DIR}/vision/src/video_processor.cpp
    ${PROJECT_SOURCE_DIR}/vision/src/object_detector.cpp
    ${PROJECT_SOURCE_DIR}/vision/src/object_tracker.cpp
    ${PROJECT_SOURCE_DIR}/vision/src/behavior_analyzer.cpp
    ${PROJECT_SOURCE_DIR}/vision/src/result_processor.cpp
    ${PROJECT_SOURCE_DIR}/vision/src/llm_enhancer.cpp
)

# 合并所有源文件
set(SOURCES ${MAIN_SOURCES} ${VISION_SOURCES})

# ---------- 可执行目标 ----------
add_executable(${PROJECT_NAME} ${SOURCES})

# ---------- 测试程序 ----------
add_executable(TestModules
    test_modules.cpp
    ${VISION_SOURCES}
)

# ---------- 添加视频重连测试程序 ----------
add_executable(TestVideoRetry
    test_video_retry.cpp
    vision/src/video_processor.cpp
)

# ---------- 查找依赖包 ----------
# OpenCV 4.x (必需)
find_package(OpenCV 4 REQUIRED)
if(OpenCV_FOUND)
    message(STATUS "Found OpenCV version: ${OpenCV_VERSION}")
else()
    message(FATAL_ERROR "OpenCV 4.x not found!")
endif()

# 线程库 (必需)
find_package(Threads REQUIRED)

# nlohmann/json (必需)
find_package(nlohmann_json 3 QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found, fetching from GitHub...")
    include(FetchContent)
    FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
    )
    FetchContent_MakeAvailable(json)
    set(JSON_TARGET nlohmann_json::nlohmann_json)
else()
    message(STATUS "Found nlohmann_json")
    set(JSON_TARGET nlohmann_json::nlohmann_json)
endif()

# ---------- 包含目录 ----------
target_include_directories(${PROJECT_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/config          # 配置头文件
    ${PROJECT_SOURCE_DIR}/configs         # 配置文件
    ${PROJECT_SOURCE_DIR}/data            # 数据结构定义
    ${PROJECT_SOURCE_DIR}/interface       # 模块接口定义
    ${PROJECT_SOURCE_DIR}/main            # 主程序头文件
    ${PROJECT_SOURCE_DIR}/vision/include  # 视觉模块头文件
    ${OpenCV_INCLUDE_DIRS}                # OpenCV头文件
)

# 设置TestModules的包含目录和链接库
target_include_directories(TestModules PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/main
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    ${CMAKE_CURRENT_SOURCE_DIR}/data
    ${CMAKE_CURRENT_SOURCE_DIR}/interface
    ${CMAKE_CURRENT_SOURCE_DIR}/vision/include
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(TestModules PRIVATE
    ${OpenCV_LIBS}
    nlohmann_json::nlohmann_json
    pthread
)

# 设置TestVideoRetry的包含目录和链接库
target_include_directories(TestVideoRetry PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/main
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    ${CMAKE_CURRENT_SOURCE_DIR}/data
    ${CMAKE_CURRENT_SOURCE_DIR}/interface
    ${CMAKE_CURRENT_SOURCE_DIR}/vision/include
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(TestVideoRetry PRIVATE
    ${OpenCV_LIBS}
    nlohmann_json::nlohmann_json
    pthread
)

# ---------- 链接库 ----------
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${OpenCV_LIBS}                        # OpenCV库
    ${JSON_TARGET}                        # JSON库
    Threads::Threads                      # 线程库
)

# 测试程序链接库
target_link_libraries(TestModules PRIVATE
    ${OpenCV_LIBS}
    ${JSON_TARGET}
    Threads::Threads
)

# ---------- 编译器特定设置 ----------
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
        -Wno-unused-variable
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
        -Wno-unused-variable
    )
endif()

# ---------- 安装规则 ----------
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# 安装配置文件
install(FILES ${PROJECT_SOURCE_DIR}/configs/default.json
    DESTINATION etc/${PROJECT_NAME}
)

# 安装文档
install(FILES ${PROJECT_SOURCE_DIR}/README.md ${PROJECT_SOURCE_DIR}/Design.md
    DESTINATION share/doc/${PROJECT_NAME}
)

# ---------- 构建信息打印 ----------
message(STATUS "=============================================")
message(STATUS "Project: ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "Source files count: ${CMAKE_CURRENT_LIST_DIR}")
list(LENGTH SOURCES SOURCE_COUNT)
message(STATUS "Total source files: ${SOURCE_COUNT}")
message(STATUS "Output directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "=============================================")