# clayterm

A terminal rendering backend for [Clay](https://github.com/nicbarker/clay),
compiled to WebAssembly.

With every frame, the entire UI tree is packed into a flat byte array and sent
to WASM in a single call. On the C side, Clay runs layout, render commands are
walked into a cell buffer, and the buffer is diffed against the previous frame.
Only the cells that actually changed produce output. The result is an ANSI
escape sequence that can be written directly to stdout. One trip to WASM per
frame, double buffered, and only the bytes that need to change hit the output
stream.

Because the WASM module is pure computation with no I/O, it runs anywhere
WebAssembly does: Deno, Node, Bun, browsers, or any other runtime.

```
 TypeScript                        WASM (C)
+---------------+                +---------------------------+
|               |  Uint32Array   |                           |
| UI ops...     | =============> | Clay layout               |
|               |                |   -> render commands      |
+---------------+                |   -> cell buffer (back)   |
                                 |   -> diff against (front) |
                                 |   -> escape bytes         |
+---------------+                |                           |
|               | ANSI byte array|                           |
| stdout.write  | <============= |                           |
|               |                |                           |
+---------------+                +---------------------------+
```

## Usage

```typescript
import { close, createTerm, grow, open, rgba, text } from "clayterm";

const term = await createTerm({ width: 80, height: 24 });

const ansi = term.render([
  open("root", {
    layout: { width: grow(), height: grow(), direction: "ttb" },
  }),
  open("box", {
    layout: { padding: { left: 2, top: 1 } },
    border: {
      color: rgba(0, 255, 0),
      left: 1,
      right: 1,
      top: 1,
      bottom: 1,
    },
    cornerRadius: { tl: 1, tr: 1, bl: 1, br: 1 },
  }),
  text("Hello, World!"),
  close(),
  close(),
]);

Deno.stdout.writeSync(ansi);
```

## Development

Requires `clang` with wasm32 target support.

First build the `.wasm`

```sh
make
```

run tests

```sh
deno task test
```
