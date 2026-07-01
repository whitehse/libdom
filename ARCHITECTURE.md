# Architecture — libdom

## Overview

libdom is a pure C library that parses HTML (and optionally CSS) into a stream of typed events. It exposes a WASM bridge for issuing DOM mutation instructions. The library is designed to be embedded — no syscalls, no callbacks, no global state.

## Parser State Machine

```
         ┌────────┐
         │  IDLE  │
         └───┬────┘
             │ '<'
             ▼
        ┌──────────┐
        │ TAG_OPEN │─── '/' ──→ TAG_CLOSE (closing tag)
        └───┬──────┘
            │ alpha
            ▼
        ┌──────────┐
        │ TAG_NAME │─── space ──→ TAG_ATTR_NAME
        └───┬──────┘
            │ '>'
            ▼
        ┌──────────────┐
        │ emit OPEN/   │──→ back to IDLE
        │ SELF_CLOSE   │
        └──────────────┘

  TAG_ATTR_NAME ── '=' ──→ TAG_ATTR_VALUE
  TAG_ATTR_VALUE ── quote/close ──→ TAG_ATTR_NAME or emit+IDLE

  IDLE + non-space → TEXT_CONTENT
  TEXT_CONTENT + '<' → flush text → TAG_OPEN

  '<!' → DOCTYPE (skip until '>') or COMMENT (<!-- ... -->)
```

### Void Elements (self-closing)

`br`, `hr`, `img`, `input`, `meta`, `link`, `area`, `base`, `col`, `embed`, `source`, `track`, `wbr`

These are recognized by tag name and automatically emit `ELEMENT_SELF_CLOSE`.

## Data Model

All data lives in **fixed-size embedded arrays** within event structs:

| Struct | Key fields |
|---|---|
| `dom_element_t` | `tag[64]`, `attrs[64]` of `dom_attr_t`, `node_id`, `parent_id`, `depth` |
| `dom_attr_t` | `name[128]`, `value[4096]`, `has_value` |
| `dom_text_t` | `text[8192]` |
| `dom_css_rule_t` | `selector[256]`, `property[128]`, `value[1024]` |
| `dom_event_t` | `type` + union of above + `error` |
| `dom_wasm_instruction_t` | `op`, `target_node_id`, `key[128]`, `value[4096]` |

**No pointers in event structs.** This makes events safe to copy, queue, and serialize without ownership concerns.

## Event Queue

A fixed-capacity ring buffer (`ring_t`). Default 256 events. Overflow emits `DOM_EVENT_QUEUE_OVERFLOW`. Caller drains via `dom_next_event()`.

## WASM Bridge

WASM mutation operations (`set_attribute`, `remove_element`, etc.) queue `DOM_EVENT_WASM_INSTRUCTION` events. The event payload encodes the operation opcode, target node ID, and key/value strings. A WASM runtime on the consuming side interprets these instructions.

## CSS Extraction

When `cfg.parse_css = 1`, the parser accumulates text content inside `<style>` blocks. On `</style>`, the accumulated CSS text is split into `dom_css_rule_t` events (selector + property + value). This is a simplified extractor — it handles `selector { property: value; }` patterns but does not implement a full CSS parser.

## Invariants

1. **Depth tracking**: `dom_depth()` always reflects the current nesting level (0 at document root).
2. **No NULL pointers in events**: all char arrays are zero-terminated.
3. **Monotonic event count**: `dom_event_count()` never decreases (even after reset it goes to 0).
4. **Max depth enforcement**: parsing aborts with an error event when depth exceeds `cfg.max_dom_depth`.
5. **Max input enforcement**: `dom_feed_input` returns -1 when cumulative input exceeds `cfg.max_input_size`.

## Deliberate Absences

- **No rendering**: libdom does not paint, layout, or rasterize.
- **No JavaScript**: no script execution, no DOM mutation from JS.
- **No network I/O**: all input is caller-provided bytes.
- **No CSS cascade/specificity**: matching is limited to `dom_css_matches()` with simple selectors (tag, `.class`, `#id`).
- **No DOM tree**: libdom produces a flat event stream, not a linked tree structure.