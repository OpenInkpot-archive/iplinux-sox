/*
 * File format: cdda   (c) 2006-8 SoX contributors
 * Based on an original idea by David Elliott
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

static int start(sox_format_t * ft) 
{
  return lsx_check_read_params(ft, 2, 44100., SOX_ENCODING_SIGN2, 16, (off_t)0);
}

static int stopwrite(sox_format_t * ft) 
{
  sox_size_t const sector_num_samples = 588 * ft->signal.channels;
  sox_size_t i = ft->olength % sector_num_samples;

  if (i) while (i++ < sector_num_samples)    /* Pad with silence to multiple */
    lsx_writew(ft, 0);                       /* of 1/75th of a second. */
  return SOX_SUCCESS;
}

SOX_FORMAT_HANDLER(cdr)
{
  static char const * const names[] = {"cdda", "cdr", NULL};
  static unsigned const write_encodings[] = {SOX_ENCODING_SIGN2, 16, 0, 0};
  static sox_rate_t const write_rates[] = {44100, 0};
  static sox_format_handler_t handler = {
    SOX_LIB_VERSION_CODE,
    "Red Book Compact Disc Digital Audio",
    names, SOX_FILE_BIG_END|SOX_FILE_STEREO,
    start, lsx_rawread, NULL,
    NULL, lsx_rawwrite, stopwrite,
    lsx_rawseek, write_encodings, write_rates
  };
  return &handler;
}
