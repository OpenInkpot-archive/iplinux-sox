/*
 * File format: DVMS (see cvsd.c)        (c) 2007-8 SoX contributors
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

#include "cvsd.h"

SOX_FORMAT_HANDLER(dvms)
{
  static char const * const names[] = {"dvms", "vms", NULL};
  static unsigned const write_encodings[] = {SOX_ENCODING_CVSD, 1, 0, 0};
  static sox_format_handler_t const handler = {
    names, SOX_FILE_MONO,
    sox_dvmsstartread, sox_cvsdread, sox_cvsdstopread,
    sox_dvmsstartwrite, sox_cvsdwrite, sox_dvmsstopwrite,
    NULL, write_encodings, NULL
  };
  return &handler;
}
