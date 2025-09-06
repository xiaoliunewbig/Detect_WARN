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
file(GLOB MAIN_SOURCES   ${PROJECT_SOURCE_DIR}/main/*.cpp)
file(GLOB VISION_SOURCES ${PROJECT_SOURCE_DIR}/vision/src/*.cpp)
set(SOURCES ${MAIN_SOURCES} ${VISION_SOURCES})

# ---------- 可执行目标 ----------
add_executable(${PROJECT_NAME} ${SOURCES})

# ---------- OpenCV 4 ----------
find_package(OpenCV 4 REQUIRED)
target_include_directories(${PROJECT_NAME} PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${OpenCV_LIBS})

# ---------- nlohmann/json ----------
find_package(nlohmann_json 3 REQUIRED)   # Ubuntu 20.04+ 自带 .cmake 配置
if(NOT nlohmann_json_FOUND)
    # 若系统没有，则自动拉取
    include(FetchContent)
    FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
    )
    FetchContent_MakeAvailable(json)
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)

# ---------- 你自己的头文件路径 ----------
target_include_directories(${PROJECT_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/config
    ${PROJECT_SOURCE_DIR}/data
    ${PROJECT_SOURCE_DIR}/interface
    ${PROJECT_SOURCE_DIR}/main
    ${PROJECT_SOURCE_DIR}/vision/include
)

# ---------- 安装 ----------
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# ---------- 打印信息 ----------
message(STATUS "=============================================")
message(STATUS "Project: ${PROJECT_NAME} ${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")
message(STATUS "=============================================")