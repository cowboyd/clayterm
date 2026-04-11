import { describe, expect, test } from "bun:test";
import {
  close,
  createTerm,
  fixed,
  grow,
  measureCellWidth,
  measureWrappedHeight,
  open,
  text,
  wrapText,
} from "../build/npm/esm/mod.js";

describe("built clayterm artifacts", () => {
  test("rebuilt npm bundle exports measurement helpers", () => {
    expect(measureCellWidth("abc")).toBe(3);
    expect(measureCellWidth("e\u0301")).toBe(1);

    const wrapped = wrapText("hello world", 5);
    expect(wrapped.length).toBe(measureWrappedHeight("hello world", 5));
    expect(wrapped[0]?.text.length).toBeGreaterThan(0);
  });

  test("rebuilt npm bundle exposes term geometry queries backed by wasm", async () => {
    const term = await createTerm({ width: 20, height: 8 });

    term.render([
      open("root", { layout: { width: grow(), height: grow(), direction: "ttb" } }),
      open("viewport", { layout: { width: fixed(10), height: fixed(3) } }),
      text("Body"),
      close(),
      close(),
    ]);

    expect(term.getElementBounds("viewport")).toEqual({
      x: 0,
      y: 0,
      width: 10,
      height: 3,
    });
  });
});
