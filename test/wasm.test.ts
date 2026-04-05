import { describe, expect, it } from "./suite.ts";
import { createTerm } from "../term.ts";
import { close, grow, open, text } from "../ops.ts";
import { setWasmSource } from "../wasm.ts";
import { print } from "./print.ts";

const decode = (bytes: Uint8Array) => new TextDecoder().decode(bytes);

describe("wasm source", () => {
  it("loads wasm from an explicit host-provided path", async () => {
    setWasmSource(new URL("../clayterm.wasm", import.meta.url));

    let term = await createTerm({ width: 20, height: 5 });
    let output = print(
      decode(
        term.render([
          open("root", {
            layout: { width: grow(), height: grow(), direction: "ttb" },
          }),
          text("Hello"),
          close(),
        ]).output,
      ),
      20,
      5,
    );

    expect(output).toContain("Hello");
  });
});
