import { type Op, pack } from "./ops.ts";
import { createTermNative } from "./term-native.ts";

export interface TermOptions {
  height: number;
  width: number;
}

export interface Term {
  render(ops: Op[]): Uint8Array;
}

export async function createTerm(options: TermOptions): Promise<Term> {
  let { width, height } = options;
  let { memory, statePtr, opsBuf, reduce, output, length } =
    await createTermNative(
      width,
      height,
    );

  return {
    render(ops: Op[]): Uint8Array {
      let len = pack(ops, memory.buffer, opsBuf, memory.buffer.byteLength);
      reduce(statePtr, opsBuf, len);
      return new Uint8Array(
        memory.buffer,
        output(statePtr),
        length(statePtr),
      );
    },
  };
}
