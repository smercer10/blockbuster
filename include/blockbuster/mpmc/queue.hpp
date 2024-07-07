#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <optional>

namespace Blockbuster::Mpmc {

constexpr std::size_t cacheLineSize { 64 };

/**
 * @brief A lock-free Multi-Producer Multi-Consumer (MPMC) queue.
 *
 * This queue supports safe concurrent access from multiple producer and consumer threads without locks.
 *
 * @tparam T The type of elements stored in the queue.
 * @tparam Capacity The maximum number of elements the queue can hold. Must be a power of 2.
 */
template <typename T, std::size_t Capacity>
class Queue {
public:
    Queue()
    {
        for (std::size_t i { 0 }; i < s_capacity; ++i) {
            m_buffer[i].sequence.store(i, std::memory_order_relaxed);
        }
    }
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
        Cell* cell {};
        std::size_t pos { m_enqueuePos.load(std::memory_order_relaxed) };

        for (;;) {
            cell = &m_buffer[wrap(pos)];
            std::size_t seq { cell->sequence.load(std::memory_order_acquire) };
            intptr_t dif { static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos) };

            if (dif == 0) {
                if (m_enqueuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    break;
                }
            } else if (dif < 0) {
                return false;
            } else {
                pos = m_enqueuePos.load(std::memory_order_relaxed);
            }
        }

        cell->data = std::forward<U>(item);
        cell->sequence.store(pos + 1, std::memory_order_release);
        return true;
    }

    /**
     * @brief Dequeues an item.
     *
     * @return An optional containing the dequeued item if successful, or std::nullopt if the queue was empty.
     */
    auto dequeue() -> std::optional<T>
    {
        Cell* cell {};
        std::size_t pos { m_dequeuePos.load(std::memory_order_relaxed) };

        for (;;) {
            cell = &m_buffer[wrap(pos)];
            std::size_t seq { cell->sequence.load(std::memory_order_acquire) };
            intptr_t dif { static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1) };

            if (dif == 0) {
                if (m_dequeuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    break;
                }
            } else if (dif < 0) {
                return std::nullopt;
            } else {
                pos = m_dequeuePos.load(std::memory_order_relaxed);
            }
        }

        T result { std::move(cell->data) };
        cell->sequence.store(pos + s_capacity, std::memory_order_release);
        return result;
    }

    /**
     * @brief Checks if the queue is empty.
     *
     * @return true if the queue is empty, false otherwise.
     * @note This may return incorrect results under concurrent access and should only be used as a heuristic.
     */
    [[nodiscard]] auto empty() const -> bool
    {
        return (m_enqueuePos.load(std::memory_order_relaxed) - m_dequeuePos.load(std::memory_order_relaxed)) <= 0;
    }

    /**
     * @brief Checks if the queue is full.
     *
     * @return true if the queue is full, false otherwise.
     * @note This may return incorrect results under concurrent access and should only be used as a heuristic.
     */
    [[nodiscard]] auto full() const -> bool
    {
        return (m_enqueuePos.load(std::memory_order_relaxed) - m_dequeuePos.load(std::memory_order_relaxed)) >= s_capacity;
    }

    /**
     * @brief Returns the capacity of the queue.
     *
     * @return The maximum number of elements the queue can hold.
     */
    [[nodiscard]] constexpr auto capacity() const -> std::size_t
    {
        return s_capacity;
    }

    /**
     * @brief Returns the current number of elements in the queue.
     *
     * @return The current number of elements in the queue.
     * @note This may return incorrect results under concurrent access and should only be used as a heuristic.
     */
    [[nodiscard]] auto size() const -> std::size_t
    {
        return m_enqueuePos.load(std::memory_order_relaxed) - m_dequeuePos.load(std::memory_order_relaxed);
    }

private:
    struct Cell {
        std::atomic<size_t> sequence {};
        T data {};
    };

    static constexpr std::size_t s_capacity { Capacity };
    static_assert(s_capacity > 0 && (s_capacity & (s_capacity - 1)) == 0, "Capacity must be greater than 0 and a power of 2");

    // Wraps index to buffer bounds (equivalent to modulo when capacity is a power of 2).
    [[nodiscard]] auto wrap(std::size_t index) const -> std::size_t
    {
        return index & (s_capacity - 1);
    }

    std::array<Cell, s_capacity> m_buffer {};

    // Pad as necessary to avoid false sharing.
    alignas(cacheLineSize) std::atomic<size_t> m_enqueuePos { 0 };
    alignas(cacheLineSize) std::atomic<size_t> m_dequeuePos { 0 };
};

} // namespace Blockbuster::Mpmc
