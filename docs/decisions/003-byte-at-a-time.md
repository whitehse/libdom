# ADR-003: Byte-at-a-Time State Machine

## Status
Accepted

## Context
HTML parsing requires a tokenizer that handles partial input.

## Decision
Implement a character-by-character state machine. Each byte fed via `dom_feed_input()` triggers a state transition.

## Consequences
- Supports incremental/streaming parsing
- No lookahead buffer needed (except for comment detection)
- Simple and auditable code