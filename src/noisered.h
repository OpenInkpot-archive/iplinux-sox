/*
 * noiseprof.h - Headers for Noise Profiling Effect. 
 *
 * Written by Ian Turner (vectro@vectro.org)
 *
 * Copyright 1999 Ian Turner and others
 * This file is part of SoX.

 * SoX is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "sox_i.h"
#include "FFT.h"

#include <math.h>

#define WINDOWSIZE 2048
#define HALFWINDOW (WINDOWSIZE / 2)
#define FREQCOUNT  (HALFWINDOW + 1)
