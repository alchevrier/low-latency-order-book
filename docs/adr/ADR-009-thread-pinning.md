# ADR-009: Thread Pinning via pthread_setaffinity_np

## Status
Accepted

## Context

**The OS scheduler problem:** by default the OS can migrate your thread to any core at any time, and preempt it when its time slice expires. Both are catastrophic for a latency-sensitive engine:

- **Migration** blows your L1/L2 cache — the new core has none of your hot data. You pay cold-cache latency on every structure until the working set is rebuilt.
- **Preemption** introduces jitter of 10–100µs while the OS context-switches, runs another task, and eventually reschedules your thread. That's orders of magnitude above your target latency.

**Thread pinning via** `pthread_setaffinity_np`: you assign each thread to a specific core and the OS never moves it. Core 1 is the book writer, core 2 is the matcher — permanently. The OS scheduler is effectively bypassed for those cores.

Both threads are spinning (polling queues/seqlock), never blocking, never yielding. From the OS perspective they look like 100% CPU utilisation — which discourages preemption further. Combined with pinning, you get near-deterministic scheduling.

**The startup assert:** on construction you verify the affinity was actually set. If `pthread_setaffinity_np` fails silently, you'd run the entire benchmark on the wrong core without knowing it — the assert catches that.

## Decision

Removing OS intervention on our working threads by thread pinning via `pthread_setaffinity_np` and quick-fail if the affinity was not actually set. 

## Consequences

### Positive
- **OS intervention in thread scheduling of our working threads prevented:** When running our threads we want to make sure our working set, BTB, TLB is in the same core. Rescheduling of threads to another core is costly not only in terms of cache loading but also in branch prediction and table pages. 

### Negative
- **Those cores are fully dedicated — no sharing:** while your pinned threads are spinning at 100% CPU, those cores are unavailable for anything else on the system. 
- **Linux-specific API:** `pthread_setaffinity_np` is POSIX non-portable (the _np suffix means "non-portable") meaning the engine will only be able to be ran on Linux. 
- **Spinning burns power:** a busy-polling thread consumes full CPU power constantly, even when there's no data.
- **Incorrect pinning is silent without the assert:** if `pthread_setaffinity_np ` fails (e.g. the requested core doesn't exist or permissions are denied) and you don't assert, the thread runs on a random core yielding non-predictable p99, p99.99 behaviour. 