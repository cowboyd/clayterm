import { readFile } from "node:fs/promises";

export type WasmSource =
  | ArrayBuffer
  | ArrayBufferView
  | Uint8Array
  | Promise<ArrayBuffer | ArrayBufferView | Uint8Array>
  | string
  | URL;

let wasmSource: WasmSource | undefined;
let compiledPromise: Promise<WebAssembly.Module> | undefined;

export function setWasmSource(source: WasmSource): void {
  wasmSource = source;
  compiledPromise = undefined;
}

async function loadDefaultWasm(): Promise<Uint8Array> {
  return new Uint8Array(await readFile(new URL("./clayterm.wasm", import.meta.url)));
}

async function resolveWasmBytes(source: WasmSource): Promise<Uint8Array> {
  let resolved = await source;

  if (typeof resolved === "string" || resolved instanceof URL) {
    return new Uint8Array(await readFile(resolved));
  }

  if (resolved instanceof Uint8Array) {
    return resolved;
  }

  if (ArrayBuffer.isView(resolved)) {
    return new Uint8Array(resolved.buffer, resolved.byteOffset, resolved.byteLength);
  }

  return new Uint8Array(resolved);
}

export async function getCompiledWasm(): Promise<WebAssembly.Module> {
  if (!compiledPromise) {
    compiledPromise = (async () => {
      let wasm = wasmSource ? await resolveWasmBytes(wasmSource) : await loadDefaultWasm();
      return await WebAssembly.compile(wasm);
    })();
  }

  return await compiledPromise;
}

export const compiled = getCompiledWasm();
