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

/* ADPCM CODECs: IMA, OKI.   (c) 2007 robs@users.sourceforge.net */

typedef struct {
  int max_step_index;
  int sign;
  int shift;
  int const * steps;
  int const * changes;
  int mask;
} adpcm_setup_t;

typedef struct {
  adpcm_setup_t setup;
  int last_output;
  int step_index;
  int errors;
} adpcm_t;

void adpcm_init(adpcm_t * p, int type, int first_sample);
int adpcm_decode(int code, adpcm_t * p);
int adpcm_encode(int sample, adpcm_t * p);

typedef struct adpcm_io {
  adpcm_t encoder;
  struct {
    uint8_t byte;               /* write store */
    uint8_t flag;
  } store;
  sox_fileinfo_t file;
} *adpcm_io_t;

/* Format methods */
void sox_adpcm_reset(adpcm_io_t state, sox_encoding_t type);
int sox_adpcm_oki_start(sox_format_t * ft, adpcm_io_t state);
int sox_adpcm_ima_start(sox_format_t * ft, adpcm_io_t state);
sox_size_t sox_adpcm_read(sox_format_t * ft, adpcm_io_t state, sox_sample_t *buffer, sox_size_t len);
int sox_adpcm_stopread(sox_format_t * ft, adpcm_io_t state);
sox_size_t sox_adpcm_write(sox_format_t * ft, adpcm_io_t state, const sox_sample_t *buffer, sox_size_t length);
void sox_adpcm_flush(sox_format_t * ft, adpcm_io_t state);
int sox_adpcm_stopwrite(sox_format_t * ft, adpcm_io_t state);
