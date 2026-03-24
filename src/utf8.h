/* utf8.h — UTF-8 encode/decode */

#ifndef UTF8_H
#define UTF8_H

#include <stdint.h>

/**
 * Return the byte length of a UTF-8 sequence given its leading byte.
 *
 * @param c  Leading byte of a UTF-8 sequence.
 * @return   Number of bytes in the sequence (1–6).
 */
int utf8_len(char c);

/**
 * Decode a UTF-8 sequence into a Unicode codepoint.
 *
 * @param out  Output codepoint.
 * @param c    Pointer to the start of a UTF-8 sequence.
 * @return     Number of bytes consumed (1–6), or negative on error.
 *             Returns 0 if c points to a null terminator.
 */
int utf8_decode(uint32_t *out, const char *c);

/**
 * Encode a Unicode codepoint as UTF-8.
 *
 * @param out  Output buffer (must hold at least 7 bytes including null).
 * @param c    Unicode codepoint.
 * @return     Number of bytes written (1–6), excluding null terminator.
 */
int utf8_encode(char *out, uint32_t c);

#endif
