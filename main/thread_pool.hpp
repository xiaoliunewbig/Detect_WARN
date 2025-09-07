/**
 * @file thread_pool.hpp
 * @brief 线程池实现 - 提供高效的多线程任务调度
 * @author pengchengkang
 * @date 2025-9-7
 * 
 * 功能描述：
 * - 支持动态任务提交和异步执行
 * - 自动管理工作线程生命周期
 * - 提供任务返回值获取机制
 * - 线程安全的任务队列管理
 */

#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <atomic>

/**
 * @brief 线程池类 - 管理多个工作线程执行异步任务
 * 使用生产者-消费者模式，支持任务的异步提交和执行
 */
class ThreadPool {
public:
    /**
     * @brief 构造函数，创建指定数量的工作线程
     * @param num_threads 工作线程数量，默认为CPU核心数
     */
    ThreadPool(size_t num_threads = std::thread::hardware_concurrency()) 
        : stop_(false) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] {
                for (;;) {
                    std::function<void()> task;
                    
                    {
                        std::unique_lock<std::mutex> lock(queue_mutex_);
                        condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                        
                        if (stop_ && tasks_.empty()) {
                            return;
                        }
                        
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    
                    task();
                }
            });
        }
    }
    
    /**
     * @brief 提交任务到线程池
     * @tparam F 函数类型
     * @tparam Args 参数类型
     * @param f 要执行的函数
     * @param args 函数参数
     * @return std::future 用于获取任务执行结果
     */
    template<class F, class... Args>
    auto submit(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type> {
        using return_type = typename std::result_of<F(Args...)>::type;
        
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        std::future<return_type> res = task->get_future();
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            
            if (stop_) {
                throw std::runtime_error("submit on stopped ThreadPool");
            }
            
            tasks_.emplace([task](){ (*task)(); });
        }
        
        condition_.notify_one();
        return res;
    }
    
    /**
     * @brief 停止线程池，等待所有任务完成
     */
    void stop() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            stop_ = true;
        }
        
        condition_.notify_all();
        
        for (std::thread &worker: workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        
        workers_.clear();
    }
    
    /**
     * @brief 重新启动线程池
     */
    void start() {
        if (stop_) {
            stop_ = false;
            // 重新创建工作线程
            size_t num_threads = std::thread::hardware_concurrency();
            for (size_t i = 0; i < num_threads; ++i) {
                workers_.emplace_back([this] {
                    for (;;) {
                        std::function<void()> task;
                        
                        {
                            std::unique_lock<std::mutex> lock(queue_mutex_);
                            condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                            
                            if (stop_ && tasks_.empty()) {
                                return;
                            }
                            
                            task = std::move(tasks_.front());
                            tasks_.pop();
                        }
                        
                        task();
                    }
                });
            }
        }
    }
    
    /**
     * @brief 获取工作线程数量
     * @return size_t 线程数量
     */
    size_t size() const {
        return workers_.size();
    }
    
    /**
     * @brief 析构函数，自动停止所有线程
     */
    ~ThreadPool() {
        stop();
    }
    
private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_;
};

#endif // THREAD_POOL_HPP
