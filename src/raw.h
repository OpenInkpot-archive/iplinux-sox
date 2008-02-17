/*
 * libSoX raw file formats
 *
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Lance Norskog And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

#define RAW_FORMAT0(id, size, flags, encoding) \
static int id ## _start(sox_format_t * ft) { \
  return sox_rawstart(ft, sox_true, sox_true, SOX_ENCODING_ ## encoding, SOX_SIZE_ ## size); \
} \
const sox_format_handler_t *sox_ ## id ## _format_fn(void); \
const sox_format_handler_t *sox_ ## id ## _format_fn(void) { \
  static sox_format_handler_t handler = { \
    names, flags, \
    id ## _start, sox_rawread , NULL, \
    id ## _start, sox_rawwrite, NULL, \
    NULL \
  }; \
  return &handler; \
}

#define RAW_FORMAT(id, size, flags, encoding) \
  static char const *names[] = {#id, NULL}; \
  RAW_FORMAT0(id, size, flags, encoding)

#define RAW_FORMAT1(id, alt, size, flags, encoding) \
  static char const *names[] = {#id, alt, NULL}; \
  RAW_FORMAT0(id, size, flags, encoding)

#define RAW_FORMAT2(id, alt1, alt2, size, flags, encoding) \
  static char const *names[] = {#id, alt1, alt2, NULL}; \
  RAW_FORMAT0(id, size, flags, encoding)

#define RAW_FORMAT3(id, alt1, alt2, alt3, size, flags, encoding) \
  static char const *names[] = {#id, alt1, alt2, alt3, NULL}; \
  RAW_FORMAT0(id, size, flags, encoding)
