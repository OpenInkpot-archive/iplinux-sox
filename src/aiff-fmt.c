/*
 * File format: AIFF (see aiff.c)           (c) 2007-8 SoX contributors
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
#include "aiff.h"

SOX_FORMAT_HANDLER(aiff)
{
  static char const * const names[] = {"aiff", "aif", NULL};
  static unsigned const write_encodings[] = {
    SOX_ENCODING_SIGN2, 32, 24, 16, 8, 0, 0};
  static sox_format_handler_t const sox_aiff_format = {
    "AIFF files used on Apple IIc/IIgs and SGI",
    names, SOX_FILE_LOOPS | SOX_FILE_BIG_END,
    sox_aiffstartread, sox_aiffread, sox_aiffstopread,
    sox_aiffstartwrite, sox_aiffwrite, sox_aiffstopwrite,
    sox_aiffseek, write_encodings, NULL
  };
  return &sox_aiff_format;
}
