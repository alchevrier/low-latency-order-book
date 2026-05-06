# ADR-003: Lazy Cancellation vs Compaction

## Status
Deferred

## Context

When an order is cancelled or modified, what needs to be done on the sorted arrays to the stale entry?

Two approaches:

Lazy cancellation: mark the entry as cancelled (e.g. qty = 0 or a cancelled flag) and leave it in place. Only remove it when you encounter it during matching or a compaction sweep. No immediate array shift.

Compaction: immediately find and remove the entry, shift the array to close the gap (memmove). The array is always clean — no dead entries.

**The tradeoff:**

- **Lazy:** lower write latency, but introduces data-dependent branch on the matcher → BTB misprediction jitter → p99 degradation
- **Compaction:** higher write latency (memmove on cancel), but matcher is branch-free over a clean array → deterministic hot path

## Decision

Deferred. Only ADD events are supported in the current scope. When cancel/modify is introduced, compaction is the preferred direction — the write path is less latency-critical than matching, and a clean array eliminates data-dependent branches and BTB misprediction jitter on the hot path.

## Consequences
