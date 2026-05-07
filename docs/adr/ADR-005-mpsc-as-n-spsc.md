# ADR-005: N-SPSC Composition for MPSC Market Data Aggregation

## Status
Accepted

## Context

**1. Why N-SPSC instead of a true MPSC:**
A true lock-free MPSC (Michael-Scott queue style) requires CAS on the producer side — multiple producers racing to claim the tail. N-SPSC eliminates that entirely: each producer owns its tail exclusively, no contention, no CAS. The consumer is the only one touching the head of each queue. You get MPSC semantics without MPSC complexity.

**2. Consumer polling strategy — priority order, not round-robin:**
The consumer polls N queues. Round-robin is fair but introduces worst-case latency of (N-1) × poll_cost on the highest-priority feed. In a latency-sensitive engine, that's unacceptable — the hot feed (primary exchange) is where matching decisions are made.

Priority polling: check queues in fixed priority order, highest first. The hottest feed is always checked first — minimum latency on the critical path.

**The starvation guard:** under sustained high-priority load, low-priority feeds starve. Mitigation: an epoch counter — after N consecutive high-priority dequeues, force one poll of the remaining queues. This bounds starvation without introducing fairness-induced latency on the hot path.

## Decision

Implementing MPSC Market Data Aggregation as N-SPSC queues with priority polling and a starvation guard.

## Consequences

### Positive
- **Market Data Aggregation OS-free:** No CAS, no thread coordination needed, each producer freely owns its tails exclusively. 
- **Backpressure isolated per feed:** A slow consumer on one high-priority queue doesn't directly affect the other queues. One fast producer cannot starve all others.

### Negative
- **N × queue working set must fit in L1D:** If N is large or capacity is over-provisioned, the combined queue working set spills out of L1D. Each poll then becomes an L2 or L3 hit on a cold head pointer meaning a latency cost on the hot path that is not acceptable.
- **Adding a new feed requires a new SPSC queue and a change to the consumer polling loop — not dynamic:** Meaning at runtime there is no mechanism to add a new feed, a re-deployment will be needed to add a new market data feed.
- **Starvation guard costs a branch and a counter increment on every dequeue:** BTB cost here to be taken into account which is predictable jitter at runtime. 