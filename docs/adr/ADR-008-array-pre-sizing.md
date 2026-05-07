# ADR-008: Array Pre-Sizing via Market Analysis

## Status
Accepted

## Context

**Why pre-size at all:** dynamic resizing (e.g. `std::vector` realloc) during live matching is catastrophic — it moves the array, invalidates all pointers, and triggers a heap allocation on the hot path. Unacceptable.

**Why not just over-provision massively:** if you reserve 10,000 levels and only 30 are ever active, you've blown your L1D/L2 working set budget for no reason. Every poll of `prices[0]` now competes with 79KB of dead memory for cache space.

**The market analysis angle:** you size based on observed or estimated active level counts. For a typical equity instrument, 20–50 live price levels per side is realistic under normal conditions. Spike conditions (circuit breakers, earnings) might push to 100–200. You size for the spike, not the average, but you size deliberately — not arbitrarily.

**The concrete math:** 200 levels × 8 bytes × 2 sides (bid/ask) × 2 arrays (price + qty) = 6,400 bytes per symbol. Comfortably fits in L1D (32KB). Even for 10 symbols: 64KB — still in L2.

## Decision

Pre-size arrays at construction time based on expected maximum active levels, derived from market analysis, with no runtime reallocation.

## Consequences

### Positive
- **Actively traded instruments are almost always hot:** Zero heap allocation on the hot path — array is constructed once, all subsequent accesses are pointer arithmetic into a fixed buffer.

### Negative
- **Wrong size crashes:** If your active levels exceed the pre-sized capacity at runtime, you overflow the array. There's no safety net since there's no reallocation. You must get the sizing right, or you have undefined behaviour. 
- **This design limits the number of actively traded instruments:** In order to stay within L1D-L2 you must accept that some instruments will be located in L3. Trading the cold instruments has consequences on the active ones in L2 which is natural and expected jitter at runtime. On the other hand, you can't have unbounded symbols without a different architecture (pool of books, eviction strategy).