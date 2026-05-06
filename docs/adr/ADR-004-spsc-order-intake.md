# ADR-004: SPSC Queue for Order Intake

## Status
Accepted

## Context

The **time priority** rule in exchanges mandates that the first order that comes in needs to be the first one to be served therefore we HAVE to use FIFO queue for order intake. 

Why SPSC specifically:

- There is exactly one producer: the order intake source (single trading strategy / single order entry point)
- There is exactly one consumer: the matching thread

With a single producer and single consumer guaranteed, SPSC is the optimal choice — no CAS, no lock, no contention. The head and tail are owned exclusively by one thread each. The only synchronisation needed is a release/acquire pair on the sequence counter — the minimum possible memory ordering cost.

Any broader queue (MPSC, MPMC) would add CAS loops or locks that are entirely unnecessary overhead given the 1:1 topology. Over-engineering the queue type is itself a latency cost.

## Decision

SPSC of fixed-size ring capacity will be used for order intake. If the producer outpaces the consumer, the queue fills and we have to drop or block. It is the caller's responsibility to handle backpressure.

## Consequences

### Positive
- **No OS mechanism involved for correctness:** Here to have a correct FIFO queue, we only need to track two indexes and use CPU instruction to prevent instruction re-ordering (in C++ memory_order_acquire and memory_order_release). No OS intervention, just need CPU instructions, one thread for producing another one for consuming

### Negative
- **Strict 1:1 topology:** if the intake needs a second producer (second strategy, risk check injecting orders) we would need to re-design. 