# ADR-006: Price Level Indexing — Fixed-Point Ticks and Sorted Contiguous Array

## Status
Accepted

## Context

**1. Eliminating float — why it matters:**
Prices arrive as floating-point from the feed. IEEE 754 makes equality and ordering unreliable — 0.1 + 0.2 != 0.3. In a price comparison (bid >= ask) a rounding error can cause a false match or a missed match. In a sorted array, FP noise can corrupt the sort invariant. These are correctness bugs, not just performance issues.

**2. Fixed-point ticks — the solution:**
Convert once at the feed boundary: int64_t ticks = std::llround(raw_price / tick_size). All internal representation is integer. Comparisons are exact, sorting is exact, no epsilon needed anywhere.

**3. Price priority rule — the indexing consequence:**
The sorted array must maintain price priority at all times — bids descending, asks ascending. When a new level arrives, you binary search for its position and memmove to insert. O(n) insert but n is small (20–50 active levels), and memmove over a cache-resident array of int64_t is a single fast memcpy under the hood.

## Decision

Implementing price level indexing using fixed-point ticks and sorted contiguous array. Conversion of prices happens once received and is then dealt with as integer for the rest of the engine.

## Consequences

### Positive

- **Maintaining the sorted array is straightforward:** We can use standard libraries tools without worrying about possible edge cases due to using floating points. Code is much simpler to understand and comparing int is cheap CPU instruction wise.
- **Insertions are cheap:** Negligible at active level counts of 20–50, dominated by cache latency not algorithmic cost.

### Negative

- **When price matching we need to keep track of indexes rather than struct:** This is a place where bugs might happen due to developers being usually used to deal with struct. 
- **Correctness is heavily correlated to `tick_size`:** Correctness depends entirely on `tick_size` being accurate at the conversion boundary — a wrong value silently corrupts all internal prices