/* ops.c — command buffer reducer for Clay layout commands */

#include "ops.h"
#include "clayterm.h"
#include "clay/clay.h"

/* ── Command buffer helpers ────────────────────────────────────────── */

static uint32_t rd(uint32_t *buf, int len, int *i) {
  if (*i < len)
    return buf[(*i)++];
  return 0;
}

static float rdf(uint32_t *buf, int len, int *i) {
  uint32_t v = rd(buf, len, i);
  float f;
  __builtin_memcpy(&f, &v, 4);
  return f;
}

static Clay_Color unpack_color(uint32_t c) {
  return (Clay_Color){
      .r = (float)((c >> 16) & 0xff),
      .g = (float)((c >> 8) & 0xff),
      .b = (float)(c & 0xff),
      .a = (float)((c >> 24) & 0xff),
  };
}

static Clay_SizingAxis decode_axis(uint32_t *buf, int len, int *i) {
  uint32_t type = rd(buf, len, i);
  float a = rdf(buf, len, i);
  float b = rdf(buf, len, i);
  Clay_SizingAxis axis = {0};
  switch (type) {
  case 0: /* FIT */
    axis.type = CLAY__SIZING_TYPE_FIT;
    axis.size.minMax.min = a;
    axis.size.minMax.max = b;
    break;
  case 1: /* GROW */
    axis.type = CLAY__SIZING_TYPE_GROW;
    axis.size.minMax.min = a;
    axis.size.minMax.max = b;
    break;
  case 2: /* PERCENT */
    axis.type = CLAY__SIZING_TYPE_PERCENT;
    axis.size.percent = a;
    break;
  case 3: /* FIXED */
    axis.type = CLAY__SIZING_TYPE_FIXED;
    axis.size.minMax.min = a;
    axis.size.minMax.max = a;
    break;
  }
  return axis;
}

/* ── Command buffer reducer ───────────────────────────────────────── */

void reduce(struct Clayterm *ct, uint32_t *buf, int len) {
  int i = 0;
  uint32_t idx = 0;

  Clay_BeginLayout();

  while (i < len) {
    uint32_t op = rd(buf, len, &i);

    switch (op) {
    case OP_OPEN_ELEMENT: {
      /* read id string */
      uint32_t id_len = rd(buf, len, &i);
      int id_words = (id_len + 3) / 4;
      char *id_chars = (char *)&buf[i];
      i += id_words;

      if (id_len > 0) {
        Clay_String str = {.length = (int32_t)id_len, .chars = id_chars};
        Clay_ElementId eid = Clay__HashString(str, idx++);
        Clay__OpenElementWithId(eid);
      } else {
        Clay__OpenElement();
      }

      /* read property mask */
      uint32_t mask = rd(buf, len, &i);
      Clay_ElementDeclaration decl = {0};

      if (mask & PROP_LAYOUT) {
        decl.layout.sizing.width = decode_axis(buf, len, &i);
        decl.layout.sizing.height = decode_axis(buf, len, &i);

        uint32_t pad = rd(buf, len, &i);
        decl.layout.padding.left = pad & 0xff;
        decl.layout.padding.right = (pad >> 8) & 0xff;
        decl.layout.padding.top = (pad >> 16) & 0xff;
        decl.layout.padding.bottom = (pad >> 24) & 0xff;

        uint32_t gd = rd(buf, len, &i);
        decl.layout.childGap = gd & 0xffff;
        decl.layout.layoutDirection = (gd >> 16) & 0xff;

        uint32_t al = rd(buf, len, &i);
        decl.layout.childAlignment.x = al & 0xff;
        decl.layout.childAlignment.y = (al >> 8) & 0xff;
      }

      if (mask & PROP_BG_COLOR) {
        decl.backgroundColor = unpack_color(rd(buf, len, &i));
      }

      if (mask & PROP_CORNER_RADIUS) {
        uint32_t cr = rd(buf, len, &i);
        decl.cornerRadius.topLeft = (float)(cr & 0xff);
        decl.cornerRadius.topRight = (float)((cr >> 8) & 0xff);
        decl.cornerRadius.bottomLeft = (float)((cr >> 16) & 0xff);
        decl.cornerRadius.bottomRight = (float)((cr >> 24) & 0xff);
      }

      if (mask & PROP_BORDER) {
        decl.border.color = unpack_color(rd(buf, len, &i));

        uint32_t bw = rd(buf, len, &i);
        decl.border.width.left = bw & 0xff;
        decl.border.width.right = (bw >> 8) & 0xff;
        decl.border.width.top = (bw >> 16) & 0xff;
        decl.border.width.bottom = (bw >> 24) & 0xff;
      }

      if (mask & PROP_CLIP) {
        uint32_t cl = rd(buf, len, &i);
        decl.clip.horizontal = cl & 0xff;
        decl.clip.vertical = (cl >> 8) & 0xff;
      }

      if (mask & PROP_FLOATING) {
        decl.floating.offset.x = rdf(buf, len, &i);
        decl.floating.offset.y = rdf(buf, len, &i);
        decl.floating.parentId = rd(buf, len, &i);

        uint32_t fc = rd(buf, len, &i);
        decl.floating.attachTo = fc & 0xff;
        decl.floating.attachPoints.element = (fc >> 8) & 0xff;
        decl.floating.attachPoints.parent = (fc >> 16) & 0xff;
        decl.floating.zIndex = (int16_t)((fc >> 24) & 0xff);
      }

      Clay__ConfigureOpenElement(decl);
      break;
    }

    case OP_TEXT: {
      uint32_t col = rd(buf, len, &i);
      uint32_t cfg = rd(buf, len, &i);
      uint32_t str_len = rd(buf, len, &i);
      int str_words = (str_len + 3) / 4;
      char *str_chars = (char *)&buf[i];
      i += str_words;

      Clay_String text = {.length = (int32_t)str_len, .chars = str_chars};

      Clay_TextElementConfig config = {0};
      config.textColor = unpack_color(col);
      config.fontSize = cfg & 0xff;
      config.fontId = (cfg >> 8) & 0xff;
      config.wrapMode = (cfg >> 16) & 0xff;
      /* attrs byte → alpha channel for render_text to extract */
      config.textColor.a = (float)((cfg >> 24) & 0xff);

      Clay__OpenTextElement(text, Clay__StoreTextElementConfig(config));
      break;
    }

    case OP_CLOSE_ELEMENT:
      Clay__CloseElement();
      break;

    default:
      break;
    }
  }

  Clay_RenderCommandArray cmds = Clay_EndLayout();
  render(ct, cmds);
}
