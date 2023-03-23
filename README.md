## 线程安全的哈希表

### 一、实现思路

采用哈希桶的方式来实现线程安全的哈希表，每个桶中是一个单链表。
使用读写锁对每个桶的读写进行加锁同步，因此临界区为哈希桶的操作

1. 对于哈希桶的查找操作使用的读锁
2. 对于哈希桶的插入和删除操作使用的写锁

因此，对此哈希表的操作，多线程可以安全的、并发的同时操作同一个哈希表中的多个哈希桶

### 二、如何使用

如下使用多线程来操作 ConcurrentHashMap

```
#include <string>
#include <vector>
#include <thread>
#include <iostream>
#include <random>
#include "concurrent_hash_map.h"

#define THREAD_COUNT (10)
#define BENCHMARK_COUNT (1000)

noahyzhang::concurrent::ConcurrentHashMap<int, std::string> concurrent_map;

int main() {
    std::vector<std::thread> vec;
    // 生成随机值
    std::random_device rd;
    std::default_random_engine eng(rd());
    int min_val = 1, max_val = 1000;
    for (size_t i = 0; i < THREAD_COUNT; ++i) {
        vec.emplace_back(std::thread([&](){
            std::uniform_int_distribution<int> rand_range(min_val, max_val);
            for (size_t n = 0; n < BENCHMARK_COUNT; ++n) {
                int rand = rand_range(eng);
                concurrent_map.insert(rand, "hello");
                std::string expect_str;
                bool is_exist = concurrent_map.find(rand, expect_str);
                if (!is_exist) {
                    std::cerr << "ERROR! not found key: 10" << std::endl;
                    break;
                }
                if (expect_str != "hello") {
                    std::cerr << "ERROR! expect value: \"hello\", get value: " << expect_str << std::endl;
                    break;
                }
                std::cout << "find key success, key: " << rand << ", value: hello." << std::endl;
                concurrent_map.erase(10);
            }
        }));
        min_val = max_val + 1;
        max_val = max_val * 2;
    }
    for (auto& x : vec) {
        if (x.joinable()) {
            x.join();
        }
    }
    return 0;
}
```

### 三、性能

测试代码见 samples/test_concurrent_hash_map.cpp

```
如下，是我们的测试结果
测试条件，10 个线程在保证线程安全的情况下并发运行，每个线程进行插入、查找、删除操作 10 亿次
 1. 使用我们的 ConcurrentHashMap，单线程进行插入、查找、删除操作 10 亿次的总耗时大概 560 秒
 2. 使用 STL unordered_map，以 pthread_mutex_t 锁用于同步，但线程进行插入、查找、删除 10 亿次的总耗时大概 7000 秒
简单的结论，ConcurrentHashMap 在保证线程安全的情况下，要比直接加锁的方法性能好至少 12 倍
 
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

```