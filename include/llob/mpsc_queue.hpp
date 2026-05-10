#pragma once
#include <llob/spsc_queue.hpp>
#include <llob/market_data_event.hpp>

namespace llob
{

// N-SPSC MPSC: one SPSCQueue per producer, consumer polls in priority order.
// Priority order is determined by the order queues are registered.
// Starvation guard: after StarvationThreshold consecutive high-priority
// dequeues, the consumer forces one poll of all lower-priority queues.

template <std::size_t Capacity, std::size_t StarvationThreshold = 64>
class MPSCQueue
{
public:
    // TODO: implement
private:
};

} // namespace llob
