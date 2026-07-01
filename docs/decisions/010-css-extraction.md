# ADR-010: CSS Extraction from Style Blocks

## Status
Accepted

## Context
Users may want CSS rules alongside HTML events.

## Decision
When `cfg.parse_css = 1`, accumulate text inside `<style>` blocks and emit `DOM_EVENT_CSS_RULE` events on `</style>`. Simple property:value splitting.

## Consequences
- No separate CSS parser library needed
- Limited to `selector { property: value; }` patterns
- No cascade/specificity computation