#pragma once
#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

namespace Blockbuster::Mpmc {

constexpr std::size_t cacheLineSize { 64 };

/**
 * @brief A lock-free Multi-Producer Multi-Consumer (MPMC) hash map.
 *
 * This hash map is designed for efficient concurrent access by multiple producer and consumer threads.
 * It uses atomic operations, open addressing with linear probing for collision resolution, and supports resizing.
 *
 * @tparam Key The type of keys stored in the hash map (must be hashable and comparable).
 * @tparam Value The type of values stored in the hash map.
 * @tparam InitialCapacity The initial capacity of the hash map. Must be a power of 2.
 * @note The capacity doubles when the load factor is greater than or equal to 0.5. The capacity never shrinks.
 */
template <typename Key, typename Value, std::size_t InitialCapacity>
class HashMap {
public:
    HashMap()
        : m_table(std::make_shared<Table>(InitialCapacity))
    {
    }
    ~HashMap() = default;

    // Delete copy and move constructors to avoid complications.
    HashMap(const HashMap&) = delete;
    auto operator=(const HashMap&) -> HashMap& = delete;
    HashMap(HashMap&&) = delete;
    auto operator=(HashMap&&) -> HashMap& = delete;

    /**
     * @brief Inserts a key-value pair into the hash map.
     *
     * @param key The key to insert.
     * @param value The value to insert.
     * @return true if the insertion was successful, false if the key already exists.
     */
    auto insert(const Key& key, const Value& value) -> bool
    {
        while (true) {
            auto table { m_table.load(std::memory_order_acquire) };

            if (table->insert(key, value)) {
                return true;
            }

            if (table->m_size.load(std::memory_order_relaxed) > table->m_capacity / 2) {
                resize();
            }
        }
    }

    /**
     * @brief Retrieves a value from the hash map.
     *
     * @param key The key to look up.
     * @return std::optional<Value> containing the value if found, or std::nullopt if not found.
     */
    [[nodiscard]] auto get(const Key& key) const -> std::optional<Value>
    {
        return m_table.load(std::memory_order_acquire)->get(key);
    }

    /**
     * @brief Removes a key-value pair from the hash map.
     *
     * @param key The key to remove.
     * @return true if the key was found and removed, false otherwise.
     */
    auto remove(const Key& key) -> bool
    {
        return m_table.load(std::memory_order_acquire)->remove(key);
    }

    /**
     * @brief Returns the current capacity of the hash map.
     *
     * @return The current maximum number of elements the hash map can hold.
     * @note This may return incorrect results under concurrent access and should only be used as a heuristic.
     */
    [[nodiscard]] auto capacity() const -> std::size_t
    {
        return m_table.load(std::memory_order_relaxed)->m_capacity;
    }

    /**
     * @brief Returns the current size of the hash map.
     *
     * @return The current number of elements in the hash map.
     * @note This may return incorrect results under concurrent access and should only be used as a heuristic.
     */
    [[nodiscard]] auto size() const -> std::size_t
    {
        return m_table.load(std::memory_order_relaxed)->m_size.load(std::memory_order_relaxed);
    }

private:
    enum class CellState : uint8_t { empty,
        writing,
        full,
        deleted };

    struct alignas(cacheLineSize) Cell {
        std::atomic<CellState> state { CellState::Empty };
        Key key {};
        Value value {};
    };

    class Table {
    public:
        explicit Table(std::size_t capacity)
            : m_capacity(capacity)
            , m_buffer(capacity)
        {
        }

        auto insert(const Key& key, const Value& value) -> bool
        {
            const std::size_t index { hash(key) };

            for (std::size_t i { 0 }; i < m_capacity; ++i) {
                Cell& cell { m_buffer[wrap(index + i)] };
                const CellState expected { CellState::Empty };

                if (cell.state.compare_exchange_strong(expected, CellState::Writing, std::memory_order_acq_rel)) {
                    cell.key = key;
                    cell.value = value;
                    cell.state.store(CellState::Full, std::memory_order_release);
                    m_size.fetch_add(1, std::memory_order_relaxed);
                    return true;
                }

                if (expected == CellState::Full && cell.key == key) {
                    return false;
                }
            }

            return false;
        }

        [[nodiscard]] auto get(const Key& key) const -> std::optional<Value>
        {
            const std::size_t index { hash(key) };

            for (std::size_t i { 0 }; i < m_capacity; ++i) {
                const Cell& cell { m_buffer[wrap(index + i)] };

                if (cell.state.load(std::memory_order_acquire) == CellState::Full) {
                    if (cell.key == key) {
                        return cell.value;
                    }
                } else if (cell.state.load(std::memory_order_acquire) == CellState::Empty) {
                    return std::nullopt;
                }
            }

            return std::nullopt;
        }

        auto remove(const Key& key) -> bool
        {
            const std::size_t index { hash(key) };

            for (std::size_t i { 0 }; i < m_capacity; ++i) {
                Cell& cell { m_buffer[wrap(index + i)] };
                const CellState expected { CellState::Full };

                if (cell.state.load(std::memory_order_acquire) == CellState::Full && cell.key == key) {
                    if (cell.state.compare_exchange_strong(expected, CellState::Deleted, std::memory_order_acq_rel)) {
                        m_size.fetch_sub(1, std::memory_order_relaxed);
                        return true;
                    }
                }

                if (cell.state.load(std::memory_order_acquire) == CellState::Empty) {
                    return false;
                }
            }

            return false;
        }

    private:
        // Wraps index to buffer bounds (equivalent to modulo when capacity is a power of 2).
        [[nodiscard]] auto wrap(std::size_t index) const -> std::size_t

        {
            return index & (m_capacity - 1);
        }

        // Compute hash and return corresponding index within buffer bounds.
        [[nodiscard]] auto hash(const Key& key) const -> std::size_t
        {
            return std::hash<Key> {}(key) & (m_capacity - 1);
        }

        std::size_t m_capacity {};
        std::vector<Cell> m_buffer {};
        std::atomic<std::size_t> m_size { 0 };
    };

    void resize()
    {
        const auto oldTable { m_table.load(std::memory_order_acquire) };
        auto newTable { std::make_shared<Table>(oldTable->m_capacity * 2) };

        for (const auto& cell : oldTable->m_buffer) {
            if (cell.state.load(std::memory_order_acquire) == CellState::Full) {
                newTable->insert(cell.key, cell.value);
            }
        }

        m_table.store(newTable, std::memory_order_release);
    }

    std::atomic<std::shared_ptr<Table>> m_table {};
};

} // namespace Blockbuster::Mpmc
