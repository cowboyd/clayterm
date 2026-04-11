const COMBINING_MARK_RE = /\p{Mark}/u;
const EXTENDED_PICTOGRAPHIC_RE = /\p{Extended_Pictographic}/u;

export type WrapTextMode = "words" | "newlines" | "none";

export interface WrapTextOptions {
  mode?: WrapTextMode;
}

export interface WrappedLine {
  text: string;
  width: number;
}

export interface TextMeasureApi {
  measureCellWidth(text: string): number;
  wrapText(text: string, width: number, options?: WrapTextOptions): WrappedLine[];
  measureWrappedHeight(text: string, width: number, options?: WrapTextOptions): number;
}

function assertWidth(width: number): void {
  if (!Number.isFinite(width) || width < 0) {
    throw new RangeError(`width must be a finite, non-negative number; received ${width}`);
  }
}

export function measureCellWidth(text: string): number {
  let width = 0;
  for (const symbol of text) {
    width += codePointWidth(symbol);
  }
  return width;
}

export function wrapText(
  text: string,
  width: number,
  options: WrapTextOptions = {},
): WrappedLine[] {
  assertWidth(width);
  if (text.length === 0 || width === 0) return [];

  const mode = options.mode ?? "words";

  switch (mode) {
    case "newlines":
      return text.split("\n").map((line) => ({ text: line, width: measureCellWidth(line) }));
    case "none": {
      const collapsed = text.replace(/\n/g, "");
      return collapsed.length === 0
        ? []
        : [{ text: collapsed, width: measureCellWidth(collapsed) }];
    }
    case "words":
      return wrapWords(text, width);
    default:
      return wrapWords(text, width);
  }
}

export function measureWrappedHeight(
  text: string,
  width: number,
  options: WrapTextOptions = {},
): number {
  assertWidth(width);
  if (text.length === 0 || width === 0) return 0;
  return wrapText(text, width, options).length;
}

function wrapWords(text: string, width: number): WrappedLine[] {
  const paragraphs = text.split("\n");
  const lines: WrappedLine[] = [];

  for (const paragraph of paragraphs) {
    if (paragraph.length === 0) {
      lines.push({ text: "", width: 0 });
      continue;
    }

    const tokens = paragraph.match(/\S+|\s+/g) ?? [paragraph];
    let currentText = "";
    let currentWidth = 0;

    for (const token of tokens) {
      const tokenWidth = measureCellWidth(token);
      if (currentText.length === 0) {
        currentText = token;
        currentWidth = tokenWidth;
        continue;
      }

      if (currentWidth + tokenWidth <= width) {
        currentText += token;
        currentWidth += tokenWidth;
        continue;
      }

      lines.push({ text: currentText, width: currentWidth });
      currentText = token;
      currentWidth = tokenWidth;
    }

    if (currentText.length > 0) {
      lines.push({ text: currentText, width: currentWidth });
    }
  }

  return lines;
}

function codePointWidth(symbol: string): number {
  const codePoint = symbol.codePointAt(0);
  if (codePoint === undefined) return 0;

  if (isControl(codePoint)) return 0;
  if (isZeroWidth(codePoint, symbol)) return 0;
  if (isWide(codePoint, symbol)) return 2;
  return 1;
}

function isControl(codePoint: number): boolean {
  return codePoint <= 0x1f || (codePoint >= 0x7f && codePoint < 0xa0);
}

function isZeroWidth(codePoint: number, symbol: string): boolean {
  return (
    codePoint === 0x200d ||
    (codePoint >= 0xfe00 && codePoint <= 0xfe0f) ||
    (codePoint >= 0xe0100 && codePoint <= 0xe01ef) ||
    (codePoint >= 0x1f3fb && codePoint <= 0x1f3ff) ||
    COMBINING_MARK_RE.test(symbol)
  );
}

function isWide(codePoint: number, symbol: string): boolean {
  return (
    EXTENDED_PICTOGRAPHIC_RE.test(symbol) ||
    codePoint >= 0x1100 && (
      codePoint <= 0x115f ||
      codePoint === 0x2329 ||
      codePoint === 0x232a ||
      (codePoint >= 0x2e80 && codePoint <= 0xa4cf && codePoint !== 0x303f) ||
      (codePoint >= 0xac00 && codePoint <= 0xd7a3) ||
      (codePoint >= 0xf900 && codePoint <= 0xfaff) ||
      (codePoint >= 0xfe10 && codePoint <= 0xfe19) ||
      (codePoint >= 0xfe30 && codePoint <= 0xfe6f) ||
      (codePoint >= 0xff00 && codePoint <= 0xff60) ||
      (codePoint >= 0xffe0 && codePoint <= 0xffe6) ||
      (codePoint >= 0x1f300 && codePoint <= 0x1f64f) ||
      (codePoint >= 0x1f900 && codePoint <= 0x1f9ff) ||
      (codePoint >= 0x20000 && codePoint <= 0x3fffd)
    )
  );
}
