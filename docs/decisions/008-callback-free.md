# ADR-008: Callback-Free API

## Status
Accepted

## Context
Callbacks create control-flow complexity and make debugging harder.

## Decision
Use a pull-based API: callers call `dom_next_event()` to retrieve events. No registration of handler functions.

## Consequences
- Deterministic control flow
- Easier to test and debug
- Caller can process events in any order/pattern