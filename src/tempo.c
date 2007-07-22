/*
 * Effect: change tempo (alter duration, maintain pitch) with a WSOLA method.
 * Copyright (c) 2007 robs@users.sourceforge.net
 * Based on ideas from Olli Parviainen's SoundTouch Library.
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
#include "xmalloc.h"
#include <math.h>
#include <string.h>
#include <assert.h>

/*------------------------- Addressible FIFO buffer --------------------------*/

typedef struct {
  char * data;
  size_t allocation;   /* Number of bytes allocated for data. */
  size_t item_size;    /* Size of each item in data */
  size_t begin;        /* Offset of the first byte to read. */
  size_t end;          /* 1 + Offset of the last byte byte to read. */
} fifo_t;

#define FIFO_MIN 0x4000

static void fifo_clear(fifo_t * f)
{
  f->end = f->begin = 0;
}

static void * fifo_reserve(fifo_t * f, size_t n)
{
  n *= f->item_size;

  if (f->begin == f->end)
    fifo_clear(f);

  while (1) {
    if (f->end + n <= f->allocation) {
      void *p = (char *) f->data + f->end;

      f->end += n;
      return p;
    }
    if (f->begin > FIFO_MIN) {
      memmove(f->data, f->data + f->begin, f->end - f->begin);
      f->end -= f->begin;
      f->begin = 0;
      continue;
    }
    f->allocation += n;
    f->data = xrealloc(f->data, f->allocation);
  }
}

static void * fifo_write(fifo_t * f, size_t n, void const * data)
{
  void * s = fifo_reserve(f, n);
  if (data)
    memcpy(s, data, n * f->item_size);
  return s;
}

static void fifo_trim(fifo_t * f, size_t n)
{
  n *= f->item_size;
  f->end = f->begin + n;
}

static size_t fifo_occupancy(fifo_t * f)
{
  return (f->end - f->begin) / f->item_size;
}

static void * fifo_read(fifo_t * f, size_t n, void * data)
{
  char * ret = f->data + f->begin;
  n *= f->item_size;
  if (n > f->end - f->begin)
    return NULL;
  if (data)
    memcpy(data, ret, n);
  f->begin += n;
  return ret;
}

#define fifo_read_ptr(f) fifo_read(f, 0, NULL)

static void fifo_delete(fifo_t * f)
{
  free(f->data);
}

static void fifo_create(fifo_t * f, size_t item_size)
{
  f->item_size = item_size;
  f->allocation = FIFO_MIN;
  f->data = xmalloc(f->allocation);
  fifo_clear(f);
}

/*---------------------------- WSOLA Tempo Change ----------------------------*/

typedef struct {
  /* Configuration parameters: */
  size_t channels;
  sox_bool quick_search; /* Whether to quick search or linear search */
  double factor;         /* 1 for no change, < 1 for slower, > 1 for faster. */
  size_t search;         /* Wide samples to search for best overlap position */
  size_t segment;        /* Processing segment length in wide samples */
  size_t overlap;        /* In wide samples */

  size_t process_size;   /* # input wide samples needed to process 1 segment */

  /* Buffers: */
  fifo_t input_fifo;
  float * overlap_buf;
  fifo_t output_fifo;

  /* Counters: */
  size_t samples_in;
  size_t samples_out;
  size_t segments_total;
  size_t skip_total;
} tempo_t;

/* For the Waveform Similarity part of WSOLA */
static float difference(const float * a, const float * b, size_t length)
{
  float diff = 0;
  size_t i = 0;

  #define _ diff += sqr(a[i] - b[i]), ++i; /* Loop optimisation */
  do {_ _ _ _ _ _ _ _} while (i < length); /* N.B. length ≡ 0 (mod 8) */
  #undef _
  return diff;
}

/* Find where the two segments are most alike over the overlap period. */
static size_t tempo_best_overlap_position(tempo_t * t, float const * new_win)
{
  float * f = t->overlap_buf;
  size_t j, best_pos, prev_best_pos = (t->search + 1) >> 1, step = 64;
  size_t i = best_pos = t->quick_search? prev_best_pos : 0;
  float diff, least_diff = difference(new_win + t->channels * i, f, t->channels * t->overlap);
  int k = 0;

  if (t->quick_search) do { /* hierarchical search */
    for (k = -1; k <= 1; k += 2) for (j = 1; j < 4 || step == 64; ++j) {
      i = prev_best_pos + k * j * step;
      if ((int)i < 0 || i >= t->search)
        break;
      diff = difference(new_win + t->channels * i, f, t->channels * t->overlap);
      if (diff < least_diff)
        least_diff = diff, best_pos = i;
    }
    prev_best_pos = best_pos;
  } while (step >>= 2);
  else for (i = 1; i < t->search; i++) { /* linear search */
    diff = difference(new_win + t->channels * i, f, t->channels * t->overlap);
    if (diff < least_diff)
      least_diff = diff, best_pos = i;
  }
  return best_pos;
}

/* For the Over-Lap part of WSOLA */
static void tempo_overlap(
    tempo_t * t, const float * in1, const float * in2, float * output)
{
  size_t i, j, k = 0;
  float fade_step = 1.0f / (float) t->overlap;

  for (i = 0; i < t->overlap; ++i) {
    float fade_in  = fade_step * (float) i;
    float fade_out = 1.0f - fade_in;
    for (j = 0; j < t->channels; ++j, ++k)
      output[k] = in1[k] * fade_out + in2[k] * fade_in;
  }
}

static void tempo_process(tempo_t * t)
{
  while (fifo_occupancy(&t->input_fifo) >= t->process_size) {
    size_t skip, offset = 0;

    /* Copy or overlap the first bit to the output */
    if (!t->segments_total)
      fifo_write(&t->output_fifo, t->overlap, fifo_read_ptr(&t->input_fifo));
    else {
      offset = tempo_best_overlap_position(t, fifo_read_ptr(&t->input_fifo));
      tempo_overlap(t, t->overlap_buf,
          (float *) fifo_read_ptr(&t->input_fifo) + t->channels * offset,
          fifo_write(&t->output_fifo, t->overlap, NULL));
    }
    /* Copy the middle bit to the output */
    if (t->segment > 2 * t->overlap)
      fifo_write(&t->output_fifo, t->segment - 2 * t->overlap,
                 (float *) fifo_read_ptr(&t->input_fifo) +
                 t->channels * (offset + t->overlap));

    /* Copy the end bit to overlap_buf ready to be mixed with
     * the beginning of the next segment. */
    memcpy(t->overlap_buf,
           (float *) fifo_read_ptr(&t->input_fifo) +
           t->channels * (offset + t->segment - t->overlap),
           t->channels * t->overlap * sizeof(*(t->overlap_buf)));

    /* The Advance part of WSOLA */
    skip = t->factor * (++t->segments_total * (t->segment - t->overlap)) + 0.5;
    skip -= (t->search + 1) >> 1; /* So search straddles nominal skip point. */
    t->skip_total += skip -= t->skip_total;
    fifo_read(&t->input_fifo, skip, NULL);
  }
}

static float * tempo_input(tempo_t * t, float const * samples, size_t n)
{
  t->samples_in += n;
  return fifo_write(&t->input_fifo, n, samples);
}

static float const * tempo_output(tempo_t * t, float * samples, size_t * n)
{
  t->samples_out += *n = min(*n, fifo_occupancy(&t->output_fifo));
  return fifo_read(&t->output_fifo, *n, samples);
}

/* Flush samples remaining in overlap_buf & input_fifo to the output. */
static void tempo_flush(tempo_t * t)
{
  size_t samples_out = t->samples_in / t->factor + .5;
  size_t remaining = samples_out - t->samples_out;
  float * buff = xcalloc(128 * t->channels, sizeof(*buff));

  if ((int)remaining > 0) {
    while (fifo_occupancy(&t->output_fifo) < remaining) {
      tempo_input(t, buff, 128);
      tempo_process(t);
    }
    fifo_trim(&t->output_fifo, remaining);
    t->samples_in = 0;
  }
  free(buff);
}

static void tempo_setup(tempo_t * t,
  double sample_rate, sox_bool quick_search, double factor,
  double segment_ms, double search_ms, double overlap_ms)
{
  size_t max_skip;
  t->quick_search = quick_search;
  t->factor = factor;
  t->segment = sample_rate * segment_ms / 1000 + .5;
  t->search  = sample_rate * search_ms / 1000 + .5;
  t->overlap = max(sample_rate * overlap_ms / 1000 + 4.5, 16);
  t->overlap &= ~7; /* Make divisible by 8 for loop optimisation */
  t->overlap_buf = xmalloc(t->overlap * t->channels * sizeof(*t->overlap_buf));
  max_skip = ceil(factor * (t->segment - t->overlap));
  t->process_size = max(max_skip + t->overlap, t->segment) + t->search;
}

static void tempo_delete(tempo_t * t)
{
  free(t->overlap_buf);
  fifo_delete(&t->output_fifo);
  fifo_delete(&t->input_fifo);
  free(t);
}

static tempo_t * tempo_create(size_t channels)
{
  tempo_t * t = xcalloc(1, sizeof(*t));
  t->channels = channels;
  fifo_create(&t->input_fifo, t->channels * sizeof(float));
  fifo_create(&t->output_fifo, t->channels * sizeof(float));
  return t;
}

/*------------------------------- SoX Wrapper --------------------------------*/

typedef struct tempo {
  tempo_t     * tempo;
  sox_bool    quick_search;
  double      factor, segment_ms, search_ms, overlap_ms;
} priv_t;

assert_static(sizeof(struct tempo) <= SOX_MAX_EFFECT_PRIVSIZE,
              /* else */ tempo_PRIVSIZE_too_big);

static int getopts(sox_effect_t * effp, int argc, char **argv)
{
  priv_t * p = (priv_t *) effp->priv;

  p->segment_ms = 82; /* Set non-zero defaults: */
  p->search_ms  = 14;
  p->overlap_ms = 12;

  p->quick_search = argc && !strcmp(*argv, "-q") && (--argc, ++argv, sox_true);
  do {                    /* break-able block */
    NUMERIC_PARAMETER(factor      ,0.25, 4  )
    NUMERIC_PARAMETER(segment_ms  , 10 , 120)
    NUMERIC_PARAMETER(search_ms   , 0  , 30 )
    NUMERIC_PARAMETER(overlap_ms  , 0  , 30 )
  } while (0);

  return argc || !p->factor || p->overlap_ms + p->search_ms >= p->segment_ms ?
    sox_usage(effp) : SOX_SUCCESS;
}

static int start(sox_effect_t * effp)
{
  priv_t * p = (priv_t *) effp->priv;

  if (p->factor == 1)
    return SOX_EFF_NULL;

  p->tempo = tempo_create(effp->ininfo.channels);
  tempo_setup(p->tempo, effp->ininfo.rate, p->quick_search, p->factor,
      p->segment_ms, p->search_ms, p->overlap_ms);
  return SOX_SUCCESS;
}

static int flow(sox_effect_t * effp, const sox_ssample_t * ibuf,
                sox_ssample_t * obuf, sox_size_t * isamp, sox_size_t * osamp)
{
  priv_t * p = (priv_t *) effp->priv;
  sox_size_t i, odone = *osamp /= effp->ininfo.channels;
  float const * s = tempo_output(p->tempo, NULL, &odone);

  for (i = 0; i < odone * effp->ininfo.channels; ++i)
    *obuf++ = SOX_FLOAT_32BIT_TO_SAMPLE(*s++, effp->clips);

  if (*isamp && odone < *osamp) {
    float * t = tempo_input(p->tempo, NULL, *isamp / effp->ininfo.channels);
    for (i = *isamp; i; --i)
      *t++ = SOX_SAMPLE_TO_FLOAT_32BIT(*ibuf++, effp->clips);
    tempo_process(p->tempo);
  }
  else *isamp = 0;

  *osamp = odone * effp->ininfo.channels;
  return SOX_SUCCESS;
}

static int drain(sox_effect_t * effp, sox_ssample_t * obuf, sox_size_t * osamp)
{
  static sox_size_t isamp = 0;
  tempo_flush(((priv_t *)effp->priv)->tempo);
  return flow(effp, 0, obuf, &isamp, osamp);
}

static int stop(sox_effect_t * effp)
{
  tempo_delete(((priv_t *)effp->priv)->tempo);
  return SOX_SUCCESS;
}

sox_effect_handler_t const * sox_tempo_effect_fn(void)
{
  static sox_effect_handler_t handler = {
    "tempo", "[-q] factor [segment-ms [search-ms [overlap-ms]]]",
    SOX_EFF_MCHAN | SOX_EFF_LENGTH, getopts, start, flow, drain, stop, NULL
  };
  return &handler;
}
