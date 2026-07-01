# TODO — libdom

## High Priority

- [ ] Add `dom_feed_input` incremental parsing (partial tokens across calls)
- [ ] Implement `dom_get_output` for serializer role (dedicated output buffer)
- [ ] Add node parent tracking for `parent_id` in element events

## Medium Priority

- [ ] Add CSS specificity scoring to `dom_css_matches()`
- [ ] Support attribute selectors (`[type="text"]`)
- [ ] Support descendant combinators (`div p`)
- [ ] Add `dom_serialize()` convenience function (parse → serialize in one call)
- [ ] Add XML/XHTML parsing mode

## Low Priority

- [ ] Add man pages in `man/man3/`
- [ ] Add `pkg-config` file generation
- [ ] Add install targets to CMakeLists.txt
- [ ] Add CMake option for shared library build
- [ ] Benchmark suite

## Completed

- [x] Core HTML parser state machine
- [x] Event-driven ring-buffer queue
- [x] Embedded string model (no pointers in events)
- [x] CSS extraction from `<style>` blocks
- [x] WASM bridge instruction queue
- [x] Simple CSS selector matching
- [x] Configurable limits (input size, DOM depth, queue size)
- [x] Smoke, dialectic, and error tests
- [x] Fuzz harness
- [x] Valgrind targets