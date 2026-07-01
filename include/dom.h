/**
 * libdom — Pure C DOM/HTML/CSS parser and representation
 *
 * Event-driven state-machine library. Syscall-free, callback-free.
 * All strings are embedded in fixed-size char arrays (no heap pointers).
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef DOM_H
#define DOM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Configuration ─────────────────────────────────────────────────── */

typedef enum {
    DOM_ROLE_PARSER     = 0,
    DOM_ROLE_SERIALIZER = 1
} dom_role_t;

typedef struct {
    uint32_t event_queue_size;   /* ring-buffer capacity (default 256)   */
    uint32_t max_input_size;     /* bytes; 0 = unlimited                 */
    uint32_t max_dom_depth;      /* nesting limit; 0 = unlimited         */
    int      parse_css;          /* extract <style> rules                */
    int      strict_mode;        /* abort on first error                 */
} dom_config_t;

/** Returns a sensible default configuration. */
dom_config_t dom_default_config(void);

/* ── Node types ────────────────────────────────────────────────────── */

typedef enum {
    DOM_NODE_ELEMENT   = 0,
    DOM_NODE_TEXT      = 1,
    DOM_NODE_COMMENT   = 2,
    DOM_NODE_DOCUMENT  = 3,
    DOM_NODE_CDATA     = 4,
    DOM_NODE_DOCTYPE   = 5
} dom_node_type_t;

/* ── Element / attribute ───────────────────────────────────────────── */

typedef struct {
    char     name[128];
    uint32_t name_len;
    char     value[4096];
    uint32_t value_len;
    int      has_value;
} dom_attr_t;

typedef struct {
    char     tag[64];
    uint32_t tag_len;
    dom_attr_t attrs[64];
    uint32_t attr_count;
    int      self_closing;
    uint32_t node_id;
    uint32_t parent_id;
    uint32_t depth;
} dom_element_t;

typedef struct {
    char     text[8192];
    uint32_t text_len;
} dom_text_t;

/* ── CSS ───────────────────────────────────────────────────────────── */

typedef struct {
    char selector[256];
    char property[128];
    char value[1024];
} dom_css_rule_t;

/* ── Event types ───────────────────────────────────────────────────── */

typedef enum {
    DOM_EVENT_DOCUMENT_START  = 0,
    DOM_EVENT_DOCUMENT_END    = 1,
    DOM_EVENT_ELEMENT_OPEN    = 2,
    DOM_EVENT_ELEMENT_CLOSE   = 3,
    DOM_EVENT_ELEMENT_SELF_CLOSE = 4,
    DOM_EVENT_TEXT            = 5,
    DOM_EVENT_COMMENT         = 6,
    DOM_EVENT_CSS_RULE        = 7,
    DOM_EVENT_WASM_INSTRUCTION = 8,
    DOM_EVENT_ERROR           = 9,
    DOM_EVENT_QUEUE_OVERFLOW  = 10
} dom_event_type_t;

typedef struct {
    char     message[512];
    uint32_t message_len;
} dom_error_t;

typedef struct {
    dom_event_type_t type;
    uint32_t         node_id;
    uint32_t         parent_id;
    uint32_t         depth;
    union {
        dom_element_t   element;
        dom_text_t      text;
        dom_css_rule_t  css_rule;
        dom_text_t      comment;   /* reuses text for comment body */
        dom_error_t     error;
    } data;
} dom_event_t;

/* ── WASM bridge ───────────────────────────────────────────────────── */

typedef enum {
    DOM_WASM_SET_ATTRIBUTE     = 0,
    DOM_WASM_REMOVE_ATTRIBUTE  = 1,
    DOM_WASM_SET_INNER_HTML    = 2,
    DOM_WASM_SET_TEXT_CONTENT  = 3,
    DOM_WASM_REMOVE_ELEMENT    = 4,
    DOM_WASM_ADD_CLASS         = 5,
    DOM_WASM_REMOVE_CLASS      = 6,
    DOM_WASM_REPLACE_ELEMENT   = 7
} dom_wasm_op_t;

typedef struct {
    dom_wasm_op_t op;
    uint32_t      target_node_id;
    char          key[128];
    char          value[4096];
} dom_wasm_instruction_t;

/* ── Opaque context ────────────────────────────────────────────────── */

typedef struct dom_ctx dom_ctx;

/* ── Core API ──────────────────────────────────────────────────────── */

dom_ctx *dom_create(void);
dom_ctx *dom_create_with_config(const dom_config_t *cfg);
void     dom_destroy(dom_ctx *ctx);
void     dom_reset(dom_ctx *ctx);

/**
 * Feed raw input bytes into the parser state-machine.
 * Returns 0 on success, -1 on error (oversized input, etc.).
 */
int dom_feed_input(dom_ctx *ctx, const char *data, size_t len);

/**
 * Dequeue the next event. Returns 1 if an event was dequeued, 0 if
 * the queue is empty.
 */
int dom_next_event(dom_ctx *ctx, dom_event_t *out);

/**
 * Retrieve serialised output from a serializer context.
 * Returns pointer to internal buffer; *out_len receives byte count.
 */
const char *dom_get_output(const dom_ctx *ctx, size_t *out_len);

/* ── Status helpers ────────────────────────────────────────────────── */

/** Current parser state (IDLE, TAG_OPEN, etc. — see source). */
int      dom_parser_state(const dom_ctx *ctx);
/** Current nesting depth. */
uint32_t dom_depth(const dom_ctx *ctx);
/** 1 if events are waiting in the queue. */
int      dom_has_pending_events(const dom_ctx *ctx);
/** Number of events produced so far (monotonic). */
uint64_t dom_event_count(const dom_ctx *ctx);

/* ── Serializer helpers ────────────────────────────────────────────── */

int dom_emit_element_open(dom_ctx *ctx, const dom_element_t *el);
int dom_emit_element_close(dom_ctx *ctx, const char *tag, uint32_t tag_len);
int dom_emit_text(dom_ctx *ctx, const char *text, uint32_t text_len);
int dom_emit_comment(dom_ctx *ctx, const char *text, uint32_t text_len);

/* ── WASM bridge ───────────────────────────────────────────────────── */

int dom_wasm_set_attribute(dom_ctx *ctx, uint32_t node_id,
                           const char *key, const char *value);
int dom_wasm_remove_attribute(dom_ctx *ctx, uint32_t node_id,
                              const char *key);
int dom_wasm_set_inner_html(dom_ctx *ctx, uint32_t node_id,
                            const char *html);
int dom_wasm_set_text_content(dom_ctx *ctx, uint32_t node_id,
                              const char *text);
int dom_wasm_remove_element(dom_ctx *ctx, uint32_t node_id);
int dom_wasm_add_class(dom_ctx *ctx, uint32_t node_id, const char *cls);
int dom_wasm_remove_class(dom_ctx *ctx, uint32_t node_id, const char *cls);
int dom_wasm_replace_element(dom_ctx *ctx, uint32_t node_id,
                             const char *html);

/* ── CSS helpers ───────────────────────────────────────────────────── */

/** Returns 1 if the simple selector matches the element. */
int dom_css_matches(const dom_element_t *el, const char *selector);

#ifdef __cplusplus
}
#endif

#endif /* DOM_H */