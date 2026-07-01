/**
 * libdom example — Parse HTML and print events
 *
 * SPDX-License-Identifier: MIT
 */
#include "dom.h"
#include <stdio.h>
#include <string.h>

static const char *event_name(dom_event_type_t t) {
    switch (t) {
    case DOM_EVENT_DOCUMENT_START:    return "DOCUMENT_START";
    case DOM_EVENT_DOCUMENT_END:      return "DOCUMENT_END";
    case DOM_EVENT_ELEMENT_OPEN:      return "ELEMENT_OPEN";
    case DOM_EVENT_ELEMENT_CLOSE:     return "ELEMENT_CLOSE";
    case DOM_EVENT_ELEMENT_SELF_CLOSE:return "ELEMENT_SELF_CLOSE";
    case DOM_EVENT_TEXT:              return "TEXT";
    case DOM_EVENT_COMMENT:           return "COMMENT";
    case DOM_EVENT_CSS_RULE:          return "CSS_RULE";
    case DOM_EVENT_WASM_INSTRUCTION:  return "WASM_INSTRUCTION";
    case DOM_EVENT_ERROR:             return "ERROR";
    case DOM_EVENT_QUEUE_OVERFLOW:    return "QUEUE_OVERFLOW";
    default:                          return "UNKNOWN";
    }
}

int main(void) {
    const char *html =
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<style>body { color: red; } h1 { font-size: 2em; }</style>"
        "</head>"
        "<body>"
        "<h1 id=\"title\" class=\"main\">Hello, libdom!</h1>"
        "<p>A <b>bold</b> paragraph.</p>"
        "<br/>"
        "<!-- a comment -->"
        "</body>"
        "</html>";

    dom_config_t cfg = dom_default_config();
    cfg.parse_css = 1;

    dom_ctx *ctx = dom_create_with_config(&cfg);
    if (!ctx) {
        fprintf(stderr, "Failed to create context\n");
        return 1;
    }

    dom_feed_input(ctx, html, strlen(html));

    dom_event_t ev;
    while (dom_next_event(ctx, &ev)) {
        printf("[%s]", event_name(ev.type));
        switch (ev.type) {
        case DOM_EVENT_ELEMENT_OPEN:
        case DOM_EVENT_ELEMENT_SELF_CLOSE:
            printf(" <%s", ev.data.element.tag);
            for (uint32_t i = 0; i < ev.data.element.attr_count; i++) {
                if (ev.data.element.attrs[i].has_value)
                    printf(" %s=\"%s\"", ev.data.element.attrs[i].name,
                           ev.data.element.attrs[i].value);
                else
                    printf(" %s", ev.data.element.attrs[i].name);
            }
            printf("%s>", ev.data.element.self_closing ? " /" : "");
            break;
        case DOM_EVENT_ELEMENT_CLOSE:
            printf(" </%s>", ev.data.element.tag);
            break;
        case DOM_EVENT_TEXT:
            printf(" \"%.*s\"", (int)ev.data.text.text_len, ev.data.text.text);
            break;
        case DOM_EVENT_COMMENT:
            printf(" <!--%s-->", ev.data.comment.text);
            break;
        case DOM_EVENT_CSS_RULE:
            printf(" %s { %s: %s }", ev.data.css_rule.selector,
                   ev.data.css_rule.property, ev.data.css_rule.value);
            break;
        case DOM_EVENT_ERROR:
            printf(" %s", ev.data.error.message);
            break;
        default:
            break;
        }
        printf("\n");
    }

    printf("\nTotal events: %lu\n", (unsigned long)dom_event_count(ctx));
    dom_destroy(ctx);
    return 0;
}