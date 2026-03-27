/**
 * Terminal input parser.
 *
 * Thin wrapper around WASM `input_*` functions that decode raw VT/ANSI
 * escape sequences into structured events. All parsing logic, state
 * management, and buffering lives in WASM.
 */

import {
  createInputNative,
  EVENT_KEY,
  EVENT_MOUSE,
  EVENT_RESIZE,
  KEY_ARROW_DOWN,
  KEY_ARROW_LEFT,
  KEY_ARROW_RIGHT,
  KEY_ARROW_UP,
  KEY_BACKSPACE,
  KEY_BACKTAB,
  KEY_DELETE,
  KEY_END,
  KEY_ENTER,
  KEY_ESC,
  KEY_F1,
  KEY_F10,
  KEY_F11,
  KEY_F12,
  KEY_F2,
  KEY_F3,
  KEY_F4,
  KEY_F5,
  KEY_F6,
  KEY_F7,
  KEY_F8,
  KEY_F9,
  KEY_HOME,
  KEY_INSERT,
  KEY_MOUSE_LEFT,
  KEY_MOUSE_MIDDLE,
  KEY_MOUSE_RELEASE,
  KEY_MOUSE_RIGHT,
  KEY_MOUSE_WHEEL_DOWN,
  KEY_MOUSE_WHEEL_UP,
  KEY_PGDN,
  KEY_PGUP,
  KEY_TAB,
  MAX_TERMINFO,
  MOD_ALT,
  MOD_CTRL,
  MOD_MOTION,
  MOD_RELEASE,
  MOD_SHIFT,
  type NativeInputEvent,
  readEvent,
  SCAN_BUFFER_SIZE,
} from "./input-native.ts";

/**
 * Modifier keys held during a key or mouse event.
 */
export interface KeyModifiers {
  alt?: true;
  ctrl?: true;
  shift?: true;
}

/**
 * Shared key information present on all keyboard events.
 */
export interface KeyInfo extends KeyModifiers {
  key: string;
}

/**
 * A key was pressed. Emitted at all enhancement levels.
 * On legacy terminals, this is the only keyboard event type.
 */
export interface KeyDown extends KeyInfo {
  type: "keydown";
  shifted?: string;
  base?: string;
  text?: string;
}

/**
 * A key is being held down (auto-repeat). Only emitted with
 * Kitty enhancement level 2+ (report event types).
 */
export interface KeyRepeat extends KeyInfo {
  type: "keyrepeat";
  shifted?: string;
  base?: string;
  text?: string;
}

/**
 * A key was released. Only emitted with Kitty enhancement
 * level 2+ (report event types). Does not carry text,
 * shifted, or base fields.
 */
export interface KeyUp extends KeyInfo {
  type: "keyup";
}

export type KeyEvent = KeyDown | KeyRepeat | KeyUp;

/**
 * A mouse button was pressed or released
 */
export interface MouseEvent extends KeyModifiers {
  type: "mouse";
  /**
   * Which mouse button triggered this event.
   *
   * Note: "release" is not technically a button, but is used by
   * `VT200` and `urxvt` protocols where the terminal reports a button
   * release without indicating which button.
   */
  button: "left" | "right" | "middle" | "release";

  /**
   * `x` coordinate of this event
   */
  x: number;

  /**
   * `y` coordinate of this event
   */
  y: number;

  /**
   * True if the event represents a button being released.
   */
  release?: true;
}

/**
 * Mouse movement while a button is held.
 */
export interface DragEvent extends KeyModifiers {
  type: "drag";
  /**
   * Which mouse button is being held during the drag.
   */
  button: "left" | "right" | "middle";
  /**
   * Cursor column (0-based).
   */
  x: number;
  /**
   * Cursor row (0-based).
   */
  y: number;
}

/**
 * A scroll wheel tick.
 */
export interface WheelEvent extends KeyModifiers {
  type: "wheel";
  /**
   * Did the wheel move up or down
   */
  direction: "up" | "down";

  /**
   * Cursor column at the time of the scroll (0-based).
   */
  x: number;

  /**
   * Cursor row at the time of the scroll (0-based).
   */
  y: number;
}

/**
 * Terminal resize notification.
 */
export interface ResizeEvent {
  type: "resize";

  /**
   * New terminal width in columns.
   */
  width: number;

  /**
   * New terminal height in rows.
   */
  height: number;
}

export type InputEvent =
  | KeyEvent
  | MouseEvent
  | DragEvent
  | WheelEvent
  | ResizeEvent;

/**
 * Result of a single scan() call.
 *
 * When `pending` is present, a lone ESC is buffered and the caller should
 * re-call scan() with an empty buffer after `pending.delay` milliseconds.
 */
export interface ScanResult {
  events: InputEvent[];
  pending?: { delay: number };
}

export interface Input {
  /**
   * Feed raw bytes from stdin into the parser and return any events
   * produced. Call with no arguments to flush a pending ESC after the
   * latency period has elapsed.
   *
   * @example
   * ```ts
   * let { events, pending } = input.scan(bytes);
   * for (let event of events) {
   *   dispatch(event);
   * }
   * if (pending) {
   *   // there is a pending ESC event. wait for the delay
   *   await sleep(pending.delay);
   *
   *   // re-scan
   *   let flush = input.scan();
   *
   *   //dispatch the flushed ESC
   *   for (let event of flush.events) {
   *     dispatch(event)
   *   }
   * }
   * ```
   */
  scan(bytes?: Uint8Array): ScanResult;
}

export interface InputOptions {
  /**
   * Milliseconds to wait before resolving a lone ESC byte as the Escape
   * key rather than the start of an escape sequence. Lower values feel
   * snappier but risk misinterpreting sequences on slow connections.
   *
   * For reference, Vim's `ttimeoutlen` defaults to 100ms and ncurses
   * `ESCDELAY` defaults to 1000ms. The default of 25ms is tuned for
   * local terminals where escape sequences arrive within microseconds.
   *
   * @default 25
   */
  escLatency?: number;

  /**
   * Compiled terminfo binary to load terminal-specific escape sequences.
   *
   * This is the format used by files like /usr/lib/terminfo/78/xterm-256color
   * and they can be directly loaded from disk into this option.
   *
   * If no terminfo is provided it will use xterm capabilities as the default
   */
  terminfo?: Uint8Array;
}

export async function createInput(options: InputOptions = {}): Promise<Input> {
  let { escLatency = 25, terminfo } = options;

  if (terminfo && terminfo.byteLength > MAX_TERMINFO) {
    throw new RangeError(
      `terminfo exceeds ${MAX_TERMINFO} byte limit (got ${terminfo.byteLength})`,
    );
  }

  let native = await createInputNative(escLatency);

  return {
    scan(bytes: Uint8Array = new Uint8Array(0)): ScanResult {
      let now = Date.now();
      let events: InputEvent[] = [];
      let offset = 0;

      while (offset < bytes.length || (offset === 0 && bytes.length === 0)) {
        let chunk = bytes.subarray(offset, offset + SCAN_BUFFER_SIZE);
        if (chunk.length > 0) {
          new Uint8Array(native.memory.buffer).set(chunk, native.buffer);
        }

        let accepted = native.scan(
          native.state,
          native.buffer,
          chunk.length,
          now,
        );

        let count = native.count(native.state);
        let view = new DataView(native.memory.buffer);
        for (let i = 0; i < count; i++) {
          let ptr = native.event(native.state, i);
          if (ptr !== 0) {
            events.push(mapEvent(readEvent(view, ptr)));
          }
        }

        offset += accepted;

        if (accepted < chunk.length) {
          break;
        }
        if (bytes.length === 0) {
          break;
        }
      }

      let delay = native.delay(native.state);
      if (delay > 0) {
        return { events, pending: { delay } };
      }
      return { events };
    },
  };
}

const KEY_NAMES = new Map<number, string>([
  [KEY_F1, "F1"],
  [KEY_F2, "F2"],
  [KEY_F3, "F3"],
  [KEY_F4, "F4"],
  [KEY_F5, "F5"],
  [KEY_F6, "F6"],
  [KEY_F7, "F7"],
  [KEY_F8, "F8"],
  [KEY_F9, "F9"],
  [KEY_F10, "F10"],
  [KEY_F11, "F11"],
  [KEY_F12, "F12"],
  [KEY_ARROW_UP, "ArrowUp"],
  [KEY_ARROW_DOWN, "ArrowDown"],
  [KEY_ARROW_LEFT, "ArrowLeft"],
  [KEY_ARROW_RIGHT, "ArrowRight"],
  [KEY_HOME, "Home"],
  [KEY_END, "End"],
  [KEY_INSERT, "Insert"],
  [KEY_DELETE, "Delete"],
  [KEY_PGUP, "PageUp"],
  [KEY_PGDN, "PageDown"],
  [KEY_BACKTAB, "Backtab"],
  [KEY_BACKSPACE, "Backspace"],
  [KEY_TAB, "Tab"],
  [KEY_ENTER, "Enter"],
  [KEY_ESC, "Escape"],
]);

const BUTTON_NAMES = new Map<number, MouseEvent["button"]>([
  [KEY_MOUSE_LEFT, "left"],
  [KEY_MOUSE_RIGHT, "right"],
  [KEY_MOUSE_MIDDLE, "middle"],
  [KEY_MOUSE_RELEASE, "release"],
]);

function mods(native: NativeInputEvent): KeyModifiers {
  let m: KeyModifiers = {};
  if (native.mod & MOD_ALT) m.alt = true;
  if (native.mod & MOD_CTRL) m.ctrl = true;
  if (native.mod & MOD_SHIFT) m.shift = true;
  return m;
}

function keyName(native: NativeInputEvent): string {
  let name = KEY_NAMES.get(native.key);
  if (name) {
    return name;
  } else if (native.key === 0 && native.ch > 0) {
    return String.fromCodePoint(native.ch);
  } else if (native.key > 0 && native.key < 0x20) {
    return String.fromCharCode(native.key + 0x60);
  } else {
    return String.fromCodePoint(native.ch || native.key);
  }
}

function textFromNative(native: NativeInputEvent): string | undefined {
  if (native.text.length === 0) {
    return undefined;
  } else {
    return String.fromCodePoint(...native.text);
  }
}

function mapKeyEvent(native: NativeInputEvent): KeyEvent {
  let key = keyName(native);
  let m = mods(native);
  let isChar = !KEY_NAMES.has(native.key);
  let text = textFromNative(native);

  if (native.action === 3) {
    return { type: "keyup", key, ...m };
  }

  let type: "keydown" | "keyrepeat" = native.action === 2
    ? "keyrepeat"
    : "keydown";

  let ev: KeyDown | KeyRepeat = { type, key, ...m };

  if (native.shifted > 0) ev.shifted = String.fromCodePoint(native.shifted);
  if (native.base > 0) ev.base = String.fromCodePoint(native.base);
  if (text) {
    ev.text = text;
  } else if (isChar && type === "keydown" && native.ch > 0) {
    ev.text = String.fromCodePoint(native.ch);
  }

  return ev;
}

function mapEvent(native: NativeInputEvent): InputEvent {
  switch (native.type) {
    case EVENT_KEY: {
      return mapKeyEvent(native);
    }
    case EVENT_MOUSE: {
      if (
        native.key === KEY_MOUSE_WHEEL_UP || native.key === KEY_MOUSE_WHEEL_DOWN
      ) {
        return {
          type: "wheel",
          direction: native.key === KEY_MOUSE_WHEEL_UP ? "up" : "down",
          x: native.x,
          y: native.y,
          ...mods(native),
        };
      }
      if (native.mod & MOD_MOTION) {
        let button = BUTTON_NAMES.get(native.key) ?? "left";
        return {
          type: "drag" as const,
          button: button === "release" ? "left" as const : button,
          x: native.x,
          y: native.y,
          ...mods(native),
        };
      }
      let button = BUTTON_NAMES.get(native.key) ?? "left";
      let m = mods(native);
      let ev: MouseEvent = {
        type: "mouse",
        button,
        x: native.x,
        y: native.y,
        ...m,
      };
      if (native.mod & MOD_RELEASE) ev.release = true;
      return ev;
    }
    case EVENT_RESIZE: {
      return { type: "resize", width: native.w, height: native.h };
    }
    default: {
      return mapKeyEvent(native);
    }
  }
}
