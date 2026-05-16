# ADR-009: Thread Pinning via pthread_setaffinity_np

## Status
Accepted

## Context

**The OS scheduler problem:** by default the OS can migrate your thread to any core at any time, and preempt it when its time slice expires. Both are catastrophic for a latency-sensitive engine:

- **Migration** blows your L1/L2 cache — the new core has none of your hot data. You pay cold-cache latency on every structure until the working set is rebuilt.
- **Preemption** introduces jitter of 10–100µs while the OS context-switches, runs another task, and eventually reschedules your thread. That's orders of magnitude above your target latency.

**Thread pinning via** `pthread_setaffinity_np`: you assign each thread to a specific logical CPU and the OS scheduler never migrates it. CPU6 is the book writer, CPU4 is the matcher — permanently. The OS scheduler is effectively bypassed for those CPUs.

Both threads are spinning (polling queues/seqlock), never blocking, never yielding. From the OS perspective they look like 100% CPU utilisation — which discourages preemption further. Combined with pinning, you get near-deterministic scheduling.

**Core selection — CPU4 and CPU6:** not all cores are equivalent. On the Intel i5-12400 (6 physical cores, 12 logical CPUs with HT), the selection criteria were:

1. **No unmigratable IRQs on the physical core.** Core 0 (CPU0/1) is where the kernel roots legacy and unmigratable interrupts (APIC, NMI, MCE). Those cannot be moved. CPU4 and CPU6 sit on physical cores 2 and 3 — no such constraint.
2. **Idle HT sibling.** CPU4 and CPU6 are on separate physical cores. Their HT siblings (CPU5 and CPU7) carry no pinned work. This matters because HT siblings share the entire L1/L2 cache, store buffer, and execution ports — a busy sibling continuously evicts your hot cache lines regardless of alignment. Empirically: moving from CPU1/CPU2 (siblings contaminated by OS threads) to CPU4/CPU6 reduced concurrent p99.9 from 24.0 ns to 7.94 ns (−67%) with no code change.
3. **Migratable IRQs steered away.** Interrupts that can be migrated (NIC, PCIe, timer on non-boot CPUs) are redirected away from CPU4 and CPU6 via `/proc/irq/*/smp_affinity_list` before the benchmark runs.

**Core selection methodology:** the following commands were used to identify unmigratable IRQs and verify core topology before selecting CPU4/6.

```bash
# Identify which IRQs are receiving interrupts and on which CPUs
cat /proc/interrupts

# List IRQs that expose an affinity file (i.e. are migratable)
ls /proc/irq/*/smp_affinity_list

# Unmigratable IRQs (NMI, LOC, MCP, ERR, MIS) have no smp_affinity_list —
# the kernel does not expose one because they cannot be moved.
# These are all rooted on CPU0 — confirming core 0 must be excluded.

# Inspect current affinity of all migratable IRQs
grep -r "" /proc/irq/*/smp_affinity_list 2>/dev/null

# Steer all migratable IRQs away from CPU4 and CPU6 before benchmarking
for f in /proc/irq/*/smp_affinity_list; do echo 0-3 | sudo tee $f; done 2>/dev/null

# Verify CPU topology — HT siblings share a physical core
cat /sys/devices/system/cpu/cpu4/topology/thread_siblings_list  # → 4,5
cat /sys/devices/system/cpu/cpu6/topology/thread_siblings_list  # → 6,7
```

CPU4 and CPU6 were selected because neither has unmigratable IRQs anchored to their physical cores, and their HT siblings (CPU5, CPU7) were idle at benchmark time.

**The startup assert:** on construction you verify the affinity was actually set. If `pthread_setaffinity_np` fails silently, you'd run the entire benchmark on the wrong core without knowing it — the assert catches that.

## Decision

Removing OS intervention on our working threads by thread pinning via `pthread_setaffinity_np` and quick-fail if the affinity was not actually set. 

## Consequences

### Positive
- **OS scheduler migration prevented:** the thread stays on its assigned core for its entire lifetime. Working set (cache lines), BTB, and TLB entries built up over time remain valid. Migration would blow all of these — cold-cache penalty on every hot structure until the working set is rebuilt on the new core.
- **Preemption by normal tasks eliminated:** a spinning thread at 100% CPU discourages the scheduler from preempting it in favour of a lower-priority task.

**Important caveat — the kernel still owns the core for interrupts and RCU.** Thread pinning only controls the scheduler. The kernel can and does still deliver timer interrupts (~250/sec at `CONFIG_HZ=250`), RCU callbacks, and hardware IRQs to pinned cores. These are the source of the p99.9 jitter observed in benchmarks (2.57 ns median → 7.94 ns p99.9). Eliminating this residual jitter requires `nohz_full` + `rcu_nocbs` + `isolcpus` (kernel parameters) or a `PREEMPT_RT` kernel — none of which are applied here, as they interact poorly with Google Benchmark's timer and are unavailable on a stock Ubuntu kernel.

### Negative
- **Those cores are fully dedicated — no sharing:** while your pinned threads are spinning at 100% CPU, those cores are unavailable for anything else on the system. 
- **HT not disabled — siblings remain live:** CPU5 and CPU7 (HT siblings of CPU4 and CPU6) are still active and schedulable by the kernel. Any work the OS places on them shares the physical core's L1/L2 cache, store buffer, and execution ports with our pinned threads. We mitigate this by choosing cores with idle siblings and steering IRQs away, but we do not eliminate it. Disabling HT in BIOS would remove this risk entirely at the cost of halving logical CPU count.
- **Linux-specific API:** `pthread_setaffinity_np` is POSIX non-portable (the _np suffix means "non-portable") meaning the engine will only be able to be ran on Linux. 
- **Spinning burns power:** a busy-polling thread consumes full CPU power constantly, even when there's no data.
- **Incorrect pinning is silent without the assert:** if `pthread_setaffinity_np ` fails (e.g. the requested core doesn't exist or permissions are denied) and you don't assert, the thread runs on a random core yielding non-predictable p99, p99.99 behaviour. 