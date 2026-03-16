import { type Op, pack } from "./ops.ts";
import { load } from "./native.ts";

export interface TermOptions {
  height: number;
  width: number;
}

export interface Term {
  render(ops: Op[]): Uint8Array;
}

export async function createTerm(options: TermOptions): Promise<Term> {
  const { width, height } = options;
  const { memory, statePtr, opsBuf, reduce, output, length } = await load(
    width,
    height,
  );

  return {
    render(ops: Op[]): Uint8Array {
      const len = pack(ops, memory.buffer, opsBuf);
      reduce(statePtr, opsBuf, len);
      return new Uint8Array(
        memory.buffer,
        output(statePtr),
        length(statePtr),
      );
    },
  };
}
