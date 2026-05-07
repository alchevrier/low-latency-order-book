# low-latency-order-book

C++23 low-latency order book engine — lock-free SPSC/MPSC queues, SOA order book, pinned threads, benchmarked under concurrent market data and matching load.

## Architecture

```
Market Data Feed 1 ─┐
Market Data Feed 2 ─┤─ N-SPSC (MPSC) ──► Book Writer Thread (core 1) ──► SOA Order Book
Market Data Feed N ─┘                                                         │ seqlock
                                                                               ▼
Order Intake ──────────── SPSC ─────────────────────────────────────► Matching Thread (core 2)
```

- **N-SPSC composition** for MPSC: each producer owns its SPSC queue; consumer polls in priority order with a starvation guard. See [ADR-005](docs/adr/ADR-005-mpsc-as-n-spsc.md).
- **SOA order book**: separate sorted `int64_t` price-tick arrays per side (bids desc, asks asc), pre-sized via market analysis. See [ADR-001](docs/adr/ADR-001-soa-vs-aos.md), [ADR-002](docs/adr/ADR-002-separate-bid-ask-arrays.md), [ADR-006](docs/adr/ADR-006-price-level-indexing.md), [ADR-008](docs/adr/ADR-008-array-pre-sizing.md).
- **Seqlock** synchronises the single writer (book thread) with the single reader (matching thread). See [ADR-010](docs/adr/ADR-010-seqlock-soa-sync.md).
- **PinnedThread**: `std::thread` + `pthread_setaffinity_np`, asserted on startup. See [ADR-009](docs/adr/ADR-009-thread-pinning.md).
- **Release/acquire memory ordering** on all queue handoffs — no fences, no OS, no seq_cst. See [ADR-007](docs/adr/ADR-007-memory-ordering-queue-handoff.md).

## Toolchain

| Tool   | Version  |
|--------|----------|
| C++    | 23       |
| CMake  | 3.28     |
| Conan  | 2.27.1   |
| GCC    | 13.3.0   |
| GTest  | latest   |
| GBench | latest   |

## Architecture Decision Records

| ADR | Title | Status |
|-----|-------|--------|
| [ADR-001](docs/adr/ADR-001-soa-vs-aos.md) | SOA vs AOS for Order Book Storage | Accepted |
| [ADR-002](docs/adr/ADR-002-separate-bid-ask-arrays.md) | Separate Sorted Arrays for Bids and Asks | Accepted |
| [ADR-003](docs/adr/ADR-003-lazy-cancellation-vs-compaction.md) | Lazy Cancellation vs Compaction | Deferred |
| [ADR-004](docs/adr/ADR-004-spsc-order-intake.md) | SPSC Queue for Order Intake | Accepted |
| [ADR-005](docs/adr/ADR-005-mpsc-as-n-spsc.md) | N-SPSC Composition for MPSC Market Data Aggregation | Accepted |
| [ADR-006](docs/adr/ADR-006-price-level-indexing.md) | Price Level Indexing — Fixed-Point Ticks and Sorted Contiguous Array | Accepted |
| [ADR-007](docs/adr/ADR-007-memory-ordering-queue-handoff.md) | Memory Ordering on Queue Handoff | Accepted |
| [ADR-008](docs/adr/ADR-008-array-pre-sizing.md) | Array Pre-Sizing via Market Analysis | Accepted |
| [ADR-009](docs/adr/ADR-009-thread-pinning.md) | Thread Pinning via pthread_setaffinity_np | Accepted |
| [ADR-010](docs/adr/ADR-010-seqlock-soa-sync.md) | Seqlock for SOA Order Book Reader-Writer Synchronisation | Accepted |
