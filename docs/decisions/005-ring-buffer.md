# ADR-005: Ring Buffer Event Queue

## Status
Accepted

## Context
Events need to be queued between producer (parser) and consumer (caller).

## Decision
Use a fixed-capacity ring buffer. Default 256 entries. Configurable at creation time.

## Consequences
- O(1) push/pop
- Bounded memory usage
- Overflow emits `DOM_EVENT_QUEUE_OVERFLOW` event
- Caller must drain queue to avoid overflow