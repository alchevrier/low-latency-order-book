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

**The memory ordering**: the writer uses release on the second counter increment. The reader uses acquire on both counter reads. Same pattern as ADR-007 — release/acquire pair is the only synchronisation needed.

**Why not a mutex:** a mutex puts the reader to sleep if the writer holds it. Sleep = OS intervention = microseconds of jitter. Unacceptable. The seqlock retry is nanoseconds.

## Decision

Using Seqlock for SOA Order Book Reader-Writer Synchronisation. 

## Consequences

### Positive

- **Using the memory ordering for correctness over OS mechanism:** This allows us to stay in the acceptable range (ns of jitter) of low-latency while being correct. 
- **Read path wait-free on the happy-path:** On the happy path, the matcher never stalls, it just reads. An example of happy-path is a volatile market where traders are usually making offers far from the market which is far from the data we usually read in our cache line. 

### Negative

- **Retry under write pressure:** if the book writer is updating frequently (frequent top-of-book updates), the matcher may retry many times before getting a clean read. Under extreme conditions this could spin for multiple microseconds — rare but worth acknowledging. Retry pressure is highest during market open/close auctions and news events when top-of-book updates are most frequent — this manifests as p99.9 latency spikes, not average-case degradation.
- **Reader reads potentially stale data mid-retry:** during the retry window the matcher is re-reading data that may have changed multiple times. The seqlock guarantees consistency but not freshness — the matched price is the book state at the moment of a successful read, which may already be one update behind. For real-time analysis this is acceptable and expected.