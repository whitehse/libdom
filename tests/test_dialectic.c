/**
 * dom_dialectic — Round-trip: parse → events → serializer → verify
 *
 * SPDX-License-Identifier: MIT
 */
#include "dom.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define PASS(name) printf("  PASS %s\n", name)

/**
 * Parse HTML, collect events, re-emit via serializer, check output.
 */
static void roundtrip(const char *html, const char *expected) {
    dom_ctx *parser = dom_create();
    assert(parser != NULL);

    int rc = dom_feed_input(parser, html, strlen(html));
    assert(rc == 0);

    dom_ctx *serializer = dom_create();
    assert(serializer != NULL);

    dom_event_t ev;
    while (dom_next_event(parser, &ev)) {
        switch (ev.type) {
        case DOM_EVENT_ELEMENT_OPEN:
        case DOM_EVENT_ELEMENT_SELF_CLOSE:
            dom_emit_element_open(serializer, &ev.data.element);
            break;
        case DOM_EVENT_ELEMENT_CLOSE:
            dom_emit_element_close(serializer, ev.data.element.tag,
                                   ev.data.element.tag_len);
            break;
        case DOM_EVENT_TEXT:
            dom_emit_text(serializer, ev.data.text.text,
                          ev.data.text.text_len);
            break;
        case DOM_EVENT_COMMENT:
            dom_emit_comment(serializer, ev.data.comment.text,
                             ev.data.comment.text_len);
            break;
        default:
            break;
        }
    }

    size_t out_len = 0;
    const char *out = dom_get_output(serializer, &out_len);
    assert(out != NULL);

    if (expected) {
        assert(out_len == strlen(expected));
        assert(memcmp(out, expected, out_len) == 0);
    }

    dom_destroy(parser);
    dom_destroy(serializer);
}

static void test_simple_element(void) {
    roundtrip("<p>Hi</p>", "<p>Hi</p>");
    PASS("simple element roundtrip");
}

static void test_element_with_attrs(void) {
    roundtrip("<div class=\"main\">Text</div>",
              "<div class=\"main\">Text</div>");
    PASS("element with attrs roundtrip");
}

static void test_self_closing(void) {
    roundtrip("<br/>", "<br />");
    PASS("self-closing roundtrip");
}

static void test_nested(void) {
    roundtrip("<div><p>A</p></div>", "<div><p>A</p></div>");
    PASS("nested roundtrip");
}

static void test_multiple_attrs(void) {
    roundtrip("<input type=\"text\" name=\"q\"/>",
              "<input type=\"text\" name=\"q\" />");
    PASS("multiple attrs roundtrip");
}

static void test_comment_roundtrip(void) {
    roundtrip("<!-- test -->", "<!-- test -->");
    PASS("comment roundtrip");
}

static void test_empty_element(void) {
    roundtrip("<span></span>", "<span></span>");
    PASS("empty element roundtrip");
}

static void test_multiple_siblings(void) {
    roundtrip("<ul><li>A</li><li>B</li></ul>",
              "<ul><li>A</li><li>B</li></ul>");
    PASS("multiple siblings roundtrip");
}

int main(void) {
    printf("dom_dialectic\n");
    test_simple_element();
    test_element_with_attrs();
    test_self_closing();
    test_nested();
    test_multiple_attrs();
    test_comment_roundtrip();
    test_empty_element();
    test_multiple_siblings();
    printf("ALL PASSED\n");
    return 0;
}