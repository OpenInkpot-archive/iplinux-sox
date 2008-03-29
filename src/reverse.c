/* June 1, 1992
 * Copyright 1992 Guido van Rossum And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Guido van Rossum And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

/*
 * "reverse" effect, uses a temporary file created by tmpfile().
 */

#include "sox_i.h"
#include <string.h>

typedef struct {
  off_t         pos;
  FILE          * tmp_file;
} priv_t;
#define p ((priv_t *)effp->priv)

static int start(sox_effect_t * effp)
{
  p->pos = 0;
  p->tmp_file = tmpfile();
  if (p->tmp_file == NULL) {
    sox_fail("can't create temporary file: %s", strerror(errno));
    return SOX_EOF;
  }
  return SOX_SUCCESS;
}

static int flow(sox_effect_t * effp, const sox_sample_t * ibuf,
    sox_sample_t * obuf, sox_size_t * isamp, sox_size_t * osamp)
{
  if (fwrite(ibuf, sizeof(*ibuf), *isamp, p->tmp_file) != *isamp) {
    sox_fail("error writing temporary file: %s", strerror(errno));
    return SOX_EOF;
  }
  (void)obuf, *osamp = 0; /* samples not output until drain */
  return SOX_SUCCESS;
}

static int drain(sox_effect_t * effp, sox_sample_t *obuf, sox_size_t *osamp)
{
  size_t i, j;

  if (p->pos == 0) {
    fflush(p->tmp_file);
    p->pos = ftello(p->tmp_file);
    if (p->pos % sizeof(sox_sample_t) != 0) {
      sox_fail("temporary file has incorrect size");
      return SOX_EOF;
    }
    p->pos /= sizeof(sox_sample_t);
  }
  p->pos -= *osamp = min((off_t)*osamp, p->pos);
  fseeko(p->tmp_file, (off_t)(p->pos * sizeof(sox_sample_t)), SEEK_SET);
  if (fread(obuf, sizeof(sox_sample_t), *osamp, p->tmp_file) != *osamp) {
    sox_fail("error reading temporary file: %s", strerror(errno));
    return SOX_EOF;
  }
  for (i = 0, j = *osamp - 1; i < j; ++i, --j) { /* reverse the samples */
    sox_sample_t temp = obuf[i];
    obuf[i] = obuf[j];
    obuf[j] = temp;
  }
  return p->pos? SOX_SUCCESS : SOX_EOF;
}

static int stop(sox_effect_t * effp)
{
  fclose(p->tmp_file); /* auto-deleted by tmpfile */
  return SOX_SUCCESS;
}

sox_effect_handler_t const * sox_reverse_effect_fn(void)
{
  static sox_effect_handler_t handler = {
    "reverse", NULL, 0, NULL, start, flow, drain, stop, NULL, sizeof(priv_t)
  };
  return &handler;
}
