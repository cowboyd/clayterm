import { readFile } from "node:fs/promises";

const wasm = new Uint8Array(
  await readFile(new URL("./clayterm.wasm", import.meta.url)),
);

export const compiled = await WebAssembly.compile(wasm);
