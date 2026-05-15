# ADR-010: Seqlock for SOA Order Book Reader-Writer Synchronisation

## Status
Accepted

## Context

**How a seqlock works:**

The writer:

1- Increments a sequence counter (odd = write in progress)
2- Writes the data
3- Increments the counter again (even = write complete)

The reader:

1- Reads the counter — if odd, a write is in progress, spin and retry
2- Reads the data
3- Reads the counter again — if it changed, a write happened mid-read, retry from step 1
4- If counter unchanged and even — data is consistent, done

**Why it's perfect for our case:**

- **Single writer** (book writer thread) — no contention on the write side, no CAS needed
- **Single reader** (matching thread) — no contention on the read side either
- **Read is free when no write is happening** — just two counter reads wrapping the data read, both likely cache-hot. Sub-nanosecond overhead on the happy path
- **Write doesn't block the reader** — the reader just retries. No mutex, no OS, no sleep

**The memory ordering**: the writer uses release on the second counter increment. The reader uses acquire on both counter reads. Same pattern as ADR-007 — release/acquire pair is the only synchronisation needed. On x86 (TSO — Total Store Order), release and acquire compile to plain `MOV` instructions — no `MFENCE`, no `LOCK` prefix. The hardware enforces the ordering. This is why the happy-path seqlock cost is sub-nanosecond on x86 (confirmed by benchmark: median 2.49 ns including the data read itself). On ARM, `STLR`/`LDAR` would be emitted instead — still no full fence, but explicit load/store ordering instructions.

**Why not a mutex:** a mutex puts the reader to sleep if the writer holds it. Sleep = OS intervention = microseconds of jitter. Unacceptable. The seqlock retry is nanoseconds.

**Why not RCU or hazard pointers — correctness, not just performance:**

RCU (Read-Copy-Update) and hazard pointers are designed to let readers complete on *old* data safely. That is their entire purpose: defer reclamation until all readers have finished with the previous version. A reader can legitimately return a value that is already logically superseded. This is safe from a memory standpoint but **inadmissible in a market data context**.

The seqlock protocol enforces a freshness invariant: a reader either completes with a consistent, current view of the order book, or it detects a concurrent write and retries. It cannot return a superseded price to the caller without the caller knowing.

This has direct regulatory implications:

- **US — Reg NMS §242.611 (Order Protection Rule):** Trading centers must maintain policies "reasonably designed to prevent trade-throughs" — i.e. executing at a price inferior to a protected quotation (NBBO). Acting on a stale best bid while a better protected quote exists is a trade-through. Consequence: FINRA Rule 11892 allows the trade to be declared **null and void**.
- **EU — MiFID II Article 27(1) (Directive 2014/65/EU):** Investment firms must take "all sufficient steps to obtain [...] the best possible result for their clients", including price. Trading on a superseded best bid violates the best execution obligation.

The seqlock retry loop is not just a latency optimisation — it is the mechanism that enforces the market data freshness invariant required for compliance. RCU would be unsafe here regardless of its performance characteristics.

## Decision

Using Seqlock for SOA Order Book Reader-Writer Synchronisation. 

## Consequences

### Positive

- **Using the memory ordering for correctness over OS mechanism:** This allows us to stay in the acceptable range (ns of jitter) of low-latency while being correct. 
- **Read path wait-free on the happy-path:** On the happy path, the matcher never stalls, it just reads. An example of happy-path is a volatile market where traders are usually making offers far from the market which is far from the data we usually read in our cache line. 

### Negative

- **Retry under write pressure:** if the book writer is updating frequently (frequent top-of-book updates), the matcher may retry many times before getting a clean read. Under extreme conditions this could spin for multiple microseconds — rare but worth acknowledging. Retry pressure is highest during market open/close auctions and news events when top-of-book updates are most frequent — this manifests as p99.9 latency spikes, not average-case degradation.
- **Retry under write pressure causes repeated reads:** during the retry window the matcher re-reads data that may have changed multiple times. All failed reads are discarded — the seqlock guarantees that only a consistent, current snapshot is returned to the caller. Note: the successful read reflects the book state at the moment the retry completes, not at the moment the original call was made. Under high update rates (open/close auctions, news events) several retries may occur before a clean read lands, manifesting as p99.9 latency spikes.