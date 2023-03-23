/**
 * @file test_concurrent_hash_map.cpp
 * @author noahyzhang
 * @brief 
 * @version 0.1
 * @date 2023-03-23
 * 
 * @copyright Copyright (c) 2023
 * 
 */

/**
 * 如下，是我们的测试结果
 * 测试条件，10 个线程在保证线程安全的情况下并发运行，每个线程进行插入、查找、删除操作 10 亿次
 * 1. 使用我们的 ConcurrentHashMap，单线程进行插入、查找、删除操作 10 亿次的总耗时大概 560 秒
 * 2. 使用 STL unordered_map，以 pthread_mutex_t 锁用于同步，但线程进行插入、查找、删除 10 亿次的总耗时大概 7000 秒
 * 简单的结论，ConcurrentHashMap 在保证线程安全的情况下，要比直接加锁的方法性能好至少 12 倍
 * 
    CONCURRENT map. tid: 9674 min_val: 1, max_val: 1000, benchmark count: 1000000000
    CONCURRENT map. tid: 9675 min_val: 1001, max_val: 2000, benchmark count: 1000000000
    CONCURRENT map. tid: 9676 min_val: 2001, max_val: 4000, benchmark count: 1000000000
    CONCURRENT map. tid: 9677 min_val: 4001, max_val: 8000, benchmark count: 1000000000
    CONCURRENT map. tid: 9678 min_val: 8001, max_val: 16000, benchmark count: 1000000000
    CONCURRENT map. tid: 9679 min_val: 16001, max_val: 32000, benchmark count: 1000000000
    CONCURRENT map. tid: 9680 min_val: 32001, max_val: 64000, benchmark count: 1000000000
    CONCURRENT map. tid: 9681 min_val: 64001, max_val: 128000, benchmark count: 1000000000
    CONCURRENT map. tid: 9682 min_val: 128001, max_val: 256000, benchmark count: 1000000000
    CONCURRENT map. tid: 9683 min_val: 256001, max_val: 512000, benchmark count: 1000000000
    tid: 9675 elapsed time: 560991 ms
    tid: 9682 elapsed time: 567004 ms
    tid: 9681 elapsed time: 568862 ms
    tid: 9677 elapsed time: 570835 ms
    tid: 9678 elapsed time: 573397 ms
    tid: 9680 elapsed time: 574059 ms
    tid: 9683 elapsed time: 575539 ms
    tid: 9679 elapsed time: 575936 ms
    tid: 9676 elapsed time: 576246 ms
    tid: 9674 elapsed time: 576352 ms


    STL map. tid: 11990 min_val: 1, max_val: 1000, benchmark count: 1000000000
    STL map. tid: 11991 min_val: 1001, max_val: 2000, benchmark count: 1000000000
    STL map. tid: 11992 min_val: 2001, max_val: 4000, benchmark count: 1000000000
    STL map. tid: 11993 min_val: 4001, max_val: 8000, benchmark count: 1000000000
    STL map. tid: 11994 min_val: 8001, max_val: 16000, benchmark count: 1000000000
    STL map. tid: 11995 min_val: 16001, max_val: 32000, benchmark count: 1000000000
    STL map. tid: 11996 min_val: 32001, max_val: 64000, benchmark count: 1000000000
    STL map. tid: 11997 min_val: 64001, max_val: 128000, benchmark count: 1000000000
    STL map. tid: 11998 min_val: 128001, max_val: 256000, benchmark count: 1000000000
    STL map. tid: 11999 min_val: 256001, max_val: 512000, benchmark count: 1000000000
    STL map. tid: 11990 elapsed time: 6977101 ms
    STL map. tid: 11991 elapsed time: 7002403 ms
    STL map. tid: 11997 elapsed time: 7004930 ms
    STL map. tid: 11998 elapsed time: 7008514 ms
    STL map. tid: 11995 elapsed time: 7015384 ms
    STL map. tid: 11994 elapsed time: 7017199 ms
    STL map. tid: 11996 elapsed time: 7018117 ms
    STL map. tid: 11999 elapsed time: 7018289 ms
    STL map. tid: 11993 elapsed time: 7018335 ms
    STL map. tid: 11992 elapsed time: 7018469 ms
 */

#include <unistd.h>
#include <sys/syscall.h>
#include <string>
#include <random>
#include <unordered_map>
#include <iostream>
#include "concurrent_hash_map.h"

using noahyzhang::concurrent::ConcurrentHashMap;

#define DEFAULT_THREAD_COUNT (10)
#define BENCHMARK_COUNT (1000000000)

struct ValueRange {
public:
    int min_val;
    int max_val;
    ValueRange(int min, int max) : min_val(min), max_val(max) {}
};

// 随机值
std::random_device rd;
std::default_random_engine eng(rd());
// 线程安全的哈希表
ConcurrentHashMap<int, std::string> concurrent_hash_map;
// STL 中哈希表
std::unordered_map<int, std::string> stl_map;
pthread_mutex_t mutex;

void* thr_func(void* arg) {
    ValueRange* value_range = reinterpret_cast<ValueRange*>(arg);
    auto tid = syscall(SYS_gettid);
    std::cout << "CONCURRENT map. tid: " << tid << " min_val: " << value_range->min_val
        << ", max_val: " << value_range->max_val << ", benchmark count: " << BENCHMARK_COUNT << std::endl;
    std::uniform_int_distribution<int> rand_range(value_range->min_val, value_range->max_val);
    auto start_tm = std::chrono::steady_clock::now();
    for (size_t i = 0; i < BENCHMARK_COUNT; ++i) {
        int rand_val = rand_range(eng);
        concurrent_hash_map.insert(rand_val, "hello");
        std::string expect_str;
        bool is_exist = concurrent_hash_map.find(rand_val, expect_str);
        if (!is_exist) {
            std::cerr << "ERROR: not found key: " << rand_val << std::endl;
            break;
        }
        if (expect_str != "hello") {
            std::cerr << "ERROR: expect str: Hello, but get str: " << expect_str << std::endl;
            break;
        }
        concurrent_hash_map.erase(rand_val);
    }
    delete value_range;
    auto end_tm = std::chrono::steady_clock::now();
    std::cout << "tid: " << tid << " elapsed time: "
        << std::chrono::duration_cast<std::chrono::milliseconds>(end_tm - start_tm).count() << " ms"
        << std::endl;
    return nullptr;
}

void* thr_func_02(void* arg) {
    ValueRange* value_range = reinterpret_cast<ValueRange*>(arg);
    auto tid = syscall(SYS_gettid);
    std::cout << "STL map. tid: " << tid << " min_val: " << value_range->min_val
        << ", max_val: " << value_range->max_val << ", benchmark count: " << BENCHMARK_COUNT << std::endl;
    std::uniform_int_distribution<int> rand_range(value_range->min_val, value_range->max_val);
    auto start_tm = std::chrono::steady_clock::now();
    for (size_t i = 0; i < BENCHMARK_COUNT; ++i) {
        int rand_val = rand_range(eng);
        pthread_mutex_lock(&mutex);
        stl_map.emplace(rand_val, "hello");
        auto iter = stl_map.find(rand_val);
        if (iter == stl_map.end()) {
            std::cerr << "ERROR: not found key: " << rand_val << std::endl;
            break;
        }
        if (iter->second != "hello") {
            std::cerr << "ERROR: expect str: Hello, but get str: " << iter->second << std::endl;
            break;
        }
        stl_map.erase(iter);
        pthread_mutex_unlock(&mutex);
    }
    delete value_range;
    auto end_tm = std::chrono::steady_clock::now();
    std::cout << "STL map. tid: " << tid << " elapsed time: "
        << std::chrono::duration_cast<std::chrono::milliseconds>(end_tm - start_tm).count() << " ms"
        << std::endl;
    return nullptr;
}

int main() {
    // 测试 CONCURRENT map
    pthread_t tids[DEFAULT_THREAD_COUNT];
    int min_val = 1, max_val = 1000;
    for (size_t i = 0; i < DEFAULT_THREAD_COUNT; ++i) {
        auto value_range = new ValueRange(min_val, max_val);
        pthread_create(&tids[i], nullptr, thr_func, reinterpret_cast<void*>(value_range));
        usleep(1000);
        min_val = max_val + 1;
        max_val = max_val * 2;
    }
    for (size_t i = 0; i < DEFAULT_THREAD_COUNT; ++i) {
        pthread_join(tids[i], nullptr);
    }

    std::cout << std::endl << std::endl << std::endl;

    // 测试 STL map
    pthread_mutex_init(&mutex, nullptr);
    pthread_t tids_02[DEFAULT_THREAD_COUNT];
    min_val = 1, max_val = 1000;
    for (size_t i = 0; i < DEFAULT_THREAD_COUNT; ++i) {
        auto value_range = new ValueRange(min_val, max_val);
        pthread_create(&tids_02[i], nullptr, thr_func_02, reinterpret_cast<void*>(value_range));
        usleep(1000);
        min_val = max_val + 1;
        max_val = max_val * 2;
    }
    for (size_t i = 0; i < DEFAULT_THREAD_COUNT; ++i) {
        pthread_join(tids_02[i], nullptr);
    }
    pthread_mutex_destroy(&mutex);

    return 0;
}
