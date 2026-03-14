/* utf8.h — UTF-8 encode/decode */

#ifndef UTF8_H
#define UTF8_H

#include <stdint.h>

int utf8_decode(uint32_t *out, const char *c);
int utf8_encode(char *out, uint32_t c);

#endif
