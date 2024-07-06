#include "blockbuster/spsc/queue.hpp"
#include <cstddef>
#include <gtest/gtest.h>
#include <thread>

constexpr std::size_t capacity { 16 };
constexpr std::size_t actualCapacity { capacity - 1 };

class SpscQueueTest : public ::testing::Test {
protected:
    Blockbuster::Spsc::Queue<int, capacity> queue {};
};

TEST_F(SpscQueueTest, EnqueueDequeue)
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

TEST_F(SpscQueueTest, EmptyAndFull)
{
    EXPECT_TRUE(queue.empty());
    EXPECT_FALSE(queue.full());

    for (std::size_t i { 0 }; i < actualCapacity; ++i) {
        EXPECT_TRUE(queue.enqueue(static_cast<int>(i)));
    }

    EXPECT_FALSE(queue.empty());
    EXPECT_TRUE(queue.full());

    EXPECT_FALSE(queue.enqueue(100));
}

TEST_F(SpscQueueTest, SizeAndCapacity)
{
    EXPECT_EQ(queue.size(), 0);
    EXPECT_EQ(queue.capacity(), capacity);

    for (std::size_t i { 0 }; i < capacity / 2; ++i) {
        queue.enqueue(static_cast<int>(i));
    }

    EXPECT_EQ(queue.size(), capacity / 2);
    EXPECT_EQ(queue.capacity(), capacity);
}

TEST_F(SpscQueueTest, WrapAround)
{
    for (std::size_t i { 0 }; i < actualCapacity; ++i) {
        EXPECT_TRUE(queue.enqueue(static_cast<int>(i)));
    }

    for (std::size_t i { 0 }; i < actualCapacity; ++i) {
        auto value { queue.dequeue() };
        EXPECT_TRUE(value.has_value());
        EXPECT_EQ(*value, static_cast<int>(i));
    }

    for (std::size_t i { 0 }; i < actualCapacity; ++i) {
        EXPECT_TRUE(queue.enqueue(static_cast<int>(i + 100)));
    }

    for (std::size_t i { 0 }; i < actualCapacity; ++i) {
        auto value { queue.dequeue() };
        EXPECT_TRUE(value.has_value());
        EXPECT_EQ(*value, static_cast<int>(i + 100));
    }
}

TEST_F(SpscQueueTest, SingleProducerAndConsumer)
{
    constexpr int iterations { 1000000 };

    std::thread producer([this]() {
        for (int i { 0 }; i < iterations; ++i) {
            while (!queue.enqueue(i)) {
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([this]() {
        for (int i { 0 }; i < iterations; ++i) {
            std::optional<int> value {};
            while (!(value = queue.dequeue())) {
                std::this_thread::yield();
            }
            EXPECT_EQ(*value, i);
        }
    });

    producer.join();
    consumer.join();

    EXPECT_TRUE(queue.empty());
}
