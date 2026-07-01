# Domain — DOM, HTML, CSS, and WASM Bridge

## What is the DOM?

The Document Object Model (DOM) is a programming interface for web documents. It represents the structure of an HTML or XML document as a tree of nodes — elements, text, comments, and the document root.

## HTML Parsing

HTML is a markup language with a well-defined (but lenient) syntax:

- **Elements**: `<tag attr="value">content</tag>`
- **Self-closing**: `<br/>`, `<img src="x">` (void elements)
- **Comments**: `<!-- text -->`
- **Doctype**: `<!DOCTYPE html>`
- **Text**: any content not inside tags

libdom implements a **state-machine parser** that processes HTML byte-by-byte, emitting events as it encounters tokens.

### Parsing Workflow

1. Caller feeds raw bytes via `dom_feed_input()`.
2. State machine transitions through: IDLE → TAG_OPEN → TAG_NAME → TAG_ATTR_NAME → TAG_ATTR_VALUE → IDLE (or TEXT_CONTENT).
3. Events are pushed into a ring buffer.
4. Caller pulls events via `dom_next_event()`.

## CSS Extraction

CSS (Cascading Style Sheets) describes the visual presentation of HTML documents. When `cfg.parse_css = 1`, libdom extracts CSS rules from `<style>` blocks:

```css
body { color: red; }
h1 { font-size: 2em; }
```

Each rule is emitted as a `DOM_EVENT_CSS_RULE` with selector, property, and value fields.

### CSS Matching

`dom_css_matches()` supports simple selectors:
- **Tag**: `div` matches `<div>`
- **Class**: `.main` matches elements with `class="main"`
- **ID**: `#top` matches elements with `id="top"`
- **Combined**: `div.main` matches `<div class="main">`

## WASM Bridge

The WASM bridge provides a set of DOM mutation instructions that can be consumed by a WASM runtime:

| Operation | Description |
|---|---|
| `SET_ATTRIBUTE` | Set an attribute on a node |
| `REMOVE_ATTRIBUTE` | Remove an attribute |
| `SET_INNER_HTML` | Set inner HTML content |
| `SET_TEXT_CONTENT` | Set text content |
| `REMOVE_ELEMENT` | Remove a node |
| `ADD_CLASS` | Add a CSS class |
| `REMOVE_CLASS` | Remove a CSS class |
| `REPLACE_ELEMENT` | Replace a node with new HTML |

Each operation is queued as a `DOM_EVENT_WASM_INSTRUCTION` event with the opcode, target node ID, and key/value strings.

## Design Philosophy

- **No rendering**: libdom does not paint or layout.
- **No JavaScript**: no script execution.
- **No network I/O**: all input is caller-provided.
- **No full CSS cascade**: matching is limited to simple selectors.
- **No DOM tree**: libdom produces a flat event stream.