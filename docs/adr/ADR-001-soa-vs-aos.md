# ADR-001: SOA vs AOS for Order Book Storage

## Status
Accepted

## Context

An order book is two sorted lists. The **bid side** - buyers ranked by price descending (highest willingness to pay at the top). The **ask side** - sellers ranked by price ascending (cheapest offer at the top). The top of each side is the **best bid** and **best ask**. Conceptually the order book is the current state of liquidity, live snapshot of all unmatched interest at every price level at this instant. 

The critical operations made during order matching are `best_ask(symbol)` and `best_bid(symbol)` - the first element of each sorted array. Matching is just `bid_price >= ask_price`.

## Decision

**Why using AOS is not relevant:**

With AOS each element is { price, qty, symbol, side }. To read `price`, you load the full struct into a cache line - you're paying for `qty`, `symbol`, `side` even though the matcher only needs price for the comparison. At 64 bytes per cache line, you get far fewer price levels per line. 

**Why SOA wins:**

With SOA you have a contiguous `int64_t prices[]` per side. Cache lines are packed with nothing but prices. Best price is `prices[0]` - one load and you've already got the next ~7 levels in the same cache line for free. `qty` is only fetched after the match is confirmed, on demand from its own array. 

**The compounding factor:** when the book writer is updating and the matcher is reading under seqlock, retry is possible but re-read is cheap since the cache line is still hot and wasn't the one affected by the write. Pure price array reads are the smallest possible footprint. 

## Consequences

### Positive
- **Cache invalidation on writer change eliminated:** Since `prices[]` and `qtys[]` will be on different cache lines a change made by the write in `qtys[]` will not affect `prices[]` -> no cache miss
- **Prefetcher friendly on single access**: the hardware prefetcher will naturally on order matching pre-fetch the needed cache lines needed as we will be striding across a single array. CPU is optimized for this use case.

### Negative
- **Code complexity:** accessing a single "order" now means indexing into multiple arrays - `prices[i], qtys[i], sides[i]`. Logic that needs the full order is more verbose.
- **Prefetcher confusion on mixed access:** the hardware prefetcher is great at sequential access on one array. If you ever need to read `price` and `qty` together, you're striding across two separate arrays - two prefetch streams, slightly harder for the CPU to predict.
- **No natural encapsulation:** you lose the ability to reason about an order as a unit. Bugs like mismatched index (`prices[i]` but `qtys[j]`) are harder to catch at compile time. 