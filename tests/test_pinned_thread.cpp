#include <gtest/gtest.h>
#include <llob/pinned_thread.hpp>
#include <atomic>

TEST(PinnedThread, RunAndVerifyAffinity)
{
    std::atomic<int> actual_core{-1};
    llob::PinnedThread t(1, [&actual_core]() {
        actual_core.store(sched_getcpu(), std::memory_order_relaxed);
    });
    t.join();
    EXPECT_EQ(actual_core.load(), 1);
}