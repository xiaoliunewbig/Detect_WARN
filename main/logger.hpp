/**
 * @file logger.hpp
 * @brief 日志系统实现 - 提供多级别日志记录功能
 * @author pengchengkang
 * @date 2025-9-7
 * 
 * 功能描述：
 * - 支持多级别日志输出（TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL）
 * - 同时支持控制台和文件输出
 * - 线程安全的日志记录
 * - 自动时间戳和日志轮转
 */

#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <filesystem>

/**
 * @brief 日志级别枚举
 * 定义了从低到高的日志级别，用于控制日志输出的详细程度
 */
enum class LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    CRITICAL = 5
};

/**
 * @brief 单例模式的日志管理器
 * 提供全局统一的日志记录接口，支持多线程安全访问
 */
class Logger {
public:
    /**
     * @brief 获取日志器单例实例
     * @return Logger& 日志器实例引用
     */
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }
    
    /**
     * @brief 初始化日志系统
     * @param log_path 日志文件保存路径
     * @param level 最低日志级别
     * @param log_to_file 是否输出到文件
     */
    static void initialize(const std::string& log_path = "logs/", 
                          LogLevel level = LogLevel::INFO,
                          bool log_to_file = true) {
        auto& logger = getInstance();
        logger.log_level_ = level;
        logger.log_to_file_ = log_to_file;
        
        if (log_to_file) {
            // 创建日志目录
            std::filesystem::create_directories(log_path);
            
            // 生成日志文件名（包含时间戳）
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            auto tm = *std::localtime(&time_t);
            
            std::ostringstream oss;
            oss << log_path << "vehicle_perception_" 
                << std::put_time(&tm, "%Y%m%d_%H%M%S") << ".log";
            
            logger.log_file_.open(oss.str(), std::ios::app);
        }
    }
    
    /**
     * @brief 记录日志消息
     * @tparam Args 可变参数类型
     * @param level 日志级别
     * @param format 格式化字符串
     * @param args 格式化参数
     */
    template<typename... Args>
    void log(LogLevel level, const std::string& format, Args&&... args) {
        if (level < log_level_) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        // 格式化消息
        std::string message = formatString(format, std::forward<Args>(args)...);
        
        // 获取时间戳
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);
        
        // 构建完整日志消息
        std::ostringstream oss;
        oss << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "] "
            << "[" << levelToString(level) << "] " << message;
        
        std::string full_message = oss.str();
        
        // 输出到控制台
        std::cout << full_message << std::endl;
        
        // 输出到文件
        if (log_to_file_ && log_file_.is_open()) {
            log_file_ << full_message << std::endl;
            log_file_.flush();
        }
    }
    
private:
    Logger() = default;
    ~Logger() {
        if (log_file_.is_open()) {
            log_file_.close();
        }
    }
    
    template<typename... Args>
    std::string formatString(const std::string& format, Args&&... args) {
        if constexpr (sizeof...(args) == 0) {
            return format;
        } else {
            // 简单的字符串格式化实现
            std::ostringstream oss;
            formatImpl(oss, format, std::forward<Args>(args)...);
            return oss.str();
        }
    }
    
    template<typename T, typename... Args>
    void formatImpl(std::ostringstream& oss, const std::string& format, T&& value, Args&&... args) {
        size_t pos = format.find("{}");
        if (pos != std::string::npos) {
            oss << format.substr(0, pos) << std::forward<T>(value);
            std::string remaining = format.substr(pos + 2);
            if constexpr (sizeof...(args) > 0) {
                formatImpl(oss, remaining, std::forward<Args>(args)...);
            } else {
                oss << remaining;
            }
        } else {
            oss << format;
        }
    }
    
    std::string levelToString(LogLevel level) {
        switch (level) {
            case LogLevel::TRACE: return "TRACE";
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO: return "INFO";
            case LogLevel::WARN: return "WARN";
            case LogLevel::ERROR: return "ERROR";
            case LogLevel::CRITICAL: return "CRITICAL";
            default: return "UNKNOWN";
        }
    }
    
    LogLevel log_level_ = LogLevel::INFO;
    bool log_to_file_ = true;
    std::ofstream log_file_;
    std::mutex mutex_;
};

// 便捷宏定义
#define LOG_TRACE(...) Logger::getInstance().log(LogLevel::TRACE, __VA_ARGS__)
#define LOG_DEBUG(...) Logger::getInstance().log(LogLevel::DEBUG, __VA_ARGS__)
#define LOG_INFO(...) Logger::getInstance().log(LogLevel::INFO, __VA_ARGS__)
#define LOG_WARN(...) Logger::getInstance().log(LogLevel::WARN, __VA_ARGS__)
#define LOG_ERROR(...) Logger::getInstance().log(LogLevel::ERROR, __VA_ARGS__)
#define LOG_CRITICAL(...) Logger::getInstance().log(LogLevel::CRITICAL, __VA_ARGS__)

#endif // LOGGER_HPP
