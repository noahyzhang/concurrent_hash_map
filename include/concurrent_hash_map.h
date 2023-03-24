/**
 * @file concurrent_hash_map.h
 * @author noahyzhang
 * @brief 
 * @version 0.1
 * @date 2023-03-23
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#pragma once

#include <functional>
#include <atomic>
#include <thread>
#include <memory>

namespace noahyzhang {
namespace concurrent {

// 默认的哈希桶的数量，注意取一个质数可以使哈希表有更好的性能
#define DEFAULT_HASH_BUCKET_SIZE (1031)

template <typename K, typename V> class HashNode;
template <typename K, typename V> class HashBucket;

/**
 * @brief 线程安全的哈希表
 *        以哈希桶作为实现，每个桶是一个单链表
 *        我们加锁的临界区为桶，所以多个线程可以并发写入哈希表中的不同桶
 * 
 * @tparam K 哈希表的键
 * @tparam V 哈希表的值
 * @tparam F 哈希函数，默认使用 stl 提供的哈希函数
 */
template <typename K, typename V, typename F = std::hash<K>>
class ConcurrentHashMap {
public:
    explicit ConcurrentHashMap(size_t hash_bucket_size = DEFAULT_HASH_BUCKET_SIZE)
        : hash_bucket_size_(hash_bucket_size) {
        hash_table_ = new HashBucket<K, V>[hash_bucket_size];
    }
    ~ConcurrentHashMap() {
        delete[] hash_table_;
    }
    ConcurrentHashMap(const ConcurrentHashMap&) = delete;
    ConcurrentHashMap& operator=(const ConcurrentHashMap&) = delete;
    ConcurrentHashMap(ConcurrentHashMap&&) = delete;
    ConcurrentHashMap& operator=(ConcurrentHashMap&&) = delete;

public:
    /**
     * @brief 查找哈希表中是否有 key，返回 bool 值
     *        如果存在的话，则给 value 赋值
     * @param key 
     * @param value 
     * @return true 
     * @return false 
     */
    bool find(const K& key, V& value) const {
        size_t hash_val = hash_fn_(key) % hash_bucket_size_;
        return hash_table_[hash_val].find(key, value);
    }

    /**
     * @brief 插入一对键值
     * 
     * @param key 
     * @param value 
     */
    void insert(const K& key, const V& value) {
        size_t hash_val = hash_fn_(key) % hash_bucket_size_;
        hash_table_[hash_val].insert(key, value);
    }

    /**
     * @brief 插入一对键值，如果键存在，则增加值
     * 
     * @param key 
     * @param value 
     */
    void insert_and_inc(const K& key, const V& value) {
        size_t hash_val = hash_fn_(key) % hash_bucket_size_;
        hash_table_[hash_val].insert_and_inc(key, value);
    }

    /**
     * @brief 删除某个键
     * 
     * @param key 
     */
    void erase(const K& key) {
        size_t hash_val = hash_fn_(key) % hash_bucket_size_;
        hash_table_[hash_val].erase(key);
    }

    /**
     * @brief 清空哈希表
     * 
     */
    void clear() {
        for (size_t i = 0; i < hash_bucket_size_; ++i) {
            hash_table_[i].clear();
        }
    }

private:
    /**
     * @brief 迭代器，待优化
     * 
     */
    class ConstIterator {
    public:
        explicit ConstIterator(HashNode<K, V>* hash_node) : hash_node_(hash_node) {}

        const K& first() const {
            return hash_node_->get_key();
        }
        const V& second() const {
            return hash_node_->get_value();
        }
        bool operator==(const ConstIterator& other) const {
            if (other.hash_node_ == nullptr || hash_node_ == nullptr) {
                return false;
            }
            if ((other.hash_node_->get_key() == hash_node_->get_key())
                && (other.hash_node_->get_value() == hash_node_->get_value())) {
                return true;
            }
            return false;
        }
        bool operator==(void* point) const {
            return hash_node_ == point;
        }
        bool operator!=(const ConstIterator& other) const {
            return !ConstIterator::operator==(other);
        }
        bool operator!=(void* point) const {
            return !ConstIterator::operator==(point);
        }

    private:
        HashNode<K, V>* hash_node_ = nullptr;
    };

private:
    // 哈希桶，以数组的形式实现
    HashBucket<K, V>* hash_table_;
    // 哈希函数
    F hash_fn_;
    // 哈希桶的个数
    size_t hash_bucket_size_;
};

/**
 * @brief 哈希桶的实现
 *        每个桶是以一个单链表的形式实现
 * 
 * @tparam K 
 * @tparam V 
 */
template <typename K, typename V>
class HashBucket {
public:
    HashBucket() {
        pthread_rwlock_init(&rw_lock_, nullptr);
    }
    ~HashBucket() {
        clear();
        pthread_rwlock_destroy(&rw_lock_);
    }
    HashBucket(const HashBucket&) = delete;
    HashBucket& operator=(const HashBucket&) = delete;
    HashBucket(HashBucket&&) = delete;
    HashBucket& operator=(HashBucket&&) = delete;

public:
    /**
     * @brief 查找某个键值，返回 bool 值
     *        如果存在，则给 value 赋值
     * 
     * @param key 
     * @param value 
     * @return true 
     * @return false 
     */
    bool find(const K& key, V& value) {
        // 加读锁
        pthread_rwlock_rdlock(&rw_lock_);
        HashNode<K, V>* node = head_;
        for (; node != nullptr;) {
            if (node->get_key() == key) {
                value = node->get_value();
                pthread_rwlock_unlock(&rw_lock_);
                return true;
            }
            node = node->next_;
        }
        pthread_rwlock_unlock(&rw_lock_);
        return false;
    }

    /**
     * @brief 插入一对键值
     * 
     * @param key 
     * @param value 
     */
    void insert(const K& key, const V& value) {
        // 加写锁
        pthread_rwlock_wrlock(&rw_lock_);
        HashNode<K, V>* prev = nullptr, *node = head_;
        for (; node != nullptr && node->get_key() != key;) {
            prev = node;
            node = node->next_;
        }
        // 此时有两种情况
        // 1. head_ 本身为空
        // 2. head_ 链表遍历完也没有发现 key，此时 node 指向尾节点的 next，为空，prev 指向尾节点
        if (node == nullptr) {
            if (head_ == nullptr) {
                head_ = new HashNode<K, V>(key, value);
            } else {
                prev->next_ = new HashNode<K, V>(key, value);
            }
        } else {
            // 桶中存在 key，直接修改
            node->set_value(value);
        }
        pthread_rwlock_unlock(&rw_lock_);
    }

    /**
     * @brief 插入一对键值，如果键存在，则增加值
     * 
     * @param key 
     * @param value 
     */
    void insert_and_inc(const K& key, const V& value) {
        // 加写锁
        pthread_rwlock_wrlock(&rw_lock_);
        HashNode<K, V>* prev = nullptr, *node = head_;
        for (; node != nullptr && node->get_key() != key;) {
            prev = node;
            node = node->next_;
        }
        // 此时有两种情况
        // 1. head_ 本身为空
        // 2. head_ 链表遍历完也没有发现 key，此时 node 指向尾节点的 next，为空，prev 指向尾节点
        if (node == nullptr) {
            if (head_ == nullptr) {
                head_ = new HashNode<K, V>(key, value);
            } else {
                prev->next_ = new HashNode<K, V>(key, value);
            }
        } else {
            // 桶中存在 key，给他增加
            node->get_value() += value;
        }
        pthread_rwlock_unlock(&rw_lock_);
    }

    /**
     * @brief 删除某个键值
     * 
     * @param key 
     */
    void erase(const K& key) {
        // 加写锁
        pthread_rwlock_wrlock(&rw_lock_);
        HashNode<K, V>* prev = nullptr, *node = head_;
        for (; node != nullptr && node->get_key() != key;) {
            prev = node;
            node = node->next_;
        }
        // key 没有找到，直接返回
        if (node == nullptr) {
            pthread_rwlock_unlock(&rw_lock_);
            return;
        } else {
            // 找到 key，分情况处理
            // 1. 如果此节点是头节点 2. 如果此节点不是头节点
            if (head_ == node) {
                head_ = node->next_;
            } else {
                prev->next_ = node->next_;
            }
            delete node;
        }
        pthread_rwlock_unlock(&rw_lock_);
    }

    /**
     * @brief 清理桶中所有元素
     * 
     */
    void clear() {
        // 加写锁
        pthread_rwlock_wrlock(&rw_lock_);
        HashNode<K, V>* prev = nullptr, *node = head_;
        for (; node != nullptr;) {
            prev = node;
            node = node->next_;
            delete prev;
        }
        head_ = nullptr;
        pthread_rwlock_unlock(&rw_lock_);
    }

private:
    // 桶中单链表的头节点
    HashNode<K, V>* head_ = nullptr;
    // 读写锁
    pthread_rwlock_t rw_lock_;
};

/**
 * @brief 哈希桶中的节点，哈希桶中是以单链表作为数据结构
 * 
 * @tparam K 
 * @tparam V 
 */
template <typename K, typename V>
class HashNode {
public:
    HashNode() = default;
    HashNode(K key, V value) : key_(key), value_(value) {}
    ~HashNode() {
        next_ = nullptr;
    }
    HashNode(const HashNode&) = delete;
    HashNode& operator=(const HashNode&) = delete;
    HashNode(HashNode&&) = delete;
    HashNode& operator=(HashNode&&) = delete;

public:
    /**
     * @brief 获取节点的键
     * 
     * @return const K& 
     */
    const K& get_key() const {
        return key_;
    }

    /**
     * @brief 获取节点的值
     * 
     * @return const V& 
     */
    const V& get_value() const {
        return value_;
    }

    /**
     * @brief 获取节点的值
     * 
     * @return V& 
     */
    V& get_value() {
        return value_;
    }

    /**
     * @brief 设置节点的值
     * 
     * @param value 
     */
    void set_value(const V value) {
        value_ = value;
    }

public:
    // 单链表的下一个指针
    HashNode* next_ = nullptr;

private:
    // 节点的键
    K key_;
    // 节点的值
    V value_;
};

}  // namespace concurrent
}  // namespace noahyzhang
