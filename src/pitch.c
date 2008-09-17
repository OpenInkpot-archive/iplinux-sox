/* libSoX effect: change the audio pitch (i.e. change pitch but not tempo)
 * Copyright (c) 2007 robs@users.sourceforge.net
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
 *
 *
 * Adjustment is given as a number of cents (100ths of a semitone) to
 * change.  Implementation comprises a tempo change (performed by tempo)
 * and a speed change performed by whichever resampling effect is in effect.
 */

#include "sox_i.h"
#include <string.h>

static int getopts(sox_effect_t * effp, int argc, char **argv)
{
  double d;
  char dummy, arg[100], **argv2 = malloc(argc * sizeof(*argv2));
  int ret, pos = (argc && !strcmp(*argv, "-q"))? 1 : 0;

  if (argc <= pos || sscanf(argv[pos], "%lf %c", &d, &dummy) != 1)
    return lsx_usage(effp);

  effp->global_info->speed *= d = pow(2., d / 1200);  /* cents --> factor */
  sprintf(arg, "%g", 1 / d);
  memcpy(argv2, argv, argc * sizeof(*argv2));
  argv2[pos] = arg;
  ret = sox_tempo_effect_fn()->getopts(effp, argc, argv2);
  free(argv2);
  return ret;
}

sox_effect_handler_t const * sox_pitch_effect_fn(void)
{
  static sox_effect_handler_t handler;
  handler = *sox_tempo_effect_fn();
  handler.name = "pitch";
  handler.usage = "[-q] shift-in-cents [segment-ms [search-ms [overlap-ms]]]",
  handler.getopts = getopts;
  handler.flags &= ~SOX_EFF_LENGTH;
  return &handler;
}

sox_effect_handler_t const * sox_key_effect_fn(void) /* old name for pitch */
{
  static sox_effect_handler_t handler;
  handler = *sox_pitch_effect_fn();
  handler.name = "key";
  handler.flags |= SOX_EFF_DEPRECATED;
  return &handler;
}
