/*
 * libSoX lpc-10 format.
 *
 * Copyright 2007 Reuben Thomas <rrt@sc3d.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, write to the Free Software
 * Foundation, Fifth Floor, 51 Franklin Street, Boston, MA 02111-1301,
 * USA.  */

#include "sox_i.h"
#include "../lpc10/lpc10.h"

/* Private data */
typedef struct lpcpriv {
  struct lpc10_encoder_state *encst;
  float speech[LPC10_SAMPLES_PER_FRAME];
  unsigned samples;
  struct lpc10_decoder_state *decst;
} *lpcpriv_t;

/*
  Write the bits in bits[0] through bits[len-1] to file f, in "packed"
  format.

  bits is expected to be an array of len integer values, where each
  integer is 0 to represent a 0 bit, and any other value represents a
  1 bit. This bit string is written to the file f in the form of
  several 8 bit characters. If len is not a multiple of 8, then the
  last character is padded with 0 bits -- the padding is in the least
  significant bits of the last byte. The 8 bit characters are "filled"
  in order from most significant bit to least significant.
*/
static void write_bits(ft_t ft, INT32 *bits, int len)
{
  int i;
  uint8_t mask;	/* The next bit position within the variable "data" to
                   place the next bit. */
  uint8_t data;	/* The contents of the next byte to place in the
                   output. */

  /* Fill in the array bits.
   * The first compressed output bit will be the most significant
   * bit of the byte, so initialize mask to 0x80.  The next byte of
   * compressed data is initially 0, and the desired bits will be
   * turned on below.
   */
  mask = 0x80;
  data = 0;

  for (i = 0; i < len; i++) {
    /* Turn on the next bit of output data, if necessary. */
    if (bits[i]) {
      data |= mask;
    }
    /*
     * If the byte data is full, determined by mask becoming 0,
     * then write the byte to the output file, and reinitialize
     * data and mask for the next output byte.  Also add the byte
     * if (i == len-1), because if len is not a multiple of 8,
     * then mask won't yet be 0.  */
    mask >>= 1;
    if ((mask == 0) || (i == len-1)) {
      sox_writeb(ft, data);
      data = 0;
      mask = 0x80;
    }
  }
}

/*
  Read bits from file f into bits[0] through bits[len-1], in "packed"
  format.

  Read ceiling(len/8) characters from file f, if that many are
  available to read, otherwise read to the end of the file. The first
  character's 8 bits, in order from MSB to LSB, are used to fill
  bits[0] through bits[7]. The second character's bits are used to
  fill bits[8] through bits[15], and so on. If ceiling(len/8)
  characters are available to read, and len is not a multiple of 8,
  then some of the least significant bits of the last character read
  are completely ignored. Every entry of bits[] that is modified is
  changed to either a 0 or a 1.

  The number of bits successfully read is returned, and is always in
  the range 0 to len, inclusive. If it is less than len, it will
  always be a multiple of 8.
*/
static int read_bits(ft_t ft, INT32 *bits, int len)
{
  int i;
  uint8_t c;

  /* Unpack the array bits into coded_frame. */
  for (i = 0; i < len; i++) {
    if (i % 8 == 0) {
      sox_readb(ft, &c);
      if (sox_eof(ft)) {
        return (i);
      }
    }
    if (c & (0x80 >> (i & 7))) {
      bits[i] = 1;
    } else {
      bits[i] = 0;
    }
  }
  return (len);
}

static int startread(ft_t ft)
{
  lpcpriv_t lpc = (lpcpriv_t)ft->priv;

  if ((lpc->decst = create_lpc10_decoder_state()) == NULL) {
    fprintf(stderr, "lpc10 could not allocate decoder state");
    return SOX_EOF;
  }
  lpc->samples = LPC10_SAMPLES_PER_FRAME;

  return SOX_SUCCESS;
}

static int startwrite(ft_t ft) 
{
  lpcpriv_t lpc = (lpcpriv_t)ft->priv;

  if ((lpc->encst = create_lpc10_encoder_state()) == NULL) {
    fprintf(stderr, "lpc10 could not allocate encoder state");
    return SOX_EOF;
  }
  lpc->samples = 0;

  return SOX_SUCCESS;
}

static sox_size_t read(ft_t ft, sox_ssample_t *buf, sox_size_t len)
{
  lpcpriv_t lpc = (lpcpriv_t)ft->priv;
  sox_size_t nread = 0;

  while (nread < len) {
    /* Read more data if buffer is empty */
    if (lpc->samples == LPC10_SAMPLES_PER_FRAME) {
      INT32 bits[LPC10_BITS_IN_COMPRESSED_FRAME];

      if (read_bits(ft, bits, LPC10_BITS_IN_COMPRESSED_FRAME) !=
          LPC10_BITS_IN_COMPRESSED_FRAME)
        break;
      lpc10_decode(bits, lpc->speech, lpc->decst);
      lpc->samples = 0;
    }

    while (lpc->samples < LPC10_BITS_IN_COMPRESSED_FRAME)
      buf[nread++] = SOX_FLOAT_32BIT_TO_SAMPLE(lpc->speech[lpc->samples++], ft->clips);
  }

  return nread;
}

static sox_size_t write(ft_t ft, const sox_ssample_t *buf, sox_size_t len)
{
  lpcpriv_t lpc = (lpcpriv_t)ft->priv;
  sox_size_t nwritten = 0;

  while (len + lpc->samples >= LPC10_SAMPLES_PER_FRAME) {
    INT32 bits[LPC10_BITS_IN_COMPRESSED_FRAME];

    while (lpc->samples < LPC10_SAMPLES_PER_FRAME) {
      lpc->speech[lpc->samples++] = SOX_SAMPLE_TO_FLOAT_32BIT(buf[nwritten++], ft->clips);
      len--;
    }
    
    lpc10_encode(lpc->speech, bits, lpc->encst);
    write_bits(ft, bits, LPC10_BITS_IN_COMPRESSED_FRAME);
    lpc->samples = 0;
  }

  return nwritten;
}

static int stopread(ft_t ft)
{
  lpcpriv_t lpc = (lpcpriv_t)ft->priv;

  free(lpc->decst);

  return SOX_SUCCESS;
}

static int stopwrite(ft_t ft)
{
  lpcpriv_t lpc = (lpcpriv_t)ft->priv;

  free(lpc->encst);

  return SOX_SUCCESS;
}

/* LPC-10 */
static const char *lpc10names[] = {
  "lpc",
  "lpc10",
  NULL
};

static sox_format_t sox_lpc10_format = {
  lpc10names,
  0,
  startread,
  read,
  stopread,
  startwrite,
  write,
  stopwrite,
  sox_format_nothing_seek
};

const sox_format_t *sox_lpc10_format_fn(void);

const sox_format_t *sox_lpc10_format_fn(void)
{
  return &sox_lpc10_format;
}
