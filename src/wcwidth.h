/* wcwidth.h — Unicode character width lookup */

#ifndef WCWIDTH_H
#define WCWIDTH_H

#include <stdint.h>

int wcwidth(uint32_t ch);
int iswprint(uint32_t ch);

#endif
