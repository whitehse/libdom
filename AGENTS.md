# AGENTS.md — libdom

## What This Is

**libdom** is a pure C library for DOM/HTML/CSS parsing and representation.

- **Syscall-free**: no I/O, no memory-mapping, no threads.
- **Callback-free**: event-driven via a pull-based ring-buffer queue.
- **Plumbing-first** (ADR 006): core library handles only parsing, serialization, and WASM bridge — no rendering, no JavaScript, no networking.
- **Configurable init**: caller controls queue size, input limits, depth limits, CSS extraction, and strict mode.

## Key Patterns

- All strings in event structs are **embedded char arrays**, never heap pointers.
- The parser is a **byte-at-a-time state machine** (IDLE → TAG_OPEN → TAG_NAME → …).
- Events are **pulled** via `dom_next_event()` — no callbacks.
- WASM bridge functions queue `DOM_EVENT_WASM_INSTRUCTION` events with opcode + target + key/value.
- CSS extraction is opt-in (`cfg.parse_css = 1`) and runs inline when `</style>` closes.

## Architecture

See [ARCHITECTURE.md](ARCHITECTURE.md) for the full state machine diagram, data model, and invariants.

## Coding Rules

1. C11 or older. No VLAs, no `_Generic`, no GNU extensions.
2. Every public function must accept NULL inputs without crashing (graceful degradation).
3. No heap allocations outside `dom_create_with_config` and internal buffers.
4. All tests must pass under Valgrind with zero leaks.
5. `-Wall -Wextra -Wpedantic -Werror` must compile clean.