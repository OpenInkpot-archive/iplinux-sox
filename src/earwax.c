/*
 * earwax - makes listening to headphones easier
 *
 * This effect takes a stereo sound that is meant to be listened to
 * on headphones, and adds audio cues to move the soundstage from inside
 * your head (standard for headphones) to outside and in front of the
 * listener (standard for speakers). This makes the sound much easier to
 * listen to on headphones. See www.geocities.com/beinges for a full
 * explanation.
 *
 * Usage:
 *   earwax
 *
 * Note:
 *   This filter only works for 44.1 kHz stereo signals (CD format)
 *
 * November 9, 2000
 * Copyright (c) 2000 Edward Beingessner And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Edward Beingessner And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

#include "sox_i.h"
#include <string.h>

static const sox_sample_t filt[32 * 2] = {
/* 30°  330° */
    4,   -6,     /* 32 tap stereo FIR filter. */
    4,  -11,     /* One side filters as if the */
   -1,   -5,     /* signal was from 30 degrees */
    3,    3,     /* from the ear, the other as */
   -2,    5,     /* if 330 degrees. */
   -5,    0,
    9,    1,
    6,    3,     /*                         Input                         */
   -4,   -1,     /*                   Left         Right                  */
   -5,   -3,     /*                __________   __________                */
   -2,   -5,     /*               |          | |          |               */
   -7,    1,     /*           .---|  Hh,0(f) | |  Hh,0(f) |---.           */
    6,   -7,     /*          /    |__________| |__________|    \          */
   30,  -29,     /*         /                \ /                \         */
   12,   -3,     /*        /                  X                  \        */
  -11,    4,     /*       /                  / \                  \       */
   -3,    7,     /*  ____V_____   __________V   V__________   _____V____  */
  -20,   23,     /* |          | |          |   |          | |          | */
    2,    0,     /* | Hh,30(f) | | Hh,330(f)|   | Hh,330(f)| | Hh,30(f) | */
    1,   -6,     /* |__________| |__________|   |__________| |__________| */
  -14,   -5,     /*      \     ___      /           \      ___     /      */
   15,  -18,     /*       \   /   \    /    _____    \    /   \   /       */
    6,    7,     /*        `->| + |<--'    /     \    `-->| + |<-'        */
   15,  -10,     /*           \___/      _/       \_      \___/           */
  -14,   22,     /*               \     / \       / \     /               */
   -7,   -2,     /*                `--->| |       | |<---'                */
   -4,    9,     /*                     \_/       \_/                     */
    6,  -12,     /*                                                       */
    6,   -6,     /*                       Headphones                      */
    0,  -11,
    0,   -5,
    4,    0};

#define EARWAX_NUMTAPS array_length(filt)
#define taps ((sox_sample_t *)&(effp->priv)) /* FIR filter z^-1 delays */

static int start(sox_effect_t * effp)
{
  assert_static(EARWAX_NUMTAPS * sizeof(*taps) <= SOX_MAX_EFFECT_PRIVSIZE,
                /* else */ earwax_PRIVSIZE_too_big);
  if (effp->ininfo.rate != 44100 || effp->ininfo.channels != 2) {
    sox_fail("works only with stereo audio sampled at 44100Hz (i.e. CDDA)");
    return SOX_EOF;
  }
  memset(taps, 0, EARWAX_NUMTAPS * sizeof(*taps)); /* zero tap memory */
  return SOX_SUCCESS;
}

static int flow(sox_effect_t * effp, const sox_sample_t * ibuf,
                sox_sample_t * obuf, sox_size_t * isamp, sox_size_t * osamp)
{
  sox_size_t i, len = *isamp = *osamp = min(*isamp, *osamp);

  while (len--) {       /* update taps and calculate output */
    sox_sample_t output = 0;

    for (i = EARWAX_NUMTAPS - 1; i; --i) {
      taps[i] = taps[i - 1];
      output += taps[i] * filt[i];
    }
    taps[0] = *ibuf++ / 64;
    *obuf++ = output + taps[0] * filt[0];   /* store scaled output */
  }
  return SOX_SUCCESS;
}

/* No drain: preserve audio file length; it's only 32 samples anyway. */

sox_effect_handler_t const *sox_earwax_effect_fn(void)
{
  static sox_effect_handler_t handler = {
    "earwax", NULL, SOX_EFF_MCHAN, NULL, start, flow, NULL, NULL, NULL};
  return &handler;
}
