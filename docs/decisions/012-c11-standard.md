# ADR-012: C11 Standard

## Status
Accepted

## Context
libdom needs broad compiler compatibility.

## Decision
Target C11. No VLAs, no `_Generic`, no GNU extensions. Compile with `-Wall -Wextra -Wpedantic -Werror`.

## Consequences
- Works with GCC, Clang, MSVC
- No C99-specific VLA security concerns
- Strict warnings catch bugs early