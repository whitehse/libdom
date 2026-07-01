# ADR-004: Self-Closing Tag Recognition

## Status
Accepted

## Context
HTML void elements (br, hr, img, etc.) don't have closing tags.

## Decision
Maintain a hardcoded list of void elements. Tags matching the list emit `ELEMENT_SELF_CLOSE` instead of requiring a `</tag>` close.

## Consequences
- Correct handling of `<br/>`, `<img>`, etc.
- List is static — new HTML elements would require code changes