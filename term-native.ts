export interface Native {
  memory: WebAssembly.Memory;
  statePtr: number;
  opsBuf: number;
  reduce(ct: number, buf: number, len: number): void;
  output(ct: number): number;
  length(ct: number): number;
}

import { compiled } from "./wasm.ts";

export async function createTermNative(w: number, h: number): Promise<Native> {
  let memory = new WebAssembly.Memory({ initial: 256 });
  let exports: Record<string, CallableFunction> = {};

  let instance = await WebAssembly.instantiate(compiled, {
    env: { memory },
    clay: {
      measureTextFunction(
        ret: number,
        text: number,
        _config: number,
        _userData: number,
      ) {
        exports.measure(ret, text);
      },
      queryScrollOffsetFunction(
        ret: number,
        _elementId: number,
        _userData: number,
      ) {
        let view = new DataView(memory.buffer);
        view.setFloat32(ret, 0, true);
        view.setFloat32(ret + 4, 0, true);
      },
    },
  });

  Object.assign(exports, instance.exports);

  let ct = exports as unknown as {
    __heap_base: WebAssembly.Global;
    clayterm_size(w: number, h: number): number;
    init(mem: number, w: number, h: number): number;
    reduce(ct: number, buf: number, len: number): void;
    output(ct: number): number;
    length(ct: number): number;
  };

  let heap = ct.__heap_base.value as number;
  let size = ct.clayterm_size(w, h);
  let statePtr = ct.init(heap, w, h);
  let opsBuf = (heap + size + 3) & ~3;

  return {
    memory,
    statePtr,
    opsBuf,
    reduce: ct.reduce,
    output: ct.output,
    length: ct.length,
  };
}
