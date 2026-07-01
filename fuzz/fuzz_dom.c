/**
 * dom_fuzz — libFuzzer harness for libdom
 *
 * SPDX-License-Identifier: MIT
 */
#include "dom.h"
#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    dom_config_t cfg = dom_default_config();
    cfg.max_input_size = 1 << 20;   /* 1 MiB cap   */
    cfg.max_dom_depth  = 1024;
    cfg.parse_css      = 1;

    dom_ctx *ctx = dom_create_with_config(&cfg);
    if (!ctx) return 0;

    dom_feed_input(ctx, (const char *)data, size);

    dom_event_t ev;
    while (dom_next_event(ctx, &ev)) {
        /* Drain events — we only care about crashes / ASan findings */
    }

    dom_destroy(ctx);
    return 0;
}