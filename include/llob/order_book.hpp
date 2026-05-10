#pragma once
#include <llob/market_data_event.hpp>
#include <cstddef>

namespace llob
{

// SOA order book for a single symbol.
// Separate sorted int64_t price-tick arrays per side.
// Bids: sorted descending — bids_[0] is best bid.
// Asks: sorted ascending  — asks_[0] is best ask.
// Synchronised via seqlock for concurrent reader (matching thread).

template <std::size_t MaxLevels>
class OrderBook
{
public:
    // TODO: implement
private:
};

} // namespace llob
