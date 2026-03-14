/* clayterm.h — WASM terminal rendering engine for Clay UI */

#ifndef CLAYTERM_H
#define CLAYTERM_H

#include "cell.h"

struct Clayterm;

/* WASM exports */
int clayterm_size(int w, int h);
struct Clayterm *init(void *mem, int w, int h);
void render(struct Clayterm *ct, Clay_RenderCommandArray cmds);
char *output(struct Clayterm *ct);
int length(struct Clayterm *ct);
void measure(int ret, int txt);

#endif
