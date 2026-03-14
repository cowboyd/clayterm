/* cell.c — cell buffer operations */

#include "cell.h"

void cells_clear(Cell *buf, int w, int h) {
  for (int i = 0; i < w * h; i++) {
    buf[i].ch = ' ';
    buf[i].fg = ATTR_DEFAULT;
    buf[i].bg = ATTR_DEFAULT;
  }
}

int cell_cmp(Cell *a, Cell *b) {
  return a->ch != b->ch || a->fg != b->fg || a->bg != b->bg;
}
