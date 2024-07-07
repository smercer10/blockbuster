#include "blockbuster/mpmc/queue.hpp"
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <gtest/gtest.h>
#include <thread>

constexpr std::size_t capacity { 16 };

class MpmcQueueTest : public ::testing::Test {
protected:
    Blockbuster::Mpmc::Queue<int, capacity> queue;
};

TEST_F(MpmcQueueTest, EnqueueDequeue)
{
    EXPECT_TRUE(queue.enqueue(1));
    EXPECT_TRUE(queue.enqueue(2));
    EXPECT_TRUE(queue.enqueue(3));

    auto value { queue.dequeue() };
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(*value, 1);

    value = queue.dequeue();
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(*value, 2);

    value = queue.dequeue();
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(*value, 3);
}

TEST_F(MpmcQueueTest, EmptyAndFull)
{
    EXPECT_TRUE(queue.empty());
    EXPECT_FALSE(queue.full());

    for (std::size_t i { 0 }; i < capacity; ++i) {
        EXPECT_TRUE(queue.enqueue(static_cast<int>(i)));
    }

    EXPECT_FALSE(queue.empty());
    EXPECT_TRUE(queue.full());

    EXPECT_FALSE(queue.enqueue(100));
}

TEST_F(MpmcQueueTest, SizeAndCapacity)
{
    EXPECT_EQ(queue.size(), 0);
    EXPECT_EQ(queue.capacity(), capacity);

    for (std::size_t i { 0 }; i < capacity / 2; ++i) {
        queue.enqueue(static_cast<int>(i));
    }

    EXPECT_EQ(queue.size(), capacity / 2);
    EXPECT_EQ(queue.capacity(), capacity);
}

TEST_F(MpmcQueueTest, WrapAround)
{
    for (std::size_t i { 0 }; i < capacity; ++i) {
        EXPECT_TRUE(queue.enqueue(static_cast<int>(i)));
    }

    for (std::size_t i { 0 }; i < capacity; ++i) {
        auto value { queue.dequeue() };
        EXPECT_TRUE(value.has_value());
        EXPECT_EQ(*value, static_cast<int>(i));
    }

    for (std::size_t i { 0 }; i < capacity; ++i) {
        EXPECT_TRUE(queue.enqueue(static_cast<int>(i + 100)));
    }

    for (std::size_t i { 0 }; i < capacity; ++i) {
        auto value { queue.dequeue() };
        EXPECT_TRUE(value.has_value());
        EXPECT_EQ(*value, static_cast<int>(i + 100));
    }
}

TEST_F(MpmcQueueTest, MultipleProducersAndConsumers)
{
    constexpr int numProducers { 4 };
    constexpr int numConsumers { 4 };
    constexpr int iterationsPerThread { 1000000 };
    constexpr int totalIterations { numProducers * iterationsPerThread };

    std::atomic<int> producedCount { 0 };
    std::atomic<int> consumedCount { 0 };
    std::vector<int> consumedValues(static_cast<std::size_t>(totalIterations), -1, std::allocator<int>());

    std::vector<std::thread> producers {};
    std::vector<std::thread> consumers {};

    for (int p { 0 }; p < numProducers; ++p) {
        producers.emplace_back([this, p, &producedCount]() {
            for (int i { 0 }; i < iterationsPerThread; ++i) {
                int value { p * iterationsPerThread + i };
                while (!queue.enqueue(value)) {
                    std::this_thread::yield();
                }
                producedCount.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (int c { 0 }; c < numConsumers; ++c) {
        consumers.emplace_back([this, &consumedCount, &consumedValues]() {
            while (consumedCount.load(std::memory_order_relaxed) < totalIterations) {
                std::optional<int> value { queue.dequeue() };
                if (value) {
                    int index { consumedCount.fetch_add(1, std::memory_order_relaxed) };
                    if (index < totalIterations) {
                        consumedValues[index] = *value;
                    }
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    for (auto& p : producers) {
        p.join();
    }
    for (auto& c : consumers) {
        c.join();
    }

    EXPECT_EQ(producedCount.load(std::memory_order_relaxed), totalIterations);
    EXPECT_EQ(consumedCount.load(std::memory_order_relaxed), totalIterations);
    EXPECT_TRUE(queue.empty());

    std::sort(consumedValues.begin(), consumedValues.end());
    for (int i { 0 }; i < totalIterations; ++i) {
        EXPECT_EQ(consumedValues[i], i);
    }
}
