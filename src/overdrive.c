/* libSoX effect: Overdrive            (c) 2008 robs@users.sourceforge.net
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "sox_i.h"

typedef struct {
  double gain, colour, last_in, last_out, b0, b1, a1;
} priv_t;

static int create(sox_effect_t * effp, int argc, char * * argv)
{
  priv_t * p = (priv_t *)effp->priv;
  p->gain = p->colour = 20;
  do {
    NUMERIC_PARAMETER(gain, 0, 100)
    NUMERIC_PARAMETER(colour, 0, 100)
  } while (0);
  p->gain = dB_to_linear(p->gain);
  p->colour /= 200;
  return argc? lsx_usage(effp) : SOX_SUCCESS;
}

static int start(sox_effect_t * effp)
{
  priv_t * p = (priv_t *)effp->priv;
  double fc = 10;
  double w0 = 2 * M_PI * fc / effp->in_signal.rate;

  if (p->gain == 1)
    return SOX_EFF_NULL;

  if (w0 > M_PI) {
    lsx_fail("frequency must be less than half the sample-rate (Nyquist rate)");
    return SOX_EOF;
  }
  p->a1 = -exp(-w0);
  p->b0 = (1 - p->a1)/2;
  p->b1 = -p->b0;
  return SOX_SUCCESS;
}

static int flow(sox_effect_t * effp, const sox_sample_t * ibuf,
    sox_sample_t * obuf, size_t * isamp, size_t * osamp)
{
  priv_t * p = (priv_t *)effp->priv;
  size_t dummy = 0, len = *isamp = *osamp = min(*isamp, *osamp);
  while (len--) {
    double d = SOX_SAMPLE_TO_FLOAT_64BIT(*ibuf++, dummy);
    d *= p->gain;
    d += p->colour;
    d = d < -1? -2./3 : d > 1? 2./3 : d - d * d * d * (1./3);
    p->last_out = p->b0 * d + p->b1 * p->last_in - p->a1 * p->last_out;
    p->last_in = d;
    *obuf++ = SOX_FLOAT_64BIT_TO_SAMPLE(p->last_out, dummy);
  }
  return SOX_SUCCESS;
}

sox_effect_handler_t const * sox_overdrive_effect_fn(void)
{
  static sox_effect_handler_t handler = {"overdrive", "[gain [colour]]",
    SOX_EFF_MCHAN, create, start, flow, NULL, NULL, NULL, sizeof(priv_t)};
  return &handler;
}
