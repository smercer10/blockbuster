#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <optional>

namespace Blockbuster::Spsc {

constexpr std::size_t cacheLineSize { 64 };

/**
 * @brief A lock-free Single-Producer Single-Consumer (SPSC) queue.
 *
 * This queue is designed for efficient communication between a single producer
 * thread and a single consumer thread. It uses atomic operations and careful
 * memory ordering to ensure thread-safety without locks.
 *
 * @tparam T The type of elements stored in the queue.
 * @tparam Capacity The maximum number of elements the queue can hold. Must be a power of 2.
 */
template <typename T, std::size_t Capacity>
class Queue {
public:
    Queue() = default;
    ~Queue() = default;

    // Delete copy and move constructors to avoid complications.
    Queue(const Queue&) = delete;
    auto operator=(const Queue&) -> Queue& = delete;
    Queue(Queue&&) = delete;
    auto operator=(Queue&&) -> Queue& = delete;

    /**
     * @brief Enqueues an item.
     *
     * @tparam U Type of the item to enqueue (allows for perfect forwarding).
     * @param item The item to enqueue.
     * @return true if the item was successfully enqueued, false if the queue was full.
     */
    template <typename U>
    auto enqueue(U&& item) -> bool
    {
        const std::size_t currTail { m_tail.load(std::memory_order_relaxed) };
        const std::size_t nextTail { wrap(currTail + 1) };

        if (nextTail == m_head.load(std::memory_order_relaxed)) {
            return false;
        }

        m_buffer[currTail] = std::forward<U>(item);
        m_tail.store(nextTail, std::memory_order_release);
        return true;
    }

    /**
     * @brief Dequeues an item.
     *
     * @return std::optional<T> containing the dequeued item if successful, or std::nullopt if the queue was empty.
     */
    auto dequeue() -> std::optional<T>
    {
        const std::size_t currHead { m_head.load(std::memory_order_relaxed) };

        if (currHead == m_tail.load(std::memory_order_acquire)) {
            return std::nullopt;
        }

        T item { std::move(m_buffer[currHead]) };
        m_head.store(wrap(currHead + 1), std::memory_order_release);
        return item;
    }

    /**
     * @brief Checks if the queue is empty.
     *
     * @return true if the queue is empty, false otherwise.
     */
    [[nodiscard]] auto empty() const -> bool
    {
        return m_head.load(std::memory_order_relaxed) == m_tail.load(std::memory_order_relaxed);
    }

    /**
     * @brief Checks if the queue is full.
     *
     * @return true if the queue is full, false otherwise.
     */
    [[nodiscard]] auto full() const -> bool
    {
        return wrap(m_tail.load(std::memory_order_relaxed) + 1) == m_head.load(std::memory_order_relaxed);
    }

    /**
     * @brief Returns the capacity of the queue.
     *
     * @return The maximum number of elements the queue can hold.
     */
    [[nodiscard]] constexpr auto capacity() const -> std::size_t
    {
        return m_capacity;
    }

    /**
     * @brief Returns the current number of elements in the queue.
     *
     * @return The current number of elements in the queue.
     */
    [[nodiscard]] auto size() const -> std::size_t
    {
        return wrap(m_tail.load(std::memory_order_relaxed) - m_head.load(std::memory_order_relaxed));
    }

private:
    static constexpr std::size_t m_capacity { Capacity };
    static_assert((m_capacity & (m_capacity - 1)) == 0, "Capacity must be a power of 2");

    // Wraps index to buffer bounds (equivalent to modulo when capacity is a power of 2).
    [[nodiscard]] auto wrap(std::size_t index) const -> std::size_t
    {
        return index & (m_capacity - 1);
    }

    std::array<T, m_capacity> m_buffer {};

    // Pad head and tail to avoid false sharing.
    alignas(cacheLineSize) std::atomic<std::size_t> m_head { 0 };
    alignas(cacheLineSize) std::atomic<std::size_t> m_tail { 0 };
};

} // namespace Blockbuster::Spsc
