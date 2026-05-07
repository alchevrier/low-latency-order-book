# ADR-002: Separate Sorted Arrays for Bids and Asks

## Status
Accepted

## Context
Exchange price-time priority rules dictate that an incoming order must be matched against the best available price first — highest bid, lowest ask. This is not a design choice, it's a market rule (price priority, then time priority at the same price level).

That rule directly drives the data structure requirement:

- Bids must be sorted descending — best_bid = bids[0]
- Asks must be sorted ascending — best_ask = asks[0]

**Why separate arrays (not one combined structure)?**

Bids and asks have opposite sort orders — a single array can't serve both correctly without branching on side. Separate arrays mean each is independently sorted, the matcher reads `bids[0]` and `asks[0]` directly with no conditional logic, and the book writer inserts into only the relevant array (no cross-side interference, no unnecessary cache line traffic on the other side).

## Decision

To define the current market we will have two sorted arrays for bids and asks for type `int64_t` per symbol each sorted in an order where the top of the market is accessible via `bids[0]` and `asks[0]`.

**The decision is therefore forced by the rules, not a preference.** 

## Consequences

### Positive
- **We always have the best bid and best ask in a single access:** As soon as we receive an order we naturally need to take the first element of each array for price matching
- **Book Writer less likely to invalidate the cache line used:** updated to the order book by orders far from the market (far from best price) insert into the middle/tail of the array meaning the cache line holding `[0]` (best price) is unaffected.

### Negative
- **Use of two arrays means keeping both in L2 working set:** Each array is `N * 8 bytes`, two arrays per side doubles the working set for prices alone. Plus `qtys[]` arrays on top, it all needs to fit in L2 (512KB - 1MB on my machine). We will need to do what ADR-008 prescribes which is pre-sizing via market analysis by knowing our levels count per side we can size it properly to fit the L2 working set. Over-provisioning means spilling possibly to L3 and in the hot-path these are latency levels that are not acceptable. 
- **When market is frequently made cache line will be invalidated frequently:** Update to the order book by orders that makes a new market (better than the best price) insert into the beginning of the array and therefore is affecting `[0]`, since the current cache line needs to be updated a Request For Ownership will need to happen (cost is ~20-60 cycles for the writer and on the read side it would be ~200 cycles if spilled to L3). 
- **Spread calculation requires access to both arrays:** Meaning two cache lines minimum to calculate the spread (`best_ask` - `best_bid`), cost is negligible but for example making a new market on both sides means two cache line invalidated (amortised once by the reader)