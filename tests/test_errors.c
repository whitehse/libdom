/**
 * dom_errors — Error and edge-case tests for libdom
 *
 * SPDX-License-Identifier: MIT
 */
#include "dom.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define PASS(name) printf("  PASS %s\n", name)

static void test_empty_input(void) {
    dom_ctx *ctx = dom_create();
    int rc = dom_feed_input(ctx, "", 0);
    assert(rc == 0);
    assert(dom_event_count(ctx) == 0);
    dom_destroy(ctx);
    PASS("empty input");
}

static void test_deep_nesting_overflow(void) {
    dom_config_t cfg = dom_default_config();
    cfg.max_dom_depth = 5;
    dom_ctx *ctx = dom_create_with_config(&cfg);

    /* 10 levels deep — should trigger depth error */
    const char *html = "<a><b><c><d><e><f></f></e></d></c></b></a>";
    int rc = dom_feed_input(ctx, html, strlen(html));
    assert(rc == -1);

    /* Check that an error event was emitted */
    dom_event_t ev;
    int found_error = 0;
    while (dom_next_event(ctx, &ev)) {
        if (ev.type == DOM_EVENT_ERROR) found_error = 1;
    }
    assert(found_error);
    dom_destroy(ctx);
    PASS("deep nesting overflow");
}

static void test_malformed_tag(void) {
    dom_ctx *ctx = dom_create();
    /* Malformed but shouldn't crash */
    const char *html = "< div><<>>";
    dom_feed_input(ctx, html, strlen(html));
    /* Just verify no crash */
    dom_event_t ev;
    while (dom_next_event(ctx, &ev)) {}
    dom_destroy(ctx);
    PASS("malformed tags");
}

static void test_oversized_input(void) {
    dom_config_t cfg = dom_default_config();
    cfg.max_input_size = 100;
    dom_ctx *ctx = dom_create_with_config(&cfg);

    /* Feed 200 bytes */
    char buf[201];
    memset(buf, 'x', 200);
    buf[200] = '\0';
    int rc = dom_feed_input(ctx, buf, 200);
    assert(rc == -1);

    dom_event_t ev;
    int found_error = 0;
    while (dom_next_event(ctx, &ev)) {
        if (ev.type == DOM_EVENT_ERROR) found_error = 1;
    }
    assert(found_error);
    dom_destroy(ctx);
    PASS("oversized input");
}

static void test_text_only(void) {
    dom_ctx *ctx = dom_create();
    dom_feed_input(ctx, "Hello world", 11);
    dom_event_t ev;
    int found = 0;
    while (dom_next_event(ctx, &ev)) {
        if (ev.type == DOM_EVENT_TEXT) {
            assert(ev.data.text.text_len == 11);
            found = 1;
        }
    }
    assert(found);
    dom_destroy(ctx);
    PASS("text only");
}

static void test_unclosed_tag(void) {
    dom_ctx *ctx = dom_create();
    dom_feed_input(ctx, "<div", 4);
    /* No crash expected; events may or may not be emitted */
    dom_event_t ev;
    while (dom_next_event(ctx, &ev)) {}
    dom_destroy(ctx);
    PASS("unclosed tag");
}

static void test_double_close(void) {
    dom_ctx *ctx = dom_create();
    dom_feed_input(ctx, "<div></div></div>", 17);
    /* Should not crash; extra close is benign */
    dom_event_t ev;
    while (dom_next_event(ctx, &ev)) {}
    dom_destroy(ctx);
    PASS("double close");
}

static void test_wasm_events(void) {
    dom_ctx *ctx = dom_create();
    dom_wasm_set_attribute(ctx, 1, "class", "active");
    dom_wasm_remove_element(ctx, 2);

    dom_event_t ev;
    int wasm_count = 0;
    while (dom_next_event(ctx, &ev)) {
        if (ev.type == DOM_EVENT_WASM_INSTRUCTION) wasm_count++;
    }
    assert(wasm_count == 2);
    dom_destroy(ctx);
    PASS("wasm events");
}

static void test_css_matches(void) {
    dom_element_t el;
    memset(&el, 0, sizeof(el));
    strcpy(el.tag, "div");
    el.tag_len = 3;

    dom_attr_t a;
    memset(&a, 0, sizeof(a));
    strcpy(a.name, "class");
    a.name_len = 5;
    strcpy(a.value, "main");
    a.value_len = 4;
    a.has_value = 1;
    el.attrs[0] = a;
    el.attr_count = 1;

    assert(dom_css_matches(&el, "div") == 1);
    assert(dom_css_matches(&el, "span") == 0);
    assert(dom_css_matches(&el, ".main") == 1);
    assert(dom_css_matches(&el, ".other") == 0);
    assert(dom_css_matches(&el, "div.main") == 1);
    assert(dom_css_matches(&el, "span.main") == 0);
    PASS("css matches");
}

int main(void) {
    printf("dom_errors\n");
    test_empty_input();
    test_deep_nesting_overflow();
    test_malformed_tag();
    test_oversized_input();
    test_text_only();
    test_unclosed_tag();
    test_double_close();
    test_wasm_events();
    test_css_matches();
    printf("ALL PASSED\n");
    return 0;
}