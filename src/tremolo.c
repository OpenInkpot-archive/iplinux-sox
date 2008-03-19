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

/* Effect: tremolo  (c) 2007 robs@users.sourceforge.net */

#include "sox_i.h"

static int getopts(sox_effect_t * effp, int n, char * * argv) 
{
  double speed, depth = 40;
  char dummy;     /* To check for extraneous chars. */
  char offset[100];
  char * args[] = {"sine", "fmod", 0, 0};

  if (n < 1 || n > 2 ||
      sscanf(argv[0], "%lf %c", &speed, &dummy) != 1 || speed < 0 ||
      (n > 1 && sscanf(argv[1], "%lf %c", &depth, &dummy) != 1) ||
      depth <= 0 || depth > 100)
    return lsx_usage(effp);
  args[2] = argv[0];
  sprintf(offset, "%g", 100 - depth / 2);
  args[3] = offset;
  return sox_synth_effect_fn()->getopts(effp, array_length(args), args);
}

sox_effect_handler_t const * sox_tremolo_effect_fn(void)
{
  static sox_effect_handler_t handler;
  handler = *sox_synth_effect_fn();
  handler.name = "tremolo";
  handler.usage = "speed_Hz [depth_percent]";
  handler.getopts = getopts;
  return &handler;
}
