/* clayterm.c — cell buffer, diff engine, escape gen, Clay render backend */

#include "clayterm.h"
#include "buffer.h"
#include "cell.h"
#include "clay/clay.h"
#include "utf8.h"
#include "wcwidth.h"

/* ── Instance state ───────────────────────────────────────────────── */

struct Clayterm {
  int w, h;
  Cell *front;
  Cell *back;
  Buffer out;
  uint32_t lastfg, lastbg;
  int lastx, lasty;
  /* clip region */
  int clipx, clipy, clipw, cliph;
  int clipping;
};

/* Memory layout inside the arena provided by the host:
 *   [Clayterm struct] [front cells] [back cells] [output buffer]
 *
 * Output buffer is sized at 64 bytes per cell — enough for worst-case
 * full-screen redraws with truecolor SGR sequences on every cell.
 */
#define OUT_BYTES_PER_CELL 64

/* ── Cell buffer ops ──────────────────────────────────────────────── */

static Cell *cell_at(struct Clayterm *ct, Cell *buf, int x, int y) {
  return &buf[y * ct->w + x];
}

static void setcell(struct Clayterm *ct, int x, int y, uint32_t ch, uint32_t fg,
                    uint32_t bg) {
  if (x < 0 || x >= ct->w || y < 0 || y >= ct->h)
    return;
  if (ct->clipping) {
    if (x < ct->clipx || x >= ct->clipx + ct->clipw)
      return;
    if (y < ct->clipy || y >= ct->clipy + ct->cliph)
      return;
  }
  Cell *c = cell_at(ct, ct->back, x, y);
  c->ch = ch;
  c->fg = fg;
  c->bg = bg;
}

/* ── Escape sequence generation ───────────────────────────────────── */

static void emit_attr(struct Clayterm *ct, uint32_t fg, uint32_t bg) {
  if (fg == ct->lastfg && bg == ct->lastbg)
    return;

  /* SGR reset */
  buf_str(&ct->out, "\x1b[0m");

  /* style attributes from fg high byte */
  if (fg & ATTR_BOLD)
    buf_str(&ct->out, "\x1b[1m");
  if (fg & ATTR_DIM)
    buf_str(&ct->out, "\x1b[2m");
  if (fg & ATTR_ITALIC)
    buf_str(&ct->out, "\x1b[3m");
  if (fg & ATTR_UNDERLINE)
    buf_str(&ct->out, "\x1b[4m");
  if (fg & ATTR_BLINK)
    buf_str(&ct->out, "\x1b[5m");
  if (fg & ATTR_REVERSE)
    buf_str(&ct->out, "\x1b[7m");
  if (fg & ATTR_STRIKEOUT)
    buf_str(&ct->out, "\x1b[9m");

  /* foreground truecolor */
  if (!(fg & ATTR_DEFAULT)) {
    buf_str(&ct->out, "\x1b[38;2;");
    buf_num(&ct->out, (fg >> 16) & 0xff);
    buf_put(&ct->out, ";", 1);
    buf_num(&ct->out, (fg >> 8) & 0xff);
    buf_put(&ct->out, ";", 1);
    buf_num(&ct->out, fg & 0xff);
    buf_put(&ct->out, "m", 1);
  }

  /* background truecolor */
  if (!(bg & ATTR_DEFAULT)) {
    buf_str(&ct->out, "\x1b[48;2;");
    buf_num(&ct->out, (bg >> 16) & 0xff);
    buf_put(&ct->out, ";", 1);
    buf_num(&ct->out, (bg >> 8) & 0xff);
    buf_put(&ct->out, ";", 1);
    buf_num(&ct->out, bg & 0xff);
    buf_put(&ct->out, "m", 1);
  }

  ct->lastfg = fg;
  ct->lastbg = bg;
}

static void emit_cursor(struct Clayterm *ct, int x, int y) {
  buf_str(&ct->out, "\x1b[");
  buf_num(&ct->out, y + 1);
  buf_put(&ct->out, ";", 1);
  buf_num(&ct->out, x + 1);
  buf_put(&ct->out, "H", 1);
}

static void emit_ch(struct Clayterm *ct, int x, int y, uint32_t ch) {
  if (ct->lastx != x - 1 || ct->lasty != y) {
    emit_cursor(ct, x, y);
  }
  ct->lastx = x;
  ct->lasty = y;

  if (!iswprint(ch))
    ch = 0xfffd;
  buf_char(&ct->out, ch);
}

/* ── Double-buffer diff (from termbox2 tb_present) ────────────────── */

static void present(struct Clayterm *ct) {
  ct->lastx = -1;
  ct->lasty = -1;

  for (int y = 0; y < ct->h; y++) {
    for (int x = 0; x < ct->w;) {
      Cell *back = cell_at(ct, ct->back, x, y);
      Cell *front = cell_at(ct, ct->front, x, y);

      int w = wcwidth(back->ch);
      if (w < 1)
        w = 1;

      if (cell_cmp(back, front)) {
        /* copy to front */
        *front = *back;

        emit_attr(ct, back->fg, back->bg);

        if (w > 1 && x >= ct->w - (w - 1)) {
          /* wide char doesn't fit, send spaces */
          for (int i = x; i < ct->w; i++)
            emit_ch(ct, i, y, ' ');
        } else {
          emit_ch(ct, x, y, back->ch);
          /* mark trailing cells of wide char as invalid in front
           * so they'll diff when overwritten by narrow chars */
          for (int i = 1; i < w; i++) {
            Cell *fw = cell_at(ct, ct->front, x + i, y);
            fw->ch = 0xffffffff;
            fw->fg = 0xffffffff;
            fw->bg = 0xffffffff;
          }
        }
      }
      x += w;
    }
  }
}

/* ── Color conversion ─────────────────────────────────────────────── */

static uint32_t color(Clay_Color c) {
  uint8_t r = (uint8_t)c.r;
  uint8_t g = (uint8_t)c.g;
  uint8_t b = (uint8_t)c.b;
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

/* ── Clay render backend ──────────────────────────────────────────── */

static void render_rect(struct Clayterm *ct, int x0, int y0, int x1, int y1,
                        Clay_RectangleRenderData *r) {
  uint32_t bg = color(r->backgroundColor);
  for (int y = y0; y < y1; y++)
    for (int x = x0; x < x1; x++)
      setcell(ct, x, y, ' ', ATTR_DEFAULT, bg);
}

static void render_text(struct Clayterm *ct, int x0, int y0,
                        Clay_TextRenderData *t) {
  uint32_t fg = color(t->textColor);

  /* text attrs are packed into the alpha channel by reduce() */
  uint32_t attrs = ((uint32_t)(uint8_t)t->textColor.a) << 24;
  fg |= attrs;

  const char *p = t->stringContents.chars;
  int rem = t->stringContents.length;
  int x = x0;

  while (rem > 0) {
    uint32_t cp;
    int n = utf8_decode(&cp, p);
    if (n <= 0) {
      n = 1;
      cp = 0xfffd;
    }
    int cw = wcwidth(cp);
    if (cw < 0)
      cw = 1;
    if (cw > 0) {
      setcell(ct, x, y0, cp, fg, ATTR_DEFAULT);
      x += cw;
    }
    p += n;
    rem -= n;
  }
}

static void render_border(struct Clayterm *ct, int x0, int y0, int x1, int y1,
                          Clay_BorderRenderData *b) {
  uint32_t fg = color(b->color);
  uint32_t bg = ATTR_DEFAULT;
  int top = b->width.top > 0;
  int bot = b->width.bottom > 0;
  int left = b->width.left > 0;
  int right = b->width.right > 0;

  /* corners — rounded when corner radius > 0 */
  uint32_t tl = b->cornerRadius.topLeft > 0 ? 0x256d : 0x250c;
  uint32_t tr = b->cornerRadius.topRight > 0 ? 0x256e : 0x2510;
  uint32_t bl = b->cornerRadius.bottomLeft > 0 ? 0x2570 : 0x2514;
  uint32_t br = b->cornerRadius.bottomRight > 0 ? 0x256f : 0x2518;

  if (top && left)
    setcell(ct, x0, y0, tl, fg, bg);
  if (top && right)
    setcell(ct, x1 - 1, y0, tr, fg, bg);
  if (bot && left)
    setcell(ct, x0, y1 - 1, bl, fg, bg);
  if (bot && right)
    setcell(ct, x1 - 1, y1 - 1, br, fg, bg);

  /* horizontal edges */
  if (top)
    for (int x = x0 + left; x < x1 - right; x++)
      setcell(ct, x, y0, 0x2500, fg, bg);
  if (bot)
    for (int x = x0 + left; x < x1 - right; x++)
      setcell(ct, x, y1 - 1, 0x2500, fg, bg);

  /* vertical edges */
  if (left)
    for (int y = y0 + top; y < y1 - bot; y++)
      setcell(ct, x0, y, 0x2502, fg, bg);
  if (right)
    for (int y = y0 + top; y < y1 - bot; y++)
      setcell(ct, x1 - 1, y, 0x2502, fg, bg);
}

static void walk(struct Clayterm *ct, Clay_RenderCommandArray cmds) {
  for (int32_t i = 0; i < cmds.length; i++) {
    Clay_RenderCommand *cmd = Clay_RenderCommandArray_Get(&cmds, i);
    Clay_BoundingBox box = cmd->boundingBox;
    int x0 = (int)box.x;
    int y0 = (int)box.y;
    int x1 = (int)(box.x + box.width);
    int y1 = (int)(box.y + box.height);

    switch (cmd->commandType) {
    case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
      render_rect(ct, x0, y0, x1, y1, &cmd->renderData.rectangle);
      break;
    case CLAY_RENDER_COMMAND_TYPE_TEXT:
      render_text(ct, x0, y0, &cmd->renderData.text);
      break;
    case CLAY_RENDER_COMMAND_TYPE_BORDER:
      render_border(ct, x0, y0, x1, y1, &cmd->renderData.border);
      break;
    case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
      ct->clipping = 1;
      ct->clipx = x0;
      ct->clipy = y0;
      ct->clipw = x1 - x0;
      ct->cliph = y1 - y0;
      break;
    case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
      ct->clipping = 0;
      break;
    default:
      break;
    }
  }
}

/* ── Public API ───────────────────────────────────────────────────── */

static int align8(int n) { return (n + 7) & ~7; }

static int align64(int n) { return (n + 63) & ~63; }

int clayterm_size(int w, int h) {
  int cell_count = w * h;
  int cell_bytes = cell_count * (int)sizeof(Cell);
  int out_bytes = cell_count * OUT_BYTES_PER_CELL;
  int clay_bytes = (int)Clay_MinMemorySize();
  return align8((int)sizeof(struct Clayterm)) + align8(cell_bytes) /* front */
         + align8(cell_bytes)                                      /* back */
         + align8(out_bytes)    /* output buffer */
         + align64(clay_bytes); /* Clay arena */
}

static void clay_error(Clay_ErrorData err) { (void)err; }

struct Clayterm *init(void *mem, int w, int h) {
  struct Clayterm *ct = (struct Clayterm *)mem;
  int cell_count = w * h;
  int cell_bytes = align8(cell_count * (int)sizeof(Cell));
  int out_bytes = align8(cell_count * OUT_BYTES_PER_CELL);
  char *base = (char *)mem + align8((int)sizeof(struct Clayterm));

  char *clay_mem = base + cell_bytes * 2 + out_bytes;
  int clay_bytes = align64((int)Clay_MinMemorySize());
  Clay_Arena arena =
      Clay_CreateArenaWithCapacityAndMemory(clay_bytes, clay_mem);
  Clay_Initialize(arena, (Clay_Dimensions){(float)w, (float)h},
                  (Clay_ErrorHandler){clay_error, 0});

  *ct = (struct Clayterm){
      .w = w,
      .h = h,
      .front = (Cell *)base,
      .back = (Cell *)(base + cell_bytes),
      .out = {base + cell_bytes * 2, 0, cell_count * OUT_BYTES_PER_CELL},
      .lastfg = 0xffffffff,
      .lastbg = 0xffffffff,
      .lastx = -1,
      .lasty = -1,
  };

  cells_clear(ct->front, w, h);
  cells_clear(ct->back, w, h);
  return ct;
}

void render(struct Clayterm *ct, Clay_RenderCommandArray cmds) {
  ct->out.length = 0;
  ct->lastfg = ct->lastbg = 0xffffffff;
  ct->lastx = ct->lasty = -1;

  cells_clear(ct->back, ct->w, ct->h);

  /* hide cursor */
  buf_str(&ct->out, "\x1b[?25l");

  /* walk Clay render commands into back buffer */
  walk(ct, cmds);

  /* diff front vs back, emit escape sequences */
  present(ct);

  /* show cursor */
  buf_str(&ct->out, "\x1b[?25h");
}

char *output(struct Clayterm *ct) { return ct->out.data; }

int length(struct Clayterm *ct) { return ct->out.length; }

void measure(int ret, int txt) {
  /* Read Clay_StringSlice from txt address.
   * Clay_StringSlice layout: { int32_t length, const char *chars, ... }
   * We only need length and chars. */
  int32_t slen = *(int32_t *)txt;
  const char *chars = *(const char **)(txt + 4);

  int w = 0;
  const char *p = chars;
  int rem = slen;
  while (rem > 0) {
    uint32_t cp;
    int n = utf8_decode(&cp, p);
    if (n <= 0) {
      n = 1;
    }
    int cw = wcwidth(cp);
    if (cw > 0)
      w += cw;
    p += n;
    rem -= n;
  }

  /* Write Clay_Dimensions { float width, float height } to ret */
  float *dims = (float *)ret;
  dims[0] = (float)w;
  dims[1] = 1.0f;
}
