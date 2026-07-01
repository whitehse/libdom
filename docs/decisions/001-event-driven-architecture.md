# ADR-001: Event-Driven Architecture

## Status
Accepted

## Context
libdom needs to communicate parsing results to callers without coupling them to internal state.

## Decision
Use a pull-based event queue (ring buffer) rather than callbacks or a persistent DOM tree. Callers call `dom_next_event()` to retrieve events.

## Consequences
- Callers control consumption rate
- No callback registration overhead
- Events are value types (copyable, queueable)