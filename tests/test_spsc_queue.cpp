#include <gtest/gtest.h>
#include <llob/spsc_queue.hpp>
#include <thread>
#include <immintrin.h>
#include <vector>

struct alignas(64) TestSlot
{
    int  qty;
    int  price;
    char direction;
};

TEST(SPSCQueue, PushAndPopSingleThread)
{
    llob::SPSCQueue<TestSlot, 8> queue;

    TestSlot slot{100, 5, 'B'};

    EXPECT_FALSE(queue.pop(slot));       // empty on construction
    EXPECT_TRUE(queue.push(slot));

    for (int i = 0; i < 7; ++i)
        EXPECT_TRUE(queue.push(slot));   // fill to capacity

    EXPECT_FALSE(queue.push(slot));      // full

    TestSlot out{};
    for (int i = 0; i < 8; ++i) {
        EXPECT_TRUE(queue.pop(out));
        EXPECT_EQ(out.qty,       100);
        EXPECT_EQ(out.price,     5);
        EXPECT_EQ(out.direction, 'B');
    }
    EXPECT_FALSE(queue.pop(out));        // empty again

    // wrap-around: indices continue past Capacity
    for (int i = 0; i < 4; ++i)
        EXPECT_TRUE(queue.push(slot));

    for (int i = 0; i < 4; ++i) {
        EXPECT_TRUE(queue.pop(out));
        EXPECT_EQ(out.qty,       100);
        EXPECT_EQ(out.price,     5);
        EXPECT_EQ(out.direction, 'B');
    }
    EXPECT_FALSE(queue.pop(out));
}

TEST(SPSCQueue, PushAndPopMultiThreaded)
{
    constexpr int N = 1024;
    std::vector<TestSlot> received;
    received.reserve(N);
    llob::SPSCQueue<TestSlot, N> queue;

    std::thread producer([&queue]() {
        for (int i = 0; i < N; ++i) {
            TestSlot slot{i, 5, 'B'};
            while (!queue.push(slot))
                _mm_pause();
        }
    });

    std::thread consumer([&queue, &received]() {
        TestSlot slot{};
        while (static_cast<int>(received.size()) < N) {
            if (queue.pop(slot))
                received.push_back(slot);
            else
                _mm_pause();
        }
    });

    producer.join();
    consumer.join();

    for (int i = 0; i < N; ++i) {
        EXPECT_EQ(received[i].qty,       i);
        EXPECT_EQ(received[i].price,     5);
        EXPECT_EQ(received[i].direction, 'B');
    }
}
