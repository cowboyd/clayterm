export interface Native {
  memory: WebAssembly.Memory;
  statePtr: number;
  opsBuf: number;
  reduce(ct: number, buf: number, len: number): void;
  output(ct: number): number;
  length(ct: number): number;
}

const wasm = await Deno.readFile(new URL("./clayterm.wasm", import.meta.url));
const compiled = await WebAssembly.compile(wasm);

export async function load(w: number, h: number): Promise<Native> {
  const memory = new WebAssembly.Memory({ initial: 256 });
  const exports: Record<string, CallableFunction> = {};

  const instance = await WebAssembly.instantiate(compiled, {
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
        const view = new DataView(memory.buffer);
        view.setFloat32(ret, 0, true);
        view.setFloat32(ret + 4, 0, true);
      },
    },
  });

  Object.assign(exports, instance.exports);

  const ct = exports as unknown as {
    __heap_base: WebAssembly.Global;
    clayterm_size(w: number, h: number): number;
    init(mem: number, w: number, h: number): number;
    reduce(ct: number, buf: number, len: number): void;
    output(ct: number): number;
    length(ct: number): number;
  };

  const heap = ct.__heap_base.value as number;
  const size = ct.clayterm_size(w, h);
  const statePtr = ct.init(heap, w, h);
  const opsBuf = (heap + size + 3) & ~3;

  return {
    memory,
    statePtr,
    opsBuf,
    reduce: ct.reduce,
    output: ct.output,
    length: ct.length,
  };
}
