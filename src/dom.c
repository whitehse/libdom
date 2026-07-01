/**
 * libdom — Pure C DOM/HTML/CSS parser implementation
 *
 * Syscall-free, callback-free event-driven state machine.
 *
 * SPDX-License-Identifier: MIT
 */
#include "dom.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

/* ── Parser states ─────────────────────────────────────────────────── */

enum parser_state {
    PS_IDLE            = 0,
    PS_TAG_OPEN        = 1,
    PS_TAG_NAME        = 2,
    PS_TAG_ATTR_NAME   = 3,
    PS_TAG_ATTR_VALUE  = 4,
    PS_TAG_CLOSE       = 5,
    PS_TEXT_CONTENT    = 6,
    PS_COMMENT         = 7,
    PS_DOCTYPE         = 8,
    PS_COMMENT_MAYBE   = 9
};

/* ── Ring buffer ───────────────────────────────────────────────────── */

#define RING_DEFAULT_SIZE 256

typedef struct {
    dom_event_t *buf;
    uint32_t     cap;
    uint32_t     head;
    uint32_t     tail;
    uint32_t     count;
} ring_t;

static int ring_init(ring_t *r, uint32_t cap) {
    r->buf = (dom_event_t *)calloc(cap, sizeof(dom_event_t));
    if (!r->buf) return -1;
    r->cap = cap; r->head = 0; r->tail = 0; r->count = 0;
    return 0;
}

static void ring_free(ring_t *r) { free(r->buf); r->buf = NULL; }

static int ring_push(ring_t *r, const dom_event_t *ev) {
    if (r->count >= r->cap) return -1;
    r->buf[r->head] = *ev;
    r->head = (r->head + 1) % r->cap;
    r->count++;
    return 0;
}

static int ring_pop(ring_t *r, dom_event_t *out) {
    if (r->count == 0) return 0;
    *out = r->buf[r->tail];
    r->tail = (r->tail + 1) % r->cap;
    r->count--;
    return 1;
}

/* ── Void elements ─────────────────────────────────────────────────── */

static int is_void_element(const char *tag, uint32_t len) {
    static const char *voids[] = {
        "br","hr","img","input","meta","link",
        "area","base","col","embed","source","track","wbr", NULL
    };
    for (const char **v = voids; *v; v++) {
        size_t vl = strlen(*v);
        if (vl == len && strncasecmp(tag, *v, len) == 0) return 1;
    }
    return 0;
}

/* ── Context ───────────────────────────────────────────────────────── */

struct dom_ctx {
    dom_config_t     cfg;
    enum parser_state state;

    char        tag_buf[64];
    uint32_t    tag_len;
    char        attr_name[128];
    uint32_t    attr_name_len;
    char        attr_value[4096];
    uint32_t    attr_value_len;
    char        text_buf[8192];
    uint32_t    text_len;
    char        comment_buf[8192];
    uint32_t    comment_len;
    int         in_attr_value;
    char        attr_quote;
    dom_attr_t  attrs[64];
    uint32_t    attr_count;
    int         closing_tag;
    int         self_closing;

    /* Comment lookahead for detecting '<!-' → '<!--' */
    char        comment_lookahead[2];
    int         comment_lookahead_len;

    /* CSS */
    char       *css_buf;
    size_t      css_len;
    size_t      css_cap;
    int         in_style;

    /* Serializer output */
    char       *output;
    size_t      output_len;
    size_t      output_cap;

    uint32_t    node_counter;
    uint32_t    depth;
    uint64_t    event_count;

    ring_t      events;
};

/* ── Event emit helpers ────────────────────────────────────────────── */

static void emit_ev(dom_ctx *ctx, const dom_event_t *ev) {
    if (ring_push(&ctx->events, ev) != 0) {
        dom_event_t ov; memset(&ov, 0, sizeof(ov));
        ov.type = DOM_EVENT_QUEUE_OVERFLOW;
        ring_push(&ctx->events, &ov);
    }
    ctx->event_count++;
}

static void emit_error(dom_ctx *ctx, const char *msg) {
    dom_event_t ev; memset(&ev, 0, sizeof(ev));
    ev.type = DOM_EVENT_ERROR;
    size_t ml = strlen(msg);
    if (ml >= sizeof(ev.data.error.message)) ml = sizeof(ev.data.error.message) - 1;
    memcpy(ev.data.error.message, msg, ml);
    ev.data.error.message[ml] = '\0';
    ev.data.error.message_len = (uint32_t)ml;
    emit_ev(ctx, &ev);
}

static void flush_text(dom_ctx *ctx) {
    if (ctx->text_len == 0) return;
    dom_event_t ev; memset(&ev, 0, sizeof(ev));
    ev.type = DOM_EVENT_TEXT; ev.depth = ctx->depth;
    uint32_t n = ctx->text_len;
    if (n >= sizeof(ev.data.text.text)) n = (uint32_t)sizeof(ev.data.text.text) - 1;
    memcpy(ev.data.text.text, ctx->text_buf, n);
    ev.data.text.text[n] = '\0';
    ev.data.text.text_len = n;
    emit_ev(ctx, &ev);
    ctx->text_len = 0;
}

static void flush_comment(dom_ctx *ctx) {
    if (ctx->comment_len == 0) return;
    dom_event_t ev; memset(&ev, 0, sizeof(ev));
    ev.type = DOM_EVENT_COMMENT; ev.depth = ctx->depth;
    uint32_t n = ctx->comment_len;
    if (n >= sizeof(ev.data.comment.text)) n = (uint32_t)sizeof(ev.data.comment.text) - 1;
    memcpy(ev.data.comment.text, ctx->comment_buf, n);
    ev.data.comment.text[n] = '\0';
    ev.data.comment.text_len = n;
    emit_ev(ctx, &ev);
    ctx->comment_len = 0;
}

static void emit_open(dom_ctx *ctx, int self_close) {
    dom_event_t ev; memset(&ev, 0, sizeof(ev));
    ev.type = self_close ? DOM_EVENT_ELEMENT_SELF_CLOSE : DOM_EVENT_ELEMENT_OPEN;
    ev.depth = ctx->depth;
    dom_element_t *el = &ev.data.element;
    uint32_t n = ctx->tag_len;
    if (n >= sizeof(el->tag)) n = (uint32_t)sizeof(el->tag) - 1;
    memcpy(el->tag, ctx->tag_buf, n); el->tag[n] = '\0';
    el->tag_len = n; el->self_closing = self_close;
    el->node_id = ++ctx->node_counter; el->parent_id = 0; el->depth = ctx->depth;
    el->attr_count = ctx->attr_count;
    if (ctx->attr_count > 64) ctx->attr_count = 64;
    for (uint32_t i = 0; i < ctx->attr_count; i++) el->attrs[i] = ctx->attrs[i];
    emit_ev(ctx, &ev);
    if (ctx->cfg.parse_css && strncasecmp(ctx->tag_buf, "style", 5) == 0 &&
        ctx->tag_len == 5 && !self_close) {
        ctx->in_style = 1; ctx->css_len = 0;
    }
}

static void emit_close(dom_ctx *ctx) {
    dom_event_t ev; memset(&ev, 0, sizeof(ev));
    ev.type = DOM_EVENT_ELEMENT_CLOSE; ev.depth = ctx->depth;
    dom_element_t *el = &ev.data.element;
    uint32_t n = ctx->tag_len;
    if (n >= sizeof(el->tag)) n = (uint32_t)sizeof(el->tag) - 1;
    memcpy(el->tag, ctx->tag_buf, n); el->tag[n] = '\0';
    el->tag_len = n;
    emit_ev(ctx, &ev);
    if (ctx->cfg.parse_css && strncasecmp(ctx->tag_buf, "style", 5) == 0 &&
        ctx->tag_len == 5) {
        ctx->in_style = 0;
    }
}

static void emit_css_rules(dom_ctx *ctx) {
    const char *p = ctx->css_buf;
    const char *end = p + ctx->css_len;
    char sel[256] = {0}, prop[128] = {0}, val[1024] = {0};
    while (p < end) {
        while (p < end && isspace((unsigned char)*p)) p++;
        if (p >= end) break;
        size_t sl = 0;
        while (p < end && *p != '{') { if (sl < sizeof(sel)-1) sel[sl++] = *p; p++; }
        sel[sl] = '\0';
        while (sl > 0 && isspace((unsigned char)sel[sl-1])) sel[--sl] = '\0';
        if (p < end) p++;
        while (p < end && *p != '}') {
            while (p < end && isspace((unsigned char)*p)) p++;
            size_t pl = 0;
            while (p < end && *p != ':') { if (pl < sizeof(prop)-1) prop[pl++] = *p; p++; }
            prop[pl] = '\0';
            while (pl > 0 && isspace((unsigned char)prop[pl-1])) prop[--pl] = '\0';
            if (p < end) p++;
            size_t vl = 0;
            while (p < end && *p != ';' && *p != '}') { if (vl < sizeof(val)-1) val[vl++] = *p; p++; }
            val[vl] = '\0';
            while (vl > 0 && isspace((unsigned char)val[vl-1])) val[--vl] = '\0';
            if (p < end && *p == ';') p++;
            if (pl > 0 && vl > 0) {
                dom_event_t ev; memset(&ev, 0, sizeof(ev));
                ev.type = DOM_EVENT_CSS_RULE;
                memcpy(ev.data.css_rule.selector, sel, sl+1);
                memcpy(ev.data.css_rule.property, prop, pl+1);
                memcpy(ev.data.css_rule.value, val, vl+1);
                emit_ev(ctx, &ev);
            }
        }
        if (p < end) p++;
    }
    ctx->css_len = 0;
}

static void save_pending_attr(dom_ctx *ctx) {
    if (ctx->attr_name_len > 0 && ctx->attr_count < 64) {
        ctx->attr_name[ctx->attr_name_len] = '\0';
        dom_attr_t *a = &ctx->attrs[ctx->attr_count];
        uint32_t n = ctx->attr_name_len;
        if (n >= sizeof(a->name)) n = (uint32_t)sizeof(a->name) - 1;
        memcpy(a->name, ctx->attr_name, n); a->name[n] = '\0'; a->name_len = n;
        a->value[0] = '\0'; a->value_len = 0; a->has_value = 0;
        ctx->attr_count++; ctx->attr_name_len = 0;
    }
}

static void save_attr(dom_ctx *ctx) {
    if (ctx->attr_count < 64) {
        ctx->attr_value[ctx->attr_value_len] = '\0';
        dom_attr_t *a = &ctx->attrs[ctx->attr_count];
        uint32_t n = ctx->attr_name_len;
        if (n >= sizeof(a->name)) n = (uint32_t)sizeof(a->name) - 1;
        memcpy(a->name, ctx->attr_name, n); a->name[n] = '\0'; a->name_len = n;
        uint32_t vn = ctx->attr_value_len;
        if (vn >= sizeof(a->value)) vn = (uint32_t)sizeof(a->value) - 1;
        memcpy(a->value, ctx->attr_value, vn); a->value[vn] = '\0'; a->value_len = vn;
        a->has_value = 1; ctx->attr_count++;
    }
    ctx->attr_name_len = 0; ctx->attr_value_len = 0; ctx->in_attr_value = 0;
}

static void finish_tag(dom_ctx *ctx) {
    ctx->tag_buf[ctx->tag_len] = '\0';
    if (ctx->closing_tag) {
        if (ctx->depth > 0) ctx->depth--;
        emit_close(ctx);
    } else {
        int sc = ctx->self_closing || is_void_element(ctx->tag_buf, ctx->tag_len);
        if (!sc) ctx->depth++;
        emit_open(ctx, sc);
        if (sc && ctx->depth > 0) ctx->depth--;
    }
    ctx->closing_tag = 0; ctx->self_closing = 0; ctx->state = PS_IDLE;
}

/* ── State machine ─────────────────────────────────────────────────── */

static void step(dom_ctx *ctx, char c) {
    switch (ctx->state) {

    case PS_IDLE:
        if (c == '<') { flush_text(ctx); ctx->state = PS_TAG_OPEN; }
        else {
            if (ctx->in_style) {
                if (ctx->css_len < ctx->css_cap - 1) ctx->css_buf[ctx->css_len++] = c;
            } else {
                if (ctx->text_len < sizeof(ctx->text_buf) - 1)
                    ctx->text_buf[ctx->text_len++] = c;
            }
            ctx->state = PS_TEXT_CONTENT;
        }
        break;

    case PS_TEXT_CONTENT:
        if (c == '<') {
            if (ctx->in_style) {
                /* CSS text ends — don't flush as a DOM text event, it's CSS content */
                ctx->text_len = 0;
            } else {
                flush_text(ctx);
            }
            ctx->state = PS_TAG_OPEN;
        } else {
            if (ctx->in_style) {
                if (ctx->css_len < ctx->css_cap - 1) ctx->css_buf[ctx->css_len++] = c;
            } else {
                if (ctx->text_len < sizeof(ctx->text_buf) - 1)
                    ctx->text_buf[ctx->text_len++] = c;
            }
        }
        break;

    case PS_TAG_OPEN:
        if (c == '/') { ctx->closing_tag = 1; ctx->tag_len = 0; ctx->state = PS_TAG_NAME; }
        else if (c == '!') { ctx->comment_lookahead_len = 0; ctx->state = PS_COMMENT_MAYBE; }
        else if (isalpha((unsigned char)c)) {
            ctx->closing_tag = 0; ctx->tag_len = 0;
            ctx->tag_buf[ctx->tag_len++] = c; ctx->attr_count = 0;
            ctx->state = PS_TAG_NAME;
        } else {
            ctx->text_len = 0;
            ctx->text_buf[ctx->text_len++] = '<';
            if (ctx->text_len < sizeof(ctx->text_buf) - 1)
                ctx->text_buf[ctx->text_len++] = c;
            ctx->state = PS_TEXT_CONTENT;
        }
        break;

    case PS_TAG_NAME:
        if (isspace((unsigned char)c)) {
            ctx->tag_buf[ctx->tag_len] = '\0'; ctx->attr_count = 0; ctx->attr_name_len = 0;
            ctx->state = PS_TAG_ATTR_NAME;
        } else if (c == '>') { ctx->attr_count = 0; finish_tag(ctx); }
        else if (c == '/') { ctx->self_closing = 1; ctx->state = PS_TAG_CLOSE; }
        else { if (ctx->tag_len < sizeof(ctx->tag_buf) - 1) ctx->tag_buf[ctx->tag_len++] = c; }
        break;

    case PS_TAG_ATTR_NAME:
        if (c == '=') {
            ctx->attr_name[ctx->attr_name_len] = '\0';
            ctx->attr_value_len = 0; ctx->in_attr_value = 0;
            ctx->state = PS_TAG_ATTR_VALUE;
        } else if (c == '>') { save_pending_attr(ctx); finish_tag(ctx); }
        else if (c == '/') { save_pending_attr(ctx); ctx->self_closing = 1; ctx->state = PS_TAG_CLOSE; }
        else if (isspace((unsigned char)c)) { save_pending_attr(ctx); }
        else { if (ctx->attr_name_len < sizeof(ctx->attr_name) - 1) ctx->attr_name[ctx->attr_name_len++] = c; }
        break;

    case PS_TAG_ATTR_VALUE:
        if (!ctx->in_attr_value) {
            if (c == '"' || c == '\'') { ctx->attr_quote = c; ctx->in_attr_value = 1; }
            else if (!isspace((unsigned char)c)) {
                ctx->attr_quote = 0; ctx->in_attr_value = 1;
                if (ctx->attr_value_len < sizeof(ctx->attr_value) - 1)
                    ctx->attr_value[ctx->attr_value_len++] = c;
            }
        } else {
            if ((ctx->attr_quote && c == ctx->attr_quote) ||
                (!ctx->attr_quote && (isspace((unsigned char)c) || c == '>'))) {
                save_attr(ctx);
                if (c == '>') finish_tag(ctx);
                else if (c == '/') { ctx->self_closing = 1; ctx->state = PS_TAG_CLOSE; }
                else ctx->state = PS_TAG_ATTR_NAME;
            } else {
                if (ctx->attr_value_len < sizeof(ctx->attr_value) - 1)
                    ctx->attr_value[ctx->attr_value_len++] = c;
            }
        }
        break;

    case PS_TAG_CLOSE:
        if (c == '>') finish_tag(ctx);
        break;

    case PS_COMMENT_MAYBE:
        if (ctx->comment_lookahead_len < 2) {
            ctx->comment_lookahead[ctx->comment_lookahead_len++] = c;
            if (ctx->comment_lookahead_len == 2) {
                if (ctx->comment_lookahead[0] == '-' && ctx->comment_lookahead[1] == '-') {
                    ctx->comment_len = 0; ctx->state = PS_COMMENT;
                } else {
                    ctx->state = PS_DOCTYPE;
                }
            }
        }
        break;

    case PS_DOCTYPE:
        if (c == '>') ctx->state = PS_IDLE;
        break;

    case PS_COMMENT:
        if (c == '>' && ctx->comment_len >= 2 &&
            ctx->comment_buf[ctx->comment_len-1] == '-' &&
            ctx->comment_buf[ctx->comment_len-2] == '-') {
            ctx->comment_len -= 2;
            ctx->comment_buf[ctx->comment_len] = '\0';
            flush_comment(ctx);
            ctx->state = PS_IDLE;
        } else {
            if (ctx->comment_len < sizeof(ctx->comment_buf) - 1)
                ctx->comment_buf[ctx->comment_len++] = c;
        }
        break;
    }
}

/* ── Public API ────────────────────────────────────────────────────── */

dom_config_t dom_default_config(void) {
    dom_config_t c;
    c.event_queue_size = RING_DEFAULT_SIZE;
    c.max_input_size = 0; c.max_dom_depth = 256;
    c.parse_css = 0; c.strict_mode = 0;
    return c;
}

dom_ctx *dom_create(void) {
    dom_config_t c = dom_default_config();
    return dom_create_with_config(&c);
}

dom_ctx *dom_create_with_config(const dom_config_t *cfg) {
    dom_ctx *ctx = (dom_ctx *)calloc(1, sizeof(dom_ctx));
    if (!ctx) return NULL;
    ctx->cfg = *cfg;
    if (ctx->cfg.event_queue_size == 0) ctx->cfg.event_queue_size = RING_DEFAULT_SIZE;
    if (ring_init(&ctx->events, ctx->cfg.event_queue_size) != 0) { free(ctx); return NULL; }
    if (ctx->cfg.parse_css) {
        ctx->css_cap = 8192;
        ctx->css_buf = (char *)calloc(1, ctx->css_cap);
        if (!ctx->css_buf) { ring_free(&ctx->events); free(ctx); return NULL; }
    }
    ctx->state = PS_IDLE;
    return ctx;
}

void dom_destroy(dom_ctx *ctx) {
    if (!ctx) return;
    ring_free(&ctx->events); free(ctx->css_buf); free(ctx->output); free(ctx);
}

void dom_reset(dom_ctx *ctx) {
    if (!ctx) return;
    dom_event_t tmp;
    while (ring_pop(&ctx->events, &tmp)) {}
    ctx->state = PS_IDLE;
    ctx->tag_len = ctx->attr_name_len = ctx->attr_value_len = 0;
    ctx->text_len = ctx->comment_len = ctx->attr_count = 0;
    ctx->closing_tag = ctx->self_closing = 0;
    ctx->in_attr_value = ctx->comment_lookahead_len = 0;
    ctx->depth = ctx->node_counter = 0;
    ctx->event_count = 0;
    ctx->in_style = 0; ctx->css_len = 0;
    ctx->output_len = 0;
}

int dom_feed_input(dom_ctx *ctx, const char *data, size_t len) {
    if (!ctx || !data) return -1;
    if (ctx->cfg.max_input_size > 0 && len > ctx->cfg.max_input_size) {
        emit_error(ctx, "input size limit exceeded"); return -1;
    }
    for (size_t i = 0; i < len; i++) {
        if (ctx->cfg.max_dom_depth > 0 && ctx->depth >= ctx->cfg.max_dom_depth) {
            emit_error(ctx, "max DOM depth exceeded"); return -1;
        }
        step(ctx, data[i]);
    }
    /* Flush remaining text at end of input */
    if (ctx->state == PS_TEXT_CONTENT) {
        if (ctx->in_style) {
            /* Don't flush CSS content as text events */
            ctx->text_len = 0;
        } else {
            flush_text(ctx);
        }
        ctx->state = PS_IDLE;
    }
    /* Emit CSS rules if style block was closed during this feed */
    if (ctx->cfg.parse_css && !ctx->in_style && ctx->css_len > 0) {
        emit_css_rules(ctx);
    }
    return 0;
}

int dom_next_event(dom_ctx *ctx, dom_event_t *out) {
    if (!ctx || !out) return 0;
    return ring_pop(&ctx->events, out);
}

const char *dom_get_output(const dom_ctx *ctx, size_t *out_len) {
    if (!ctx || !ctx->output) { if (out_len) *out_len = 0; return ""; }
    if (out_len) *out_len = ctx->output_len;
    return ctx->output;
}

int dom_parser_state(const dom_ctx *ctx) { return ctx ? (int)ctx->state : 0; }
uint32_t dom_depth(const dom_ctx *ctx) { return ctx ? ctx->depth : 0; }
int dom_has_pending_events(const dom_ctx *ctx) { return ctx ? (ctx->events.count > 0) : 0; }
uint64_t dom_event_count(const dom_ctx *ctx) { return ctx ? ctx->event_count : 0; }

/* ── Serializer ────────────────────────────────────────────────────── */

static int ser_append(dom_ctx *ctx, const char *s, size_t n) {
    if (!ctx->output) {
        ctx->output_cap = 4096;
        ctx->output = (char *)calloc(1, ctx->output_cap);
        if (!ctx->output) return -1;
    }
    size_t need = ctx->output_len + n + 1;
    if (need > ctx->output_cap) {
        size_t nc = ctx->output_cap * 2;
        while (nc < need) nc *= 2;
        char *p = (char *)realloc(ctx->output, nc);
        if (!p) return -1;
        ctx->output = p; ctx->output_cap = nc;
    }
    memcpy(ctx->output + ctx->output_len, s, n);
    ctx->output_len += n; ctx->output[ctx->output_len] = '\0';
    return 0;
}

int dom_emit_element_open(dom_ctx *ctx, const dom_element_t *el) {
    if (!ctx || !el) return -1;
    char buf[4096];
    int n = snprintf(buf, sizeof(buf), "<%.*s", (int)el->tag_len, el->tag);
    if (n < 0) return -1;
    for (uint32_t i = 0; i < el->attr_count; i++) {
        const dom_attr_t *a = &el->attrs[i];
        int r;
        if (a->has_value)
            r = snprintf(buf+(size_t)n, sizeof(buf)-(size_t)n, " %s=\"%s\"", a->name, a->value);
        else
            r = snprintf(buf+(size_t)n, sizeof(buf)-(size_t)n, " %s", a->name);
        if (r > 0) n += r;
    }
    {
        int r = el->self_closing
            ? snprintf(buf+(size_t)n, sizeof(buf)-(size_t)n, " />")
            : snprintf(buf+(size_t)n, sizeof(buf)-(size_t)n, ">");
        if (r > 0) n += r;
    }
    return ser_append(ctx, buf, (size_t)n);
}

int dom_emit_element_close(dom_ctx *ctx, const char *tag, uint32_t tag_len) {
    if (!ctx || !tag) return -1;
    char buf[512];
    int n = snprintf(buf, sizeof(buf), "</%.*s>", (int)tag_len, tag);
    if (n < 0) return -1;
    return ser_append(ctx, buf, (size_t)n);
}

int dom_emit_text(dom_ctx *ctx, const char *text, uint32_t text_len) {
    if (!ctx || !text) return -1;
    return ser_append(ctx, text, text_len);
}

int dom_emit_comment(dom_ctx *ctx, const char *text, uint32_t text_len) {
    if (!ctx || !text) return -1;
    char buf[8208];
    int n = snprintf(buf, sizeof(buf), "<!--%.*s-->", (int)text_len, text);
    if (n < 0) return -1;
    return ser_append(ctx, buf, (size_t)n);
}

/* ── WASM bridge ───────────────────────────────────────────────────── */

static int wasm_push(dom_ctx *ctx, dom_wasm_op_t op, uint32_t node_id,
                     const char *key, const char *value) {
    if (!ctx) return -1;
    dom_event_t ev; memset(&ev, 0, sizeof(ev));
    ev.type = DOM_EVENT_WASM_INSTRUCTION; ev.node_id = node_id;
    dom_wasm_instruction_t *inst = (dom_wasm_instruction_t *)&ev.data;
    inst->op = op; inst->target_node_id = node_id;
    if (key) { size_t kl = strlen(key); if (kl >= sizeof(inst->key)) kl = sizeof(inst->key)-1;
        memcpy(inst->key, key, kl); inst->key[kl] = '\0'; }
    if (value) { size_t vl = strlen(value); if (vl >= sizeof(inst->value)) vl = sizeof(inst->value)-1;
        memcpy(inst->value, value, vl); inst->value[vl] = '\0'; }
    emit_ev(ctx, &ev);
    return 0;
}

int dom_wasm_set_attribute(dom_ctx *ctx, uint32_t nid, const char *k, const char *v) { return wasm_push(ctx, DOM_WASM_SET_ATTRIBUTE, nid, k, v); }
int dom_wasm_remove_attribute(dom_ctx *ctx, uint32_t nid, const char *k) { return wasm_push(ctx, DOM_WASM_REMOVE_ATTRIBUTE, nid, k, NULL); }
int dom_wasm_set_inner_html(dom_ctx *ctx, uint32_t nid, const char *h) { return wasm_push(ctx, DOM_WASM_SET_INNER_HTML, nid, NULL, h); }
int dom_wasm_set_text_content(dom_ctx *ctx, uint32_t nid, const char *t) { return wasm_push(ctx, DOM_WASM_SET_TEXT_CONTENT, nid, NULL, t); }
int dom_wasm_remove_element(dom_ctx *ctx, uint32_t nid) { return wasm_push(ctx, DOM_WASM_REMOVE_ELEMENT, nid, NULL, NULL); }
int dom_wasm_add_class(dom_ctx *ctx, uint32_t nid, const char *c) { return wasm_push(ctx, DOM_WASM_ADD_CLASS, nid, c, NULL); }
int dom_wasm_remove_class(dom_ctx *ctx, uint32_t nid, const char *c) { return wasm_push(ctx, DOM_WASM_REMOVE_CLASS, nid, c, NULL); }
int dom_wasm_replace_element(dom_ctx *ctx, uint32_t nid, const char *h) { return wasm_push(ctx, DOM_WASM_REPLACE_ELEMENT, nid, NULL, h); }

/* ── CSS matching ──────────────────────────────────────────────────── */

int dom_css_matches(const dom_element_t *el, const char *selector) {
    if (!el || !selector) return 0;
    const char *s = selector;
    const char *dot = strchr(s, '.');
    const char *hash = strchr(s, '#');
    const char *first = dot && hash ? (dot < hash ? dot : hash) : dot ? dot : hash;
    size_t tlen = first ? (size_t)(first - s) : strlen(s);
    if (tlen > 0) {
        if (tlen != el->tag_len) return 0;
        if (strncasecmp(s, el->tag, tlen) != 0) return 0;
    }
    const char *p = s;
    while (*p) {
        if (*p == '.') {
            p++; const char *end = p;
            while (*end && *end != '.' && *end != '#') end++;
            size_t cl = (size_t)(end - p); int found = 0;
            for (uint32_t i = 0; i < el->attr_count; i++) {
                if (strcasecmp(el->attrs[i].name, "class") == 0) {
                    const char *cv = el->attrs[i].value;
                    while (*cv) { while (*cv == ' ') cv++; const char *ce = cv;
                        while (*ce && *ce != ' ') ce++;
                        if ((size_t)(ce-cv) == cl && strncasecmp(cv, p, cl) == 0) { found = 1; break; }
                        cv = ce; }
                    break;
                }
            }
            if (!found) return 0;
            p = end;
        } else if (*p == '#') {
            p++; const char *end = p;
            while (*end && *end != '.' && *end != '#') end++;
            size_t il = (size_t)(end - p); int found = 0;
            for (uint32_t i = 0; i < el->attr_count; i++) {
                if (strcasecmp(el->attrs[i].name, "id") == 0 &&
                    el->attrs[i].value_len == il &&
                    strncasecmp(el->attrs[i].value, p, il) == 0) { found = 1; break; }
            }
            if (!found) return 0;
            p = end;
        } else { p++; }
    }
    return 1;
}