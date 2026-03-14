/* ops.h — command buffer protocol for Clay layout commands */

#ifndef OPS_H
#define OPS_H

#include <stdint.h>

struct Clayterm;

/* Command buffer opcodes */
#define OP_BEGIN_LAYOUT  0x01
#define OP_OPEN_ELEMENT  0x02
#define OP_TEXT          0x03
#define OP_CLOSE_ELEMENT 0x04
#define OP_END_LAYOUT    0x05

/* Property group masks for OPEN_ELEMENT */
#define PROP_LAYOUT       0x01
#define PROP_BG_COLOR     0x02
#define PROP_CORNER_RADIUS 0x04
#define PROP_BORDER       0x08
#define PROP_CLIP         0x10
#define PROP_FLOATING     0x20

void reduce(struct Clayterm *ct, uint32_t *buf, int len);

#endif
