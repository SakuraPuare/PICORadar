#include <benchmark/benchmark.h>
#include "core/player_registry.hpp"
#include "common.pb.h"
#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <chrono>
#include <random>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <future>
#include <barrier>

using namespace picoradar::core;
using PlayerData = picoradar::PlayerData;

// 并发客户端测试基类
class ConcurrentBenchmarkBase : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) override {
        gen_.seed(42 + state.thread_index()); // 每个线程不同的种子
        setupTestData();
    }
    
    void TearDown(const ::benchmark::State& state) override {
        test_player_data_.clear();
    }

protected:
    std::mt19937 gen_;
    std::vector<PlayerData> test_player_data_;
    
    void setupTestData() {
        std::uniform_real_distribution<float> pos_dist(-100.0f, 100.0f);
        std::uniform_real_distribution<float> rot_dist(-1.0f, 1.0f);
        
        test_player_data_.reserve(1000);
        
        for (int i = 0; i < 1000; ++i) {
            PlayerData player;
            player.set_player_id("concurrent_player_" + std::to_string(i));
            player.set_timestamp(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::high_resolution_clock::now().time_since_epoch()
                ).count()
            );
            
            auto* position = player.mutable_position();
            position->set_x(pos_dist(gen_));
            position->set_y(pos_dist(gen_));
            position->set_z(pos_dist(gen_));
            
            auto* rotation = player.mutable_rotation();
            rotation->set_x(rot_dist(gen_));
            rotation->set_y(rot_dist(gen_));
            rotation->set_z(rot_dist(gen_));
            rotation->set_w(rot_dist(gen_));
            
            test_player_data_.push_back(std::move(player));
        }
    }
    
    PlayerData createRandomPlayerUpdate() {
        std::uniform_int_distribution<> player_dist(0, test_player_data_.size() - 1);
        std::uniform_real_distribution<float> delta_dist(-1.0f, 1.0f);
        
        PlayerData player = test_player_data_[player_dist(gen_)];
        
        // 随机更新位置
        auto* position = player.mutable_position();
        position->set_x(position->x() + delta_dist(gen_));
        position->set_y(position->y() + delta_dist(gen_));
        position->set_z(position->z() + delta_dist(gen_));
        
        // 更新时间戳
        player.set_timestamp(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()
            ).count()
        );
        
        return player;
    }
};

// 测试多线程访问PlayerRegistry的性能
BENCHMARK_F(ConcurrentBenchmarkBase, ConcurrentPlayerRegistryAccess)(benchmark::State& state) {
    static std::shared_ptr<PlayerRegistry> registry;
    static std::once_flag registry_flag;
    
    // 初始化共享的PlayerRegistry
    std::call_once(registry_flag, []() {
        registry = std::make_shared<PlayerRegistry>();
    });
    
    // 为每个线程分配唯一的玩家ID范围
    size_t thread_id = state.thread_index();
    size_t players_per_thread = 100;
    size_t start_id = thread_id * players_per_thread;
    
    for (auto _ : state) {
        // 每个线程操作自己的玩家数据
        for (size_t i = 0; i < 10; ++i) {
            PlayerData player = createRandomPlayerUpdate();
            player.set_player_id("thread_" + std::to_string(thread_id) + "_player_" + std::to_string(start_id + i));
            
            // 更新玩家数据
            registry->updatePlayer(player);
            
            // 偶尔读取所有玩家数据
            if (i % 5 == 0) {
                auto all_players = registry->getAllPlayers();
                benchmark::DoNotOptimize(all_players);
            }
        }
    }
    
    state.counters["ThreadOperationsPerSecond"] = benchmark::Counter(
        10 * state.iterations(), benchmark::Counter::kIsRate
    );
}
BENCHMARK_REGISTER_F(ConcurrentBenchmarkBase, ConcurrentPlayerRegistryAccess)
    ->Threads(1)->Threads(2)->Threads(4)->Threads(8)->Threads(16);

// 测试高并发消息处理性能
BENCHMARK_F(ConcurrentBenchmarkBase, HighConcurrencyMessageProcessing)(benchmark::State& state) {
    static std::queue<std::string> message_queue;
    static std::mutex queue_mutex;
    static std::condition_variable queue_cv;
    static std::atomic<bool> stop_producer{false};
    
    if (state.thread_index() == 0) {
        // 重置状态
        stop_producer = false;
        std::queue<std::string> empty;
        std::lock_guard<std::mutex> lock(queue_mutex);
        std::swap(message_queue, empty);
    }
    
    // 一半线程作为生产者，一半作为消费者
    bool is_producer = state.thread_index() < (state.threads() / 2 + 1);
    
    if (is_producer) {
        // 生产者线程：生成消息
        for (auto _ : state) {
            PlayerData player = createRandomPlayerUpdate();
            std::string serialized;
            player.SerializeToString(&serialized);
            
            {
                std::lock_guard<std::mutex> lock(queue_mutex);
                message_queue.push(std::move(serialized));
            }
            queue_cv.notify_one();
        }
    } else {
        // 消费者线程：处理消息
        size_t processed = 0;
        for (auto _ : state) {
            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_cv.wait(lock, []() { return !message_queue.empty() || stop_producer; });
            
            if (!message_queue.empty()) {
                std::string message = std::move(message_queue.front());
                message_queue.pop();
                lock.unlock();
                
                // 处理消息
                PlayerData player;
                if (player.ParseFromString(message)) {
                    ++processed;
                    benchmark::DoNotOptimize(player);
                }
            }
        }
        
        state.counters["MessagesProcessed"] = processed;
    }
    
    // 最后一个线程停止生产者
    if (state.thread_index() == state.threads() - 1) {
        stop_producer = true;
        queue_cv.notify_all();
    }
    
    state.counters["ThreadType"] = is_producer ? 1 : 0; // 1=Producer, 0=Consumer
}
BENCHMARK_REGISTER_F(ConcurrentBenchmarkBase, HighConcurrencyMessageProcessing)
    ->Threads(2)->Threads(4)->Threads(8)->Threads(16);

// 测试线程池模式的性能
class ThreadPoolBenchmark : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) override {
        thread_count_ = state.range(0);
        stop_threads_ = false;
        
        // 启动工作线程
        for (size_t i = 0; i < thread_count_; ++i) {
            threads_.emplace_back([this]() {
                while (!stop_threads_) {
                    std::function<void()> task;
                    
                    {
                        std::unique_lock<std::mutex> lock(queue_mutex_);
                        queue_cv_.wait(lock, [this]() { 
                            return !task_queue_.empty() || stop_threads_; 
                        });
                        
                        if (stop_threads_) break;
                        
                        if (!task_queue_.empty()) {
                            task = std::move(task_queue_.front());
                            task_queue_.pop();
                        }
                    }
                    
                    if (task) {
                        task();
                    }
                }
            });
        }
    }
    
    void TearDown(const ::benchmark::State& state) override {
        // 停止所有线程
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            stop_threads_ = true;
        }
        queue_cv_.notify_all();
        
        for (auto& thread : threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        
        threads_.clear();
        
        // 清空任务队列
        std::queue<std::function<void()>> empty;
        std::swap(task_queue_, empty);
    }

protected:
    void submitTask(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            task_queue_.push(std::move(task));
        }
        queue_cv_.notify_one();
    }
    
    size_t getQueueSize() {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        return task_queue_.size();
    }

private:
    size_t thread_count_;
    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> task_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::atomic<bool> stop_threads_;
};

BENCHMARK_F(ThreadPoolBenchmark, ThreadPoolPerformance)(benchmark::State& state) {
    std::atomic<size_t> completed_tasks{0};
    size_t total_tasks = 1000;
    
    for (auto _ : state) {
        completed_tasks = 0;
        
        // 提交大量任务
        for (size_t i = 0; i < total_tasks; ++i) {
            submitTask([&completed_tasks, i]() {
                // 模拟PlayerData处理
                PlayerData player;
                player.set_player_id("pool_player_" + std::to_string(i));
                player.set_timestamp(i);
                
                auto* position = player.mutable_position();
                position->set_x(static_cast<float>(i));
                position->set_y(static_cast<float>(i + 1));
                position->set_z(static_cast<float>(i + 2));
                
                std::string serialized;
                player.SerializeToString(&serialized);
                
                ++completed_tasks;
            });
        }
        
        // 等待所有任务完成
        while (completed_tasks < total_tasks) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
    
    state.counters["ThreadPoolSize"] = thread_count_;
    state.counters["TasksPerSecond"] = benchmark::Counter(
        total_tasks * state.iterations(), benchmark::Counter::kIsRate
    );
}
BENCHMARK_REGISTER_F(ThreadPoolBenchmark, ThreadPoolPerformance)
    ->Arg(1)->Arg(2)->Arg(4)->Arg(8)->Arg(16);

// 测试读写锁性能
static void BM_ReadWriteLockPerformance(benchmark::State& state) {
    std::shared_mutex rw_mutex;
    std::vector<PlayerData> shared_data;
    bool is_writer = state.thread_index() == 0; // 只有第一个线程是写者
    
    // 初始化共享数据
    if (is_writer) {
        shared_data.reserve(1000);
        for (int i = 0; i < 1000; ++i) {
            PlayerData player;
            player.set_player_id("rw_player_" + std::to_string(i));
            shared_data.push_back(std::move(player));
        }
    }
    
    for (auto _ : state) {
        if (is_writer) {
            // 写者：独占锁，修改数据
            std::unique_lock<std::shared_mutex> lock(rw_mutex);
            if (!shared_data.empty()) {
                auto& player = shared_data[0];
                player.set_timestamp(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::high_resolution_clock::now().time_since_epoch()
                    ).count()
                );
            }
        } else {
            // 读者：共享锁，读取数据
            std::shared_lock<std::shared_mutex> lock(rw_mutex);
            size_t total_players = shared_data.size();
            if (total_players > 0) {
                const auto& player = shared_data[0];
                benchmark::DoNotOptimize(player.player_id());
            }
        }
    }
    
    state.counters["AccessType"] = is_writer ? 1 : 0; // 1=Writer, 0=Reader
}
BENCHMARK(BM_ReadWriteLockPerformance)->Threads(1)->Threads(4)->Threads(8)->Threads(16);

// 测试无锁数据结构性能
static void BM_LockFreeDataStructure(benchmark::State& state) {
    std::atomic<uint64_t> counter{0};
    std::atomic<uint64_t> shared_timestamp{0};
    
    for (auto _ : state) {
        // 模拟无锁操作
        uint64_t current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count();
        
        // 原子操作
        uint64_t old_counter = counter.fetch_add(1, std::memory_order_relaxed);
        shared_timestamp.store(current_time, std::memory_order_release);
        
        // 模拟数据处理
        uint64_t read_timestamp = shared_timestamp.load(std::memory_order_acquire);
        benchmark::DoNotOptimize(old_counter + read_timestamp);
    }
    
    state.counters["AtomicOperationsPerSecond"] = benchmark::Counter(
        state.iterations(), benchmark::Counter::kIsRate
    );
}
BENCHMARK(BM_LockFreeDataStructure)->Threads(1)->Threads(2)->Threads(4)->Threads(8)->Threads(16);

// 测试批处理 vs 单个处理的并发性能
BENCHMARK_F(ConcurrentBenchmarkBase, BatchVsSingleProcessing)(benchmark::State& state) {
    bool use_batch = state.range(0) == 1;
    static std::shared_ptr<PlayerRegistry> registry;
    static std::once_flag registry_flag;
    
    std::call_once(registry_flag, []() {
        registry = std::make_shared<PlayerRegistry>();
    });
    
    if (use_batch) {
        // 批处理模式
        for (auto _ : state) {
            std::vector<PlayerData> batch;
            batch.reserve(10);
            
            // 准备批量数据
            for (int i = 0; i < 10; ++i) {
                PlayerData player = createRandomPlayerUpdate();
                player.set_player_id("batch_" + std::to_string(state.thread_index()) + "_" + std::to_string(i));
                batch.push_back(std::move(player));
            }
            
            // 批量更新
            for (const auto& player : batch) {
                registry->updatePlayer(player);
            }
        }
    } else {
        // 单个处理模式
        for (auto _ : state) {
            for (int i = 0; i < 10; ++i) {
                PlayerData player = createRandomPlayerUpdate();
                player.set_player_id("single_" + std::to_string(state.thread_index()) + "_" + std::to_string(i));
                registry->updatePlayer(player);
            }
        }
    }
    
    state.counters["ProcessingMode"] = use_batch ? 1 : 0; // 1=Batch, 0=Single
    state.counters["UpdatesPerSecond"] = benchmark::Counter(
        10 * state.iterations(), benchmark::Counter::kIsRate
    );
}
BENCHMARK_REGISTER_F(ConcurrentBenchmarkBase, BatchVsSingleProcessing)
    ->Args({0, 1})->Args({1, 1})->Args({0, 2})->Args({1, 2})
    ->Args({0, 4})->Args({1, 4})->Args({0, 8})->Args({1, 8});

// 测试内存屏障和缓存一致性影响
static void BM_MemoryBarrierImpact(benchmark::State& state) {
    std::atomic<uint64_t> shared_counter{0};
    bool use_strong_ordering = state.range(0) == 1;
    
    for (auto _ : state) {
        if (use_strong_ordering) {
            // 使用强内存序
            uint64_t value = shared_counter.fetch_add(1, std::memory_order_seq_cst);
            benchmark::DoNotOptimize(value);
        } else {
            // 使用弱内存序
            uint64_t value = shared_counter.fetch_add(1, std::memory_order_relaxed);
            benchmark::DoNotOptimize(value);
        }
    }
    
    state.counters["MemoryOrdering"] = use_strong_ordering ? 1 : 0; // 1=Strong, 0=Relaxed
    state.counters["OperationsPerSecond"] = benchmark::Counter(
        state.iterations(), benchmark::Counter::kIsRate
    );
}
BENCHMARK(BM_MemoryBarrierImpact)->Arg(0)->Arg(1)->Threads(1)->Threads(4)->Threads(8);
