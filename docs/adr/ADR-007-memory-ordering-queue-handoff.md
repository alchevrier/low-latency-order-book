# ADR-007: Memory Ordering on Queue Handoff

## Status
Accepted

## Context

The problem: the CPU and compiler are free to reorder instructions for performance. On a queue handoff, you need to guarantee that when the consumer sees the updated sequence/tail pointer, it also sees the payload that was written before it.

The three orderings in play:

`memory_order_relaxed` — no ordering guarantees at all. Safe only for operations where you don't care about what other threads see relative to it. Example: incrementing a stats counter.

`memory_order_release` (producer side, on the tail/sequence write) — guarantees that all writes before this point are visible to any thread that does an acquire on the same variable. This is how the payload write is "published" to the consumer.

`memory_order_acquire` (consumer side, on the tail/sequence read) — guarantees that all reads after this point see everything that was written before the corresponding release. This is how the consumer "receives" the payload safely.

**Concepts applied to SPSC:**

- Producer writes payload → memory_order_relaxed (no one is watching yet)
- Producer writes tail → memory_order_release (publishes everything above it)
- Consumer reads tail → memory_order_acquire (synchronises with the release)
- Consumer reads payload → memory_order_relaxed (already safe, acquire happened)

## Decision

The release/acquire pair on the tail pointer is the only synchronisation needed. No `memory_order_seq_cst`, no fences — those would add unnecessary cost. This also only works in the context of having one thread as a consumer and one thread as a producer. 

## Consequences

### Positive

- **No OS intervention nor flushes of cache needed:** Here we only use CPU instructions to ensure correctness, the OS never intervenes here and we have no write-side RFO (Invalidation done on the producer cache-line) as well because the consumer will only read the index/payload committed by the producer. 
- **Acquire/release as compiler and CPU reordering guards**: The acquire/release pair prevents the compiler and CPU from reordering across the handoff. On x86, the CPU already has strong ordering (TSO), so the acquire/release only costs a compiler barrier on x86. On ARM it would generate actual fence instructions.


### Negative

- **Tied to single-threadness:** This model only works with exactly one producer and one consumer. Multiple producers require a different queue design (see ADR-005).