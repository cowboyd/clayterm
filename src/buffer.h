/* buffer.h — fixed-capacity byte buffer */

#ifndef BUFFER_H
#define BUFFER_H

#include <stdint.h>

typedef struct {
  char *data;
  int length;
  int capacity;
} Buffer;

void buf_put(Buffer *b, const char *s, int n);
void buf_str(Buffer *b, const char *s);
void buf_num(Buffer *b, int n);
void buf_char(Buffer *b, uint32_t ch);

#endif
