/*
    High-pass effect file for SoX
    Copyright (C) 1999 Jan Paul Schmidt <jps@fundament.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.
 
    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Code based on the high-pass implementation in

    Sound Processing Kit - A C++ Class Library for Audio Signal Processing
    Copyright (C) 1995-1998 Kai Lassfolk

    as described in

    Computer music: synthesis, composition, and performance
    Charles Dodge, Thomas A. Jerse
    [2nd ed.]
    Page 215

 */

#include <math.h>

#include "st_i.h"
#include "btrworth.h"

static st_effect_t st_highpass_effect;

static int st_highpass_getopts(eff_t effp, int n, char **argv) 
{
  butterworth_t butterworth = (butterworth_t)effp->priv;

  if (n != 1) {
    st_fail(st_highpass_effect.usage);
    return (ST_EOF);
  }

  st_butterworth_start (effp);

  if (!(sscanf (argv [0], "%lf", &butterworth->frequency))) {
    st_fail("highpass: illegal frequency");
    return (ST_EOF);
  }
  return (ST_SUCCESS);
}

static int st_highpass_start(eff_t effp)
{
  butterworth_t butterworth = (butterworth_t) effp->priv;
  double c;

  c = tan (M_PI * butterworth->frequency / effp->ininfo.rate);

  butterworth->a [0] = 1.0 / (1.0 + sqrt (2.0) * c + c * c);
  butterworth->a [1] = -2.0 * butterworth->a [0];
  butterworth->a [2] = butterworth->a [0];

  butterworth->b [0] = 2 * (c * c - 1.0) * butterworth->a[0];
  butterworth->b [1] = (1.0 - sqrt(2.0) * c + c * c) * butterworth->a [0];
  st_butterworth_plot(effp);
  return (ST_SUCCESS);
}

static st_effect_t st_highpass_effect = {
  "highpass",
  "Usage: highpass FREQUENCY",
  0,
  st_highpass_getopts,
  st_highpass_start,
  st_butterworth_flow,
  st_effect_nothing_drain,
  st_effect_nothing,
  st_effect_nothing
};

const st_effect_t *st_highpass_effect_fn(void)
{
    return &st_highpass_effect;
}
