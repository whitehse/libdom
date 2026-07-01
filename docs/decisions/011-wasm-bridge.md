# ADR-011: WASM Bridge via Instruction Events

## Status
Accepted

## Context
DOM mutations need to be expressible in a format consumable by WASM runtimes.

## Decision
WASM operations (`set_attribute`, `remove_element`, etc.) are queued as `DOM_EVENT_WASM_INSTRUCTION` events with an opcode enum, target node ID, and key/value strings.

## Consequences
- Uniform event-driven interface for mutations
- No direct DOM manipulation — mutations are declarative
- WASM runtime interprets instructions on its side