#include <gtest/gtest.h>
#include <llob/mpsc_queue.hpp>
#include <thread>
#include <immintrin.h>
#include <vector>

struct alignas(64) TestSlot
{
    int  qty;
    int  price;
    char direction;
};

TEST(MPSCQueue, PushAndPopSingleThread)
{
    llob::MPSCQueue<TestSlot, 4, 1, 4> queue;

    auto producer = queue.get_producer(0);

    TestSlot slot{100, 5, 'B'};
    TestSlot out{};

    EXPECT_FALSE(queue.pop(out));

    for (auto i = 0; i < 3; ++i) {
        EXPECT_TRUE(producer.push(slot));
    }

    for (auto i = 0; i < 3; ++i) {
        EXPECT_TRUE(queue.pop(out));
        EXPECT_EQ(out.qty,       100);
        EXPECT_EQ(out.price,     5);
        EXPECT_EQ(out.direction, 'B');
    }
    EXPECT_FALSE(queue.pop(out));        // empty again

    // wrap-around: indices continue past Capacity
    for (int i = 0; i < 4; ++i)
        EXPECT_TRUE(producer.push(slot));

    for (int i = 0; i < 4; ++i) {
        EXPECT_TRUE(queue.pop(out));
        EXPECT_EQ(out.qty,       100);
        EXPECT_EQ(out.price,     5);
        EXPECT_EQ(out.direction, 'B');
    }
    EXPECT_FALSE(queue.pop(out));
}

TEST(MPSCQueue, PriorityOrderingSingleThread)
{
    llob::MPSCQueue<TestSlot, 4, 3, 4> queue;

    auto producer = queue.get_producer(0);
    auto second_producer = queue.get_producer(1);
    auto third_producer = queue.get_producer(2);

    TestSlot slot{100, 1, 'B'};
    TestSlot second_slot{200, 2, 'B'};
    TestSlot third_slot{300, 3, 'B'};

    third_producer.push(third_slot);
    second_producer.push(second_slot);
    producer.push(slot);

    TestSlot out{};
    EXPECT_TRUE(queue.pop(out));
    // priority queue is first to be popped
    EXPECT_EQ(out.qty,       100);
    EXPECT_EQ(out.price,     1);
    EXPECT_EQ(out.direction, 'B');

    EXPECT_TRUE(queue.pop(out));
    // then second one
    EXPECT_EQ(out.qty,       200);
    EXPECT_EQ(out.price,     2);
    EXPECT_EQ(out.direction, 'B');

    EXPECT_TRUE(queue.pop(out));
    // then third one
    EXPECT_EQ(out.qty,       300);
    EXPECT_EQ(out.price,     3);
    EXPECT_EQ(out.direction, 'B');

    EXPECT_FALSE(queue.pop(out));
}

TEST(MPSCQueue, StarvationGuardFiresSingleThreaded)
{
    llob::MPSCQueue<TestSlot, 8, 2, 4> queue;

    auto producer = queue.get_producer(0);
    auto second_producer = queue.get_producer(1);

    TestSlot slot{100, 1, 'B'};
    for (auto i = 0; i < 5; i++) {
        producer.push(slot);
    }

    TestSlot second_slot{200, 2, 'B'};
    second_producer.push(second_slot);

    TestSlot out{};
    for (auto i = 0; i < 4; i++) {
        EXPECT_TRUE(queue.pop(out));
        EXPECT_EQ(out.qty,       100);
        EXPECT_EQ(out.price,     1);
        EXPECT_EQ(out.direction, 'B');
    }

    // Starvation guard should prevent from the priority queue to be read
    EXPECT_TRUE(queue.pop(out));
    EXPECT_EQ(out.qty,       200);
    EXPECT_EQ(out.price,     2);
    EXPECT_EQ(out.direction, 'B');

    // Going back to the priority queue as the other queue is empty
    EXPECT_TRUE(queue.pop(out));
    EXPECT_EQ(out.qty,       100);
    EXPECT_EQ(out.price,     1);
    EXPECT_EQ(out.direction, 'B');

    EXPECT_FALSE(queue.pop(out));
}

TEST(MPSCQueue, PushAndPopMultiThreaded)
{
    constexpr int N = 8;
    std::vector<TestSlot> received;
    received.reserve(N);
    llob::MPSCQueue<TestSlot, 1, N, 1> queue;

    std::vector<std::thread> producers;
    producers.reserve(N);

    for (auto i = 0; i < N; ++i) {
        auto producer = queue.get_producer(i);
        TestSlot slot{100, i, 'B'};
        producers.push_back(std::thread([producer, slot]() {
            producer.push(slot);
        }));
    }
    

    std::thread consumer([&queue, &received]() {
        TestSlot slot{};
        while (static_cast<int>(received.size()) < N) {
            if (queue.pop(slot)) {
                received.push_back(slot);
            } else {
                _mm_pause();
            }
        }
    });

    for (auto i = 0; i < N; ++i) {
        producers[i].join();
    }
    consumer.join();

    std::sort(received.begin(), received.end(), [](const TestSlot& a, const TestSlot& b) {
        return a.price < b.price;
    });

    for (auto i = 0; i < N; ++i) {
        EXPECT_EQ(received[i].qty,       100);
        EXPECT_EQ(received[i].price,     i);
        EXPECT_EQ(received[i].direction, 'B');
    }
}