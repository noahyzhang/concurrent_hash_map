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
