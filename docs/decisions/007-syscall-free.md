# ADR-007: Syscall-Free Design

## Status
Accepted

## Context
libdom targets environments where syscalls may be unavailable or restricted.

## Decision
No file I/O, no memory mapping, no threading primitives, no dynamic loading. All input is caller-provided bytes.

## Consequences
- Portable to bare-metal and WASM
- No error handling for I/O failures
- Caller is responsible for reading files/network