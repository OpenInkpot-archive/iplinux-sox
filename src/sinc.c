/* Effect: sinc filters     Copyright (c) 2008-9 robs@users.sourceforge.net
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
#include "dft_filter.h"
#include "getopt.h"
#include <string.h>

typedef struct {
  dft_filter_priv_t  base;
  double             att, beta, phase, Fc0, Fc1, tbw0, tbw1;
  int                num_taps[2];
  sox_bool           round;
} priv_t;

static int create(sox_effect_t * effp, int argc, char * * argv)
{
  priv_t * p = (priv_t *)effp->priv;
  dft_filter_priv_t * b = &p->base;
  char * parse_ptr = argv[0];
  int i = 0;

  b->filter_ptr = &b->filter;
  p->phase = 50;
  p->beta = -1;
  while (i < 2) {
    int c = 1;
    while (c && (c = getopt(argc, argv, "+ra:b:p:MILt:n:")) != -1) switch (c) {
      char * parse_ptr;
      case 'r': p->round = sox_true; break;
      GETOPT_NUMERIC('a', att,  40 , 170)
      GETOPT_NUMERIC('b', beta,  0 , 18)
      GETOPT_NUMERIC('p', phase, 0, 100)
      case 'M': p->phase =  0; break;
      case 'I': p->phase = 25; break;
      case 'L': p->phase = 50; break;
      GETOPT_NUMERIC('n', num_taps[1], 11, 32767)
      case 't': p->tbw1 = lsx_parse_frequency(optarg, &parse_ptr);
        if (p->tbw1 < 1 || *parse_ptr) return lsx_usage(effp);
        break;
      default: c = 0;
    }
    if ((p->att && p->beta >= 0) || (p->tbw1 && p->num_taps[1]))
      return lsx_usage(effp);
    if (!i || !p->Fc1)
      p->tbw0 = p->tbw1, p->num_taps[0] = p->num_taps[1];
    if (!i++ && optind < argc) {
      if (*(parse_ptr = argv[optind++]) != '-')
        p->Fc0 = lsx_parse_frequency(parse_ptr, &parse_ptr);
      if (*parse_ptr == '-')
        p->Fc1 = lsx_parse_frequency(parse_ptr + 1, &parse_ptr);
    }
  }
  return optind != argc || p->Fc0 < 0 || p->Fc1 < 0 || *parse_ptr ?
      lsx_usage(effp) : SOX_SUCCESS;
}

static void invert(double * h, int n)
{
  int i;
  for (i = 0; i < n; ++i)
    h[i] = -h[i];
  h[(n - 1) / 2] += 1;
}

static double * lpf(double Fn, double Fc, double tbw, int * num_taps, double att, double * beta, double phase, sox_bool round)
{
  if ((Fc /= Fn) <= 0 || Fc >= 1) {
    *num_taps = 0;
    return NULL;
  }
  att = att? att : 120;
  att = phase && phase != 50 && phase != 100? 34./33 * att : att;
  *beta = *beta < 0? lsx_kaiser_beta(att) : *beta;
  if (!*num_taps) {
    int n = lsx_lpf_num_taps(att, (tbw? tbw / Fn : .05) * .5, 0);
    *num_taps = range_limit(n, 11, 32767);
    if (round)
      *num_taps = 1 + 2 * (int)((int)((*num_taps / 2) * Fc + .5) / Fc + .5);
    lsx_report("num taps = %i (from %i)", *num_taps, n);
  }
  return lsx_make_lpf(*num_taps | 1, Fc, *beta, 1., sox_false);
}

static int start(sox_effect_t * effp)
{
  priv_t * p = (priv_t *)effp->priv;
  dft_filter_filter_t * f = p->base.filter_ptr;

  if (!f->num_taps) {
    double Fn = effp->in_signal.rate * .5;
    double * h[2];
    int i, longer;

    if (p->Fc0 >= Fn || p->Fc1 >= Fn) {
      lsx_fail("filter frequency must be less than sample-rate / 2");
      return SOX_EOF;
    }
    h[0] = lpf(Fn, p->Fc0, p->tbw0, &p->num_taps[0], p->att, &p->beta, p->phase, p->round);
    if (h[0]) invert(h[0], p->num_taps[0]);
    h[1] = lpf(Fn, p->Fc1, p->tbw1, &p->num_taps[1], p->att, &p->beta, p->phase, p->round);
    longer = p->num_taps[1] > p->num_taps[0];
    f->num_taps = p->num_taps[longer];
    if (h[0] && h[1]) {
      for (i = 0; i < p->num_taps[!longer]; ++i)
        h[longer][i + (f->num_taps - p->num_taps[!longer])/2] += h[!longer][i];
      free(h[!longer]);
      if (p->Fc0 < p->Fc1)
        invert(h[longer], f->num_taps);
    }
    if (p->phase != 50)
      lsx_fir_to_phase(&h[longer], &f->num_taps, &f->post_peak, p->phase);
    else f->post_peak = f->num_taps / 2;
    lsx_debug("%i %i %g%%", f->num_taps, f->post_peak,
        100 - 100. * f->post_peak / (f->num_taps - 1));
    if (effp->global_info->plot != sox_plot_off) {
      char title[100];
      sprintf(title, "SoX effect: sinc filter freq=%g-%g", p->Fc0, p->Fc1? p->Fc1 : Fn);
      lsx_plot_fir(h[longer], f->num_taps, effp->in_signal.rate,
          effp->global_info->plot, title, -p->beta * 10 - 25, 5.);
      return SOX_EOF;
    }
    f->dft_length = lsx_set_dft_length(f->num_taps);
    f->coefs = lsx_calloc(f->dft_length, sizeof(*f->coefs));
    for (i = 0; i < f->num_taps; ++i)
      f->coefs[(i + f->dft_length - f->num_taps + 1) & (f->dft_length - 1)] = h[longer][i] / f->dft_length * 2;
    free(h[longer]);
    lsx_safe_rdft(f->dft_length, 1, f->coefs);
  }
  return sox_dft_filter_effect_fn()->start(effp);
}

sox_effect_handler_t const * sox_sinc_effect_fn(void)
{
  static sox_effect_handler_t handler;
  handler = *sox_dft_filter_effect_fn();
  handler.name = "sinc";
  handler.usage = "[-a att] [-p phase|-M|-I|-L] [-t tbw|-n taps] [freqHP][-freqLP [-t ...|-n ...]]";
  handler.getopts = create;
  handler.start = start;
  handler.priv_size = sizeof(priv_t);
  return &handler;
}
