/*
 * Effect: remix
 * Copyright (c) 2008 robs@users.sourceforge.net
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, write to the Free Software Foundation,
 * Fifth Floor, 51 Franklin Street, Boston, MA 02111-1301, USA.
 */

#include "sox_i.h"
#include <math.h>
#include <string.h>

typedef struct remix
{
  enum {semi, automatic, manual} mode;
  unsigned num_out_channels, min_in_channels;
  struct {
    char * str;          /* Command-line argument to parse for this out_spec */
    unsigned num_in_channels;
    struct in_spec {
      unsigned channel_num;
      double   multiplier;
    } * in_specs;
  } * out_specs;
} * remix_t;

assert_static(sizeof(struct remix) <= SOX_MAX_EFFECT_PRIVSIZE,
              /* else */ remix_PRIVSIZE_too_big);

#define PARSE(SEP, SCAN, VAR, MIN, SEPARATORS) do {\
  end = strpbrk(text, SEPARATORS); \
  if (end == text) \
    SEP = *text++; \
  else { \
    SEP = (SEPARATORS)[strlen(SEPARATORS) - 1]; \
    n = sscanf(text, SCAN"%c", &VAR, &SEP); \
    if (VAR < MIN || (n == 2 && !strchr(SEPARATORS, SEP))) \
      return sox_usage(effp); \
    text = end? end + 1 : text + strlen(text); \
  } \
} while (0)

static int parse(sox_effect_t * effp, char * * argv, unsigned channels)
{
  remix_t p = (remix_t) effp->priv;
  unsigned i, j;

  p->min_in_channels = 0;
  for (i = 0; i < p->num_out_channels; ++i) {
    sox_bool mul_spec = sox_false;
    char * text, * end;
    if (argv) /* 1st parse only */
      p->out_specs[i].str = xstrdup(argv[i]);
    for (j = 0, text = p->out_specs[i].str; *text;) {
      static char const separators[] = "-vpi,";
      char sep1, sep2;
      int chan1 = 1, chan2 = channels, n;
      double multiplier = HUGE_VAL;

      PARSE(sep1, "%i", chan1, 0, separators);
      if (!chan1) {
       if (j || *text)
         return sox_usage(effp);
       continue;
      }
      if (sep1 == '-')
        PARSE(sep1, "%i", chan2, 0, separators + 1);
      else chan2 = chan1;
      if (sep1 != ',') {
        multiplier = sep1 == 'v' ? 1 : 0;
        PARSE(sep2, "%lf", multiplier, -HUGE_VAL, separators + 4);
        if (sep1 != 'v')
          multiplier = (sep1 == 'p'? 1 : -1) * exp(multiplier / 40 * log(10.));
        mul_spec = sox_true;
      }
      if (chan2 < chan1) {int t = chan1; chan1 = chan2; chan2 = t;}
      p->out_specs[i].in_specs = xrealloc(p->out_specs[i].in_specs,
          (j + chan2 - chan1 + 1) * sizeof(*p->out_specs[i].in_specs));
      while (chan1 <= chan2) {
        p->out_specs[i].in_specs[j].channel_num = chan1++ - 1;
        p->out_specs[i].in_specs[j++].multiplier = multiplier;
      }
      p->min_in_channels = max(p->min_in_channels, (unsigned)chan2);
    }
    p->out_specs[i].num_in_channels = j;
    for (j = 0; j < p->out_specs[i].num_in_channels; ++j)
      if (p->out_specs[i].in_specs[j].multiplier == HUGE_VAL)
        p->out_specs[i].in_specs[j].multiplier = (p->mode == automatic || (p->mode == semi && !mul_spec)) ?  1. / p->out_specs[i].num_in_channels : 1;
  }
  effp->outinfo.channels = p->num_out_channels;
  return SOX_SUCCESS;
}

static int create(sox_effect_t * effp, int argc, char * * argv)
{
  remix_t p = (remix_t) effp->priv;
  if (argc && !strcmp(*argv, "-m")) p->mode = manual   , ++argv, --argc;
  if (argc && !strcmp(*argv, "-a")) p->mode = automatic, ++argv, --argc;
  p->out_specs = xcalloc(p->num_out_channels = argc, sizeof(*p->out_specs));
  return parse(effp, argv, 1); /* No channels yet; parse with dummy */
}

static int start(sox_effect_t * effp)
{
  remix_t p = (remix_t) effp->priv;
  parse(effp, NULL, effp->ininfo.channels);
  if (effp->ininfo.channels < p->min_in_channels) {
    sox_fail("too few input channels");
    return SOX_EOF;
  }
  return SOX_SUCCESS;
}

static int flow(sox_effect_t * effp, const sox_sample_t * ibuf,
    sox_sample_t * obuf, sox_size_t * isamp, sox_size_t * osamp)
{
  remix_t p = (remix_t) effp->priv;
  unsigned i, j, len;
  len =  min(*isamp / effp->ininfo.channels, *osamp / effp->outinfo.channels);
  *isamp = len * effp->ininfo.channels;
  *osamp = len * effp->outinfo.channels;

  for (; len--; ibuf += effp->ininfo.channels) for (j = 0; j < effp->outinfo.channels; j++) {
    double out = 0;
    for (i = 0; i < p->out_specs[j].num_in_channels; i++)
      out += ibuf[p->out_specs[j].in_specs[i].channel_num] * p->out_specs[j].in_specs[i].multiplier;
    *obuf++ = SOX_ROUND_CLIP_COUNT(out, effp->clips);
  }
  return SOX_SUCCESS;
}

static int kill(sox_effect_t * effp)
{
  remix_t p = (remix_t) effp->priv;
  unsigned i;
  for (i = 0; i < p->num_out_channels; ++i) {
    free(p->out_specs[i].str);
    free(p->out_specs[i].in_specs);
  }
  free(p->out_specs);
  return SOX_SUCCESS;
}

sox_effect_handler_t const * sox_remix_effect_fn(void)
{
  static sox_effect_handler_t handler = {
    "remix", "<0|in-chan[v|d|i volume]{,in-chan[v|d|i volume]}>",
    SOX_EFF_MCHAN | SOX_EFF_CHAN,
    create, start, flow, NULL, NULL, kill
  };
  return &handler;
}
