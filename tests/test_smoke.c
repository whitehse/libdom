/**
 * dom_smoke — Basic smoke tests for libdom
 *
 * SPDX-License-Identifier: MIT
 */
#include "dom.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define PASS(name) printf("  PASS %s\n", name)

static void test_create_destroy(void) {
    dom_ctx *ctx = dom_create();
    assert(ctx != NULL);
    dom_destroy(ctx);
    PASS("create/destroy");
}

static void test_null_handling(void) {
    dom_destroy(NULL); /* should not crash */
    dom_reset(NULL);
    assert(dom_depth(NULL) == 0);
    assert(dom_has_pending_events(NULL) == 0);
    assert(dom_event_count(NULL) == 0);
    assert(dom_parser_state(NULL) == 0);
    dom_event_t ev;
    assert(dom_next_event(NULL, &ev) == 0);
    assert(dom_feed_input(NULL, "x", 1) == -1);
    PASS("null handling");
}

static void test_config(void) {
    dom_config_t cfg = dom_default_config();
    assert(cfg.event_queue_size == 256);
    assert(cfg.max_dom_depth == 256);
    cfg.max_dom_depth = 10;
    cfg.parse_css = 1;
    dom_ctx *ctx = dom_create_with_config(&cfg);
    assert(ctx != NULL);
    dom_destroy(ctx);
    PASS("config");
}

static void test_simple_html(void) {
    dom_ctx *ctx = dom_create();
    const char *html = "<p>Hello</p>";
    int rc = dom_feed_input(ctx, html, strlen(html));
    assert(rc == 0);

    dom_event_t ev;
    int found_open = 0, found_text = 0, found_close = 0;
    while (dom_next_event(ctx, &ev)) {
        if (ev.type == DOM_EVENT_ELEMENT_OPEN) {
            assert(ev.data.element.tag_len == 1);
            assert(ev.data.element.tag[0] == 'p');
            found_open = 1;
        }
        if (ev.type == DOM_EVENT_TEXT) {
            assert(ev.data.text.text_len == 5);
            assert(memcmp(ev.data.text.text, "Hello", 5) == 0);
            found_text = 1;
        }
        if (ev.type == DOM_EVENT_ELEMENT_CLOSE) {
            found_close = 1;
        }
    }
    assert(found_open);
    assert(found_text);
    assert(found_close);
    dom_destroy(ctx);
    PASS("simple HTML parse");
}

static void test_self_closing(void) {
    dom_ctx *ctx = dom_create();
    const char *html = "<br/><img src=\"x.png\">";
    int rc = dom_feed_input(ctx, html, strlen(html));
    assert(rc == 0);

    dom_event_t ev;
    int br_count = 0;
    while (dom_next_event(ctx, &ev)) {
        if (ev.type == DOM_EVENT_ELEMENT_SELF_CLOSE) br_count++;
    }
    assert(br_count == 2);
    dom_destroy(ctx);
    PASS("self-closing tags");
}

static void test_attributes(void) {
    dom_ctx *ctx = dom_create();
    const char *html = "<div class=\"main\" id=\"top\">";
    dom_feed_input(ctx, html, strlen(html));

    dom_event_t ev;
    int ok = 0;
    while (dom_next_event(ctx, &ev)) {
        if (ev.type == DOM_EVENT_ELEMENT_OPEN) {
            assert(ev.data.element.attr_count == 2);
            assert(strcmp(ev.data.element.attrs[0].name, "class") == 0);
            assert(strcmp(ev.data.element.attrs[0].value, "main") == 0);
            assert(strcmp(ev.data.element.attrs[1].name, "id") == 0);
            ok = 1;
        }
    }
    assert(ok);
    dom_destroy(ctx);
    PASS("attributes");
}

static void test_reset(void) {
    dom_ctx *ctx = dom_create();
    dom_feed_input(ctx, "<p>Hi</p>", 9);
    dom_reset(ctx);
    assert(dom_depth(ctx) == 0);
    assert(dom_event_count(ctx) == 0);
    assert(!dom_has_pending_events(ctx));
    dom_destroy(ctx);
    PASS("reset");
}

static void test_depth_tracking(void) {
    dom_ctx *ctx = dom_create();
    dom_feed_input(ctx, "<div><span><b>", 14);
    assert(dom_depth(ctx) == 3);
    dom_feed_input(ctx, "</b></span></div>", 17);
    assert(dom_depth(ctx) == 0);
    dom_destroy(ctx);
    PASS("depth tracking");
}

static void test_event_count(void) {
    dom_ctx *ctx = dom_create();
    dom_feed_input(ctx, "<p>Hi</p>", 9);
    uint64_t cnt = dom_event_count(ctx);
    /* Should have: ELEMENT_OPEN, TEXT, ELEMENT_CLOSE = 3 */
    assert(cnt == 3);
    dom_destroy(ctx);
    PASS("event count");
}

static void test_comment(void) {
    dom_ctx *ctx = dom_create();
    dom_feed_input(ctx, "<!-- hello -->", 14);
    dom_event_t ev;
    int found = 0;
    while (dom_next_event(ctx, &ev)) {
        if (ev.type == DOM_EVENT_COMMENT) {
            assert(strstr(ev.data.comment.text, "hello") != NULL);
            found = 1;
        }
    }
    assert(found);
    dom_destroy(ctx);
    PASS("comment");
}

static void test_nested_text(void) {
    dom_ctx *ctx = dom_create();
    dom_feed_input(ctx, "<div><p>A</p><p>B</p></div>", 27);
    int text_count = 0;
    dom_event_t ev;
    while (dom_next_event(ctx, &ev)) {
        if (ev.type == DOM_EVENT_TEXT) text_count++;
    }
    assert(text_count == 2);
    dom_destroy(ctx);
    PASS("nested text");
}

int main(void) {
    printf("dom_smoke\n");
    test_create_destroy();
    test_null_handling();
    test_config();
    test_simple_html();
    test_self_closing();
    test_attributes();
    test_reset();
    test_depth_tracking();
    test_event_count();
    test_comment();
    test_nested_text();
    printf("ALL PASSED\n");
    return 0;
}