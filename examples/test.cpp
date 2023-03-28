#include <iostream>
#include "concurrent_hash_map.h"

int main() {
    // 测试 insert_and_inc
    // noahyzhang::concurrent::ConcurrentHashMap<int, int> mp;
    // mp.insert_and_inc(10, 20);
    // mp.insert_and_inc(10, 30);
    // int value;
    // bool is_exist = mp.find(10, value);
    // if (is_exist) {
    //     std::cout << value << std::endl;
    // } else {
    //     std::cout << "not found key: 10" << std::endl;
    // }

    // 测试 insert_and_inc
    // struct Data {
    //     uint64_t a_;
    //     uint64_t b_;

    //     Data& operator+=(const Data& data) {
    //         a_ += data.a_;
    //         b_ += data.b_;
    //         return *this;
    //     }
    // };
    // noahyzhang::concurrent::ConcurrentHashMap<int, Data> mp_02;
    // mp_02.insert_and_inc(10, Data{10, 10});
    // mp_02.insert_and_inc(10, Data{20, 20});
    // Data value_02;
    // bool is_exist = mp_02.find(10, value_02);
    // if (is_exist) {
    //     std::cout << value_02.a_ << " " << value_02.b_ << std::endl;
    // } else {
    //     std::cout << "not found key: 10" << std::endl;
    // }

    // 测试迭代器
    noahyzhang::concurrent::ConcurrentHashMap<int, std::string> mp_03;
    mp_03.insert(10, "hello");
    mp_03.insert(20, "world");
    mp_03.insert(30, "ok");
    mp_03.insert(40, "noahyzhang");
    auto iter = mp_03.get_iterator();
    for (; iter != nullptr; iter++) {
        std::cout << iter->get_key() << ", " << iter->get_value() << std::endl;
    }
    return 0;
}
