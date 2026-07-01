# ADR-006: Plumbing-Only Library

## Status
Accepted

## Context
libdom should be focused and embeddable.

## Decision
libdom handles only parsing, serialization, and WASM bridge. No rendering, no JavaScript, no networking, no layout engine.

## Consequences
- Small codebase, easy to audit
- No OS dependencies
- Suitable for embedded/WASM/sandboxed environments