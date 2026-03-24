/* utf8.c — UTF-8 encode/decode
 * Extracted from termbox2 (https://github.com/termbox/termbox2)
 */

#include <stdint.h>

static const unsigned char utf8_length[256] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 1, 1};

static const unsigned char utf8_mask[6] = {0x7f, 0x1f, 0x0f, 0x07, 0x03, 0x01};

int utf8_len(char c) { return utf8_length[(unsigned char)c]; }

int utf8_decode(uint32_t *out, const char *c) {
  if (*c == '\0')
    return 0;

  int i;
  unsigned char len = utf8_len(*c);
  unsigned char mask = utf8_mask[len - 1];
  uint32_t result = c[0] & mask;
  for (i = 1; i < len && c[i] != '\0'; ++i) {
    result <<= 6;
    result |= c[i] & 0x3f;
  }

  if (i != len)
    return i * -1;

  *out = result;
  return (int)len;
}

int utf8_encode(char *out, uint32_t c) {
  int len = 0;
  int first;
  int i;

  if (c < 0x80) {
    first = 0;
    len = 1;
  } else if (c < 0x800) {
    first = 0xc0;
    len = 2;
  } else if (c < 0x10000) {
    first = 0xe0;
    len = 3;
  } else if (c < 0x200000) {
    first = 0xf0;
    len = 4;
  } else if (c < 0x4000000) {
    first = 0xf8;
    len = 5;
  } else {
    first = 0xfc;
    len = 6;
  }

  for (i = len - 1; i > 0; --i) {
    out[i] = (c & 0x3f) | 0x80;
    c >>= 6;
  }
  out[0] = c | first;
  out[len] = '\0';

  return len;
}
