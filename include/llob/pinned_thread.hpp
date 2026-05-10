#pragma once
#include <thread>
#include <pthread.h>
#include <cassert>

namespace llob
{

// Wraps std::thread with pthread_setaffinity_np.
// Asserts on startup that affinity was successfully set.

class PinnedThread
{
public:
    // TODO: implement
private:
    std::thread thread_;
};

} // namespace llob
