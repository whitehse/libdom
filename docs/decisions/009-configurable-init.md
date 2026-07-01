# ADR-009: Configurable Initialization

## Status
Accepted

## Context
Different use cases need different limits (queue size, input size, depth).

## Decision
Provide `dom_config_t` with sensible defaults. `dom_create()` uses defaults; `dom_create_with_config()` accepts custom config.

## Consequences
- Simple API for common case
- Full control when needed
- Limits prevent unbounded resource consumption