import { describe, expect, test } from "bun:test";
import { createTerm } from "../term.ts";
import { close, fixed, grow, open, text } from "../ops.ts";

describe("term geometry", () => {
  test("getElementBounds returns bounds for rendered elements", async () => {
    const term = await createTerm({ width: 20, height: 8 });

    term.render([
      open("root", { layout: { width: grow(), height: grow(), direction: "ttb" } }),
      open("header", { layout: { width: grow(), height: fixed(1) } }),
      text("Header"),
      close(),
      open("viewport", { layout: { width: fixed(10), height: fixed(3) } }),
      text("Body"),
      close(),
      close(),
    ]);

    expect(term.getElementBounds("header")).toEqual({
      x: 0,
      y: 0,
      width: 20,
      height: 1,
    });
    expect(term.getElementBounds("viewport")).toEqual({
      x: 0,
      y: 1,
      width: 10,
      height: 3,
    });
  });

  test("getElementBounds returns undefined for unknown ids and before first render", async () => {
    const term = await createTerm({ width: 20, height: 8 });

    expect(term.getElementBounds("missing")).toBeUndefined();

    term.render([
      open("root", { layout: { width: grow(), height: grow(), direction: "ttb" } }),
      close(),
    ]);

    expect(term.getElementBounds("missing")).toBeUndefined();
  });

  test("getElementBounds updates after later renders", async () => {
    const term = await createTerm({ width: 20, height: 8 });

    term.render([
      open("box", { layout: { width: fixed(5), height: fixed(2) } }),
      close(),
    ]);
    expect(term.getElementBounds("box")).toEqual({ x: 0, y: 0, width: 5, height: 2 });

    term.render([
      open("box", { layout: { width: fixed(7), height: fixed(4) } }),
      close(),
    ]);
    expect(term.getElementBounds("box")).toEqual({ x: 0, y: 0, width: 7, height: 4 });
  });
});
