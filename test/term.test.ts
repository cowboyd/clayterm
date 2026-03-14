import { beforeEach, describe, expect, it } from "./suite.ts";
import { createTerm, type Term } from "../term.ts";
import { close, grow, open, rgba, text } from "../ops.ts";
import { print } from "./print.ts";

describe("term", () => {
  let term: Term;

  beforeEach(async () => {
    term = await createTerm({ width: 40, height: 10 });
  });

  it("renders hello world", () => {
    const out = print(term.render([
      open("root", {
        layout: { width: grow(), height: grow(), direction: "ttb" },
      }),
      text("Hello, World!"),
      close(),
    ]), 40, 10);

    expect(out).toContain("Hello, World!");
  });

  it("renders borders and padding", () => {
    const out = print(term.render([
      open("box", {
        layout: {
          width: grow(),
          height: grow(),
          padding: { left: 5, top: 5 },
          direction: "ttb",
        },
        border: {
          color: rgba(0, 255, 0),
          left: 1,
          right: 1,
          top: 1,
          bottom: 1,
        },
        cornerRadius: { tl: 1, tr: 1, bl: 1, br: 1 },
      }),
      text("padded"),
      close(),
    ]), 40, 10);

    expect(out).toEqual(`
╭──────────────────────────────────────╮
│                                      │
│                                      │
│                                      │
│                                      │
│    padded                            │
│                                      │
│                                      │
│                                      │
╰──────────────────────────────────────╯`.trim());
  });
});
