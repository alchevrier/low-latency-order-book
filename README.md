# low-latency-order-book

C++23 low-latency order book engine — lock-free SPSC/MPSC queues, SOA order book, pinned threads, benchmarked under concurrent write and read load.

## Intended architecture

```
Market Data Feed 1 ─┐
Market Data Feed 2 ─┤─ N-SPSC (MPSC) ──► Book Writer Thread (core 1) ──► SOA Order Book
Market Data Feed N ─┘                                                         │ seqlock
                                                                               ▼
Order Intake ──────────── SPSC ─────────────────────────────────────► Matching Thread (core 2)
```

Each component is implemented and unit-tested independently. The end-to-end pipeline above is the intended integration target — components are designed to compose this way. The benchmark does not exercise the full pipeline; it isolates and quantifies the critical-path bottleneck: seqlock-protected SOA access under concurrent write pressure.

## Components

| Component | File | Notes |
|---|---|---|
| SPSC queue | `include/llob/spsc_queue.hpp` | Single-producer single-consumer, cache-line aligned, release/acquire. [ADR-004](docs/adr/ADR-004-spsc-order-intake.md), [ADR-007](docs/adr/ADR-007-memory-ordering-queue-handoff.md) |
| MPSC queue | `include/llob/mpsc_queue.hpp` | N-SPSC composition; each producer owns its queue, consumer polls with starvation guard. [ADR-005](docs/adr/ADR-005-mpsc-as-n-spsc.md) |
| SOA order book | `include/llob/order_book.hpp` | Separate sorted `int64_t` price-tick arrays per side (bids desc, asks asc), pre-sized. [ADR-001](docs/adr/ADR-001-soa-vs-aos.md), [ADR-002](docs/adr/ADR-002-separate-bid-ask-arrays.md), [ADR-006](docs/adr/ADR-006-price-level-indexing.md), [ADR-008](docs/adr/ADR-008-array-pre-sizing.md) |
| Seqlock | embedded in `order_book.hpp` | Writer increments sequence counter; reader retries on odd count. Guarantees freshness. [ADR-010](docs/adr/ADR-010-seqlock-soa-sync.md) |
| PinnedThread | `include/llob/pinned_thread.hpp` | `std::thread` + `pthread_setaffinity_np`, asserted on startup. [ADR-009](docs/adr/ADR-009-thread-pinning.md) |

23 unit tests across 5 test suites covering all components: SPSC queue, MPSC queue, order book (bid/ask invariants, capacity, cross-thread), pinned thread, and concept constraints.

### Benchmark setup

The benchmark wires two pinned threads directly to the SOA order book to measure seqlock overhead under live write pressure — without the queues, which are a separate concern:

```
Writer thread (CPU6) ──► order_book.add()  ─┐
                                             │ seqlock
Reader thread (CPU4) ──► order_book.best_bid() ◄─┘
```

## Benchmarks

Measured on an Intel i5-12400 (consumer desktop, HT enabled, shared OS — not a tuned server), GCC 13.3.0 `-O3`.  
Benchmark threads pinned via `pthread_setaffinity_np`. `performance` governor set. No `isolcpus`.

- Reader (matcher) pinned to **CPU4**, writer (book) pinned to **CPU6** — separate physical cores with idle HT siblings.

### Matching latency — `best_bid()` read from SOA order book

| Scenario | Median | p99 | p99.9 | p99.99 |
|---|---|---|---|---|
| Isolated (no concurrent writes) | 2.50 ns | 7.22 ns | 9.01 ns | 36.7 ns |
| Concurrent (book writer CPU6, matcher CPU4) | 2.57 ns | 3.33 ns | 7.94 ns | 7.94 ns |

**Seqlock overhead under live write pressure: < 1 ns at median.** Tail divergence is OS scheduling jitter — isolated runs 10× more repetitions (10,000 vs 1,000), capturing more rare preemption events. The median is the meaningful comparison.

### HT interference — `alignas(64)` is not enough

Cache-line alignment (`alignas(64)`) prevents false sharing *between* cores but does nothing for HT sibling interference *within* a core: two logical threads on the same physical core share L1, L2, execution ports, store buffer, and load buffer. Any OS activity on the sibling evicts hot cache lines regardless of alignment.

Observed when moving the reader/writer from CPU1/CPU2 (HT siblings of cores already loaded by the OS) to CPU4/CPU6 (idle physical cores), with code and data layout unchanged:

| Pinning | Concurrent p99.9 | Δ |
|---|---|---|
| CPU1 / CPU2 (HT-contaminated) | 24.0 ns | baseline |
| CPU4 / CPU6 (clean physical cores) | 7.44 ns | **−69%** |

The fix is not alignment — it is a dedicated physical core. `alignas`, `isolcpus`, and a dedicated core address three separate layers of cache interference.

### Memory profile (Valgrind massif)

- **Zero heap allocation in the hot path** — SOA arrays are stack/static, seqlock counter embedded in the object.
- Peak heap: 14.6 MB — entirely Google Benchmark framework (result storage, statistics).

## Toolchain

| Tool   | Version  |
|--------|----------|
| C++    | 23       |
| CMake  | 3.28     |
| Conan  | 2.27.1   |
| GCC    | 13.3.0   |
| GTest  | latest   |
| GBench | latest   |

## Build

```bash
conan install . --output-folder=build --build=missing
source build/conanbuild.sh
cmake -B build -DCMAKE_PREFIX_PATH="$(pwd)/build" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Run tests

```bash
./build/tests/tests
```

### Run benchmarks

```bash
./build/benchmarks/bench_matching
```

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
