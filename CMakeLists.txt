cmake_minimum_required(VERSION 3.25.0)

project(concurrent_hash_map)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -Werror -std=c++11")

include_directories(
    ./include
)

# 测试性能
file(GLOB TEST_SRC
    ./examples/test_concurrent_hash_map.cpp
)
add_executable(test_concurrent_hash_map ${TEST_SRC})
target_link_libraries(test_concurrent_hash_map
    pthread
)

# 例子
file(GLOB SAMPLE_SRC
    ./examples/sample.cpp
)
add_executable(sample ${SAMPLE_SRC})
target_link_libraries(sample
    pthread
)

# 测试
file (GLOB OTHER_SRC
    ./examples/test.cpp
)
add_executable(test ${OTHER_SRC})
target_link_libraries(test
    pthread
)