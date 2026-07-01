# ADR-002: Embedded Strings (No Pointers in Events)

## Status
Accepted

## Context
Event structs need to carry strings (tag names, attribute values, text content).

## Decision
Use fixed-size char arrays inside event structs. No heap-allocated string pointers.

## Consequences
- Events are trivially copyable
- No ownership/lifetime issues
- Size limits on strings (tag: 64, attr name: 128, attr value: 4096, text: 8192)