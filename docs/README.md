# libdom Documentation

## Overview

libdom is a pure C library for parsing HTML and CSS into a stream of typed events. It provides a WASM bridge for DOM mutation instructions. The library is designed for embedding in constrained environments — no syscalls, no callbacks, no global state.

## Quick Start

```c
#include "dom.h"

dom_ctx *ctx = dom_create();
dom_feed_input(ctx, "<p>Hello</p>", 12);

dom_event_t ev;
while (dom_next_event(ctx, &ev)) {
    if (ev.type == DOM_EVENT_ELEMENT_OPEN)
        printf("open: %s\n", ev.data.element.tag);
}

dom_destroy(ctx);
```

## Building

```bash
cmake -B build -S .
cmake --build build
ctest --test-dir build
```

## Key Concepts

- **Events**: All parsing output is delivered as `dom_event_t` structs via a pull-based queue.
- **Embedded strings**: All strings in events are fixed-size char arrays — no heap pointers.
- **Configuration**: Tune queue size, input limits, depth limits, CSS extraction, and strict mode via `dom_config_t`.
- **WASM bridge**: Queue DOM mutation instructions via `dom_wasm_*()` functions.
- **CSS matching**: Simple selector matching via `dom_css_matches()`.

## Further Reading

- [ARCHITECTURE.md](../ARCHITECTURE.md) — State machine, data model, invariants
- [DOMAIN.md](DOMAIN.md) — DOM/HTML/CSS domain overview
- [ADR 001–012](decisions/) — Architectural decisions