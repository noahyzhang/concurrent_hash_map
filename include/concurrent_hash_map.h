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
#include <utility>
#include <memory>

namespace noahyzhang {
namespace concurrent {

// 默认的哈希桶的数量，注意取一个质数可以使哈希表有更好的性能
#define DEFAULT_HASH_BUCKET_SIZE (1031)

template <typename K, typename V> class HashNode;
template <typename K, typename V> class HashBucket;
template <typename K, typename V> class ConstIterator;

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

    /**
     * @brief 获取迭代器
     * 
     * @return ConstIterator 
     */
    ConstIterator<K, V> get_iterator() {
        return ConstIterator<K, V>(this);
    }

private:
    // 哈希桶，以数组的形式实现
    HashBucket<K, V>* hash_table_;
    // 哈希函数
    F hash_fn_;
    // 哈希桶的个数
    size_t hash_bucket_size_;
    friend class ConstIterator<K, V>;
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

public:
    // 桶中单链表的头节点
    HashNode<K, V>* head_ = nullptr;

private:
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

/**
 * @brief 迭代器，待优化
 * 
 */

/**
 * @brief 迭代器
 *  注意：此迭代器非线程安全，特别注意
 * @tparam K 
 * @tparam V 
 */
template <typename K, typename V>
class ConstIterator {
public:
    ConstIterator() = delete;
    ~ConstIterator() = default;
    explicit ConstIterator(ConcurrentHashMap<K, V>* cmp) : cmp_(cmp) {
        for (; hash_node_ == nullptr && bucket_pos_ < cmp_->hash_bucket_size_;) {
            HashNode<K, V>* node = cmp_->hash_table_[bucket_pos_].head_;
            if (node != nullptr) {
                hash_node_ = node;
                break;
            }
            if (++bucket_pos_ >= cmp_->hash_bucket_size_) break;
        }
    }
    // 拷贝函数使用浅拷贝是没有问题的
    ConstIterator(const ConstIterator&) = default;
    ConstIterator& operator=(const ConstIterator&) = default;
    ConstIterator(ConstIterator&&) = default;
    ConstIterator& operator=(ConstIterator&&) = default;

public:
    /**
     * @brief 运算符 == 重载
     *  比较存储的 node 的 key 、value 是否相等
     * @param other 
     * @return true 
     * @return false 
     */
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

    /**
     * @brief 运算符 == 重载
     *  比较 node 是否为空
     * @param point 
     * @return true 
     * @return false 
     */
    bool operator==(void* point) const {
        return hash_node_ == point;
    }

    /**
     * @brief 运算符 != 重载
     *  
     * @param other 
     * @return true 
     * @return false 
     */
    bool operator!=(const ConstIterator& other) const {
        return !ConstIterator::operator==(other);
    }

    /**
     * @brief 运算符 != 重载
     * 
     * @param point 
     * @return true 
     * @return false 
     */
    bool operator!=(void* point) const {
        return !ConstIterator::operator==(point);
    }

    /**
     * @brief 运算符 ++ 重载
     * 
     * @return ConstIterator& 
     */
    ConstIterator& operator++(int) {
        // hash_node 为空，直接返回
        if (hash_node_ == nullptr) return *this;
        // hash_node 不为空，并且 next 有值，则返回 next
        if (hash_node_->next_ != nullptr) {
            hash_node_ = hash_node_->next_;
            return *this;
        }
        // 如果 hash_node 是当前桶中最后一个元素，寻找下一个桶的非空头节点
        ++bucket_pos_;
        for (; bucket_pos_ < cmp_->hash_bucket_size_; ++bucket_pos_) {
            HashNode<K, V>* node = cmp_->hash_table_[bucket_pos_].head_;
            if (node != nullptr) {
                hash_node_ = node;
                return *this;
            }
        }
        hash_node_ = nullptr;
        return *this;
    }

    /**
     * @brief 运算符 -> 重载
     *  返回 node 指针
     * @return HashNode<K, V>* 
     */
    HashNode<K, V>* operator->() const {
        return hash_node_;
    }

private:
    // hash map 的指针
    ConcurrentHashMap<K, V>* cmp_;
    // 当前处于那个 bucket 位置
    uint64_t bucket_pos_ = 0;
    // 当前指向的 node
    HashNode<K, V>* hash_node_ = nullptr;
};

}  // namespace concurrent
}  // namespace noahyzhang
