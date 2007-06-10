/*
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

/* libSoX Effect: Adjust the audio speed (pitch and tempo together)
 *
 * (c) 2006 robs@users.sourceforge.net
 *
 * Adjustment is given as the ratio of the new speed to the old speed, or as
 * a number of cents (100ths of a semitone) to change.  Speed change is
 * actually performed by whichever resampling effect is in effect.
 */

#include "sox_i.h"
#include <math.h>
#include <string.h>

static int getopts(sox_effect_t * effp, int n, char * * argv)
{
  sox_bool is_cents = sox_false;
  double speed;

  /* Be quietly compatible with the old speed effect: */
  if (n != 0 && strcmp(*argv, "-c") == 0)
    is_cents = sox_true, ++argv, --n;

  if (n == 1) {
    char c, dummy;
    int scanned = sscanf(*argv, "%lf%c %c", &speed, &c, &dummy);
    if (scanned == 1 || (scanned == 2 && c == 'c')) {
      is_cents |= scanned == 2;
      if (is_cents || speed > 0) {
        effp->global_info->speed *= is_cents? pow(2., speed/1200) : speed;
        return SOX_SUCCESS;
      }
    }
  }
  return sox_usage(effp);
}

sox_effect_handler_t const *sox_speed_effect_fn(void)
{
  static sox_effect_handler_t handler = {
    "speed", "factor[c]", SOX_EFF_NULL|SOX_EFF_LENGTH,
    getopts, 0, 0, 0, 0, 0};
  return &handler;
}
