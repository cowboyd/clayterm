export interface Setting {
  apply: Uint8Array;
  revert: Uint8Array;
}

export function settings(...sequence: Setting[]): Setting {
  return {
    apply: concat(sequence.map((s) => s.apply)),
    revert: concat(sequence.map((s) => s.revert).reverse()),
  };
}

export function alternateBuffer(): Setting {
  return {
    apply: csi("?1049h"),
    revert: csi("?1049l"),
  };
}

export function cursor(visible: boolean): Setting {
  if (visible) {
    return {
      apply: csi("?25h"),
      revert: csi("?25l"),
    };
  } else {
    return {
      apply: csi("?25l"),
      revert: csi("?25h"),
    };
  }
}

export function progressiveInput(level: number): Setting {
  return {
    apply: csi(`>${level}u`),
    revert: csi("<u"),
  };
}

export function mouseTracking(): Setting {
  return {
    apply: concat([csi("?1003h"), csi("?1006h")]),
    revert: concat([csi("?1006l"), csi("?1003l")]),
  };
}

let encoder = new TextEncoder();

function encode(str: string): Uint8Array {
  return encoder.encode(str);
}

function csi(str: string): Uint8Array {
  return encode(`\x1b[${str}`);
}

function concat(arrays: Uint8Array[]): Uint8Array {
  let length = arrays.reduce((sum, a) => sum + a.length, 0);
  let result = new Uint8Array(length);
  let offset = 0;
  for (let a of arrays) {
    result.set(a, offset);
    offset += a.length;
  }
  return result;
}
