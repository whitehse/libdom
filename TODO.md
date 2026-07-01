# TODO — libdom

## Bugs (confirmed)

- [x] **CSS multi-rule parsing bug**: Fixed (added `if (p >= end || *p == '}') break;` after whitespace-skip in inner loop of emit_css_rules; verified via ad-hoc test + example)
- [x] **CSS value leading whitespace not trimmed**: Fixed (added leading ws trim on value after trailing trim; verified)

## High Priority

- [ ] Add `dom_feed_input` incremental parsing (partial tokens across calls) — note: already supports via persistent state machine; multiple feed calls work in tests
- [ ] Implement `dom_get_output` for serializer role (dedicated output buffer) — note: already present and used in dialectic tests
- [ ] Add node parent tracking for `parent_id` in element events (currently hardcoded to 0)
- [x] **Add `dom_wasm_instruction_t` to `dom_event_t` union**: Implemented (added member to union, reordered defs in dom.h for compile cleanliness, direct member access in wasm_push; ad-hoc verified + clean -Werror build)
- [ ] Implement `strict_mode` config option (declared in `dom_config_t` but never checked)
- [ ] Emit `DOM_EVENT_DOCUMENT_START` / `DOM_EVENT_DOCUMENT_END` events (enum values exist but are never produced by the parser)

## Medium Priority

- [ ] Add CSS specificity scoring to `dom_css_matches()`
- [ ] Support attribute selectors (`[type="text"]`)
- [ ] Support descendant combinators (`div p`)
- [ ] Add `dom_serialize()` convenience function (parse → serialize in one call)
- [ ] Add XML/XHTML parsing mode
- [ ] Handle `<script>` and `<textarea>` content (raw text elements — content should not be parsed as HTML)
- [ ] Add CSS overflow detection — when CSS content exceeds `css_cap` (8192), emit an error event instead of silently dropping characters
- [ ] Remove or implement `dom_role_t` enum (defined in header but never used)
- [ ] Remove or implement `DOM_NODE_CDATA` / `DOM_NODE_DOCTYPE` node types (defined but no corresponding events or parsing states)

## Low Priority

- [ ] Add man pages in `man/man3/`
- [ ] Add `pkg-config` file generation
- [ ] Add install targets to CMakeLists.txt
- [ ] Add CMake option for shared library build
- [ ] Benchmark suite

## Testing Gaps

- [ ] **Test CSS extraction**: No test exercises `cfg.parse_css = 1` and verifies `DOM_EVENT_CSS_RULE` events (the example does, but there is no test for it)
- [ ] **Test streaming/incremental parsing**: No test exercises multiple `dom_feed_input()` calls on the same context
- [ ] **Test queue overflow**: No test verifies `DOM_EVENT_QUEUE_OVERFLOW` behavior (small queue + many events)
- [ ] **Test `dom_css_matches()` with `#id` selector**: Only tag, `.class`, and combined selectors are tested
- [ ] **Test WASM instruction content**: Current test only counts WASM events, does not verify opcode, target_node_id, or key/value fields
- [ ] **Test serializer functions directly**: No test exercises `dom_emit_*` in isolation (only tested via dialectic roundtrip)
- [ ] **Test `dom_reset()` after CSS**: No test verifies that `dom_reset()` properly clears CSS state and allows re-parsing
- [ ] **Test deeply nested attributes**: No test for elements with many (10+) attributes
- [ ] **Test empty attribute values**: `attr=""` and bare `attr` semantics
- [ ] **Test DOM_EVENT_DOCUMENT_START/END**: Blocked on implementing emission of these

## Completed

- [x] Core HTML parser state machine
- [x] Event-driven ring-buffer queue
- [x] Embedded string model (no pointers in events)
- [x] CSS extraction from `<style>` blocks (bugs fixed)
- [x] WASM bridge instruction queue
- [x] Simple CSS selector matching
- [x] Configurable limits (input size, DOM depth, queue size)
- [x] Smoke, dialectic, and error tests
- [x] Fuzz harness
- [x] Valgrind targets
