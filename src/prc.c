/*
 * Psion Record format (format of sound files used for EPOC machines).
 * The file normally has no extension, so SoX uses .prc (Psion ReCord).
 * Based (heavily) on the wve.c format file. 
 * Hacked by Bert van Leeuwen (bert@e.co.za)
 * 
 * Header check improved, ADPCM encoding added, and other improvements
 * by Reuben Thomas <rrt@sc3d.org>, using file format info at
 * http://software.frodo.looijaard.name/psiconv/formats/
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
 * USA.
 *
 * Includes code for ADPCM framing based on code carrying the
 * following copyright:
 *
 *******************************************************************
 Copyright 1992 by Stichting Mathematisch Centrum, Amsterdam, The
 Netherlands.
 
                        All Rights Reserved

 Permission to use, copy, modify, and distribute this software and its
 documentation for any purpose and without fee is hereby granted,
 provided that the above copyright notice appear in all copies and that
 both that copyright notice and this permission notice appear in 
 supporting documentation, and that the names of Stichting Mathematisch
 Centrum or CWI not be used in advertising or publicity pertaining to
 distribution of the software without specific, written prior permission.

 STICHTING MATHEMATISCH CENTRUM DISCLAIMS ALL WARRANTIES WITH REGARD TO
 THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 FITNESS, IN NO EVENT SHALL STICHTING MATHEMATISCH CENTRUM BE LIABLE
 FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 ******************************************************************/

 
#include "sox_i.h"

#include "adpcms.h"

#include <assert.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

typedef struct prcpriv {
  uint32_t nsamp, nbytes;
  short padding;
  short repeats;
  sox_size_t data_start;         /* for seeking */
  struct adpcm_io adpcm;
  unsigned frame_samp;     /* samples left to read in current frame */
} *prc_t;

/* File header. The first 4 words are fixed; the rest of the header
   could theoretically be different, and this is the first place to
   check with apparently invalid files.

   N.B. All offsets are from start of file. */
static const char prc_header[]={
  /* Header section */
  '\x37','\x00','\x00','\x10', /* 0x00: File type (UID 1) */
  '\x6d','\x00','\x00','\x10', /* 0x04: File kind (UID 2) */
  '\x7e','\x00','\x00','\x10', /* 0x08: Application ID (UID 3) */
  '\xcf','\xac','\x08','\x55', /* 0x0c: Checksum of UIDs 1-3 */
  '\x14','\x00','\x00','\x00', /* 0x10: File offset of Section Table Section */
  /* Section Table Section: a BListL, i.e. a list of longs preceded by
     length byte.
     The longs are in (ID, offset) pairs, each pair identifying a
     section. */
  '\x04',                      /* 0x14: List has 4 bytes, i.e. 2 pairs */
  '\x52','\x00','\x00','\x10', /* 0x15: ID: Record Section */
  '\x34','\x00','\x00','\x00', /* 0x19: Offset to Record Section */
  '\x89','\x00','\x00','\x10', /* 0x1d: ID: Application ID Section */
  '\x25','\x00','\x00','\x00', /* 0x21: Offset to Application ID Section */
  '\x7e','\x00','\x00','\x10', /* 0x25: Application ID Section:
                                  Record.app identifier */
  /* Next comes the string, which can be either case. */
};

/* Format of the Record Section (offset 0x34):

00 L Uncompressed data length
04 ID a1 01 00 10 for ADPCM, 00 00 00 00 for A-law
08 W number of times sound will be repeated (0 = played once)
0a B Volume setting (01-05)
0b B Always 00 (?)
0c L Time between repeats in usec
10 LListB (i.e. long giving number of bytes followed by bytes) Sound Data
*/

int prc_checkheader(ft_t ft, char *head)
{
  sox_readbuf(ft, head, sizeof(prc_header));
  return memcmp(head, prc_header, sizeof(prc_header)) == 0;
}

static void prcwriteheader(ft_t ft);

static int seek(ft_t ft, sox_size_t offset)
{
  prc_t prc = (prc_t)ft->priv;
  sox_size_t new_offset, channel_block, alignment;

  new_offset = offset * ft->signal.size;
  /* Make sure request aligns to a channel block (i.e. left+right) */
  channel_block = ft->signal.channels * ft->signal.size;
  alignment = new_offset % channel_block;
  /* Most common mistake is to compute something like
   * "skip everthing up to and including this sample" so
   * advance to next sample block in this case.
   */
  if (alignment != 0)
    new_offset += (channel_block - alignment);
  new_offset += prc->data_start;

  return sox_seeki(ft, new_offset, SEEK_SET);
}

static int startread(ft_t ft)
{
  prc_t p = (prc_t)ft->priv;
  char head[sizeof(prc_header)];
  uint8_t byte;
  uint16_t reps;
  uint32_t len, listlen, encoding, repgap;
  unsigned char volume;
  char appname[0x40]; /* Maximum possible length of name */

  /* Check the header */
  if (prc_checkheader(ft, head))
    sox_debug("Found Psion Record header");
  else {
      sox_fail_errno(ft,SOX_EHDR,"Not a Psion Record file");
      return (SOX_EOF);
  }

  sox_readb(ft, &byte);
  if ((byte & 0x3) != 0x2) {
    sox_fail_errno(ft, SOX_EHDR, "Invalid length byte for application name string %d", (int)(byte));
    return SOX_EOF;
  }

  byte >>= 2;
  assert(byte < 64);
  sox_reads(ft, appname, byte);
  if (strncasecmp(appname, "record.app", byte) != 0) {
    sox_fail_errno(ft, SOX_EHDR, "Invalid application name string %.63s", appname);
    return SOX_EOF;
  }
        
  sox_readdw(ft, &len);
  p->nsamp = len;
  sox_debug("Number of samples: %d", len);

  sox_readdw(ft, &encoding);
  sox_debug("Encoding of samples: %x", encoding);
  if (encoding == 0)
    ft->signal.encoding = SOX_ENCODING_ALAW;
  else if (encoding == 0x100001a1)
    ft->signal.encoding = SOX_ENCODING_IMA_ADPCM;
  else {
    sox_fail_errno(ft, SOX_EHDR, "Unrecognised encoding");
    return SOX_EOF;
  }

  sox_readw(ft, &reps);    /* Number of repeats */
  sox_debug("Repeats: %d", reps);
        
  sox_readb(ft, &volume);
  sox_debug("Volume: %d", (unsigned)volume);
  if (volume < 1 || volume > 5)
    sox_warn("Volume %d outside range 1..5", volume);

  sox_readb(ft, &byte);   /* Unused and seems always zero */

  sox_readdw(ft, &repgap); /* Time between repeats in usec */
  sox_debug("Time between repeats (usec): %ld", repgap);

  sox_readdw(ft, &listlen); /* Length of samples list */
  sox_debug("Number of bytes in samples list: %ld", listlen);

  if (ft->signal.rate != 0 && ft->signal.rate != 8000)
    sox_report("PRC only supports 8 kHz; overriding.");
  ft->signal.rate = 8000;

  if (ft->signal.channels != 1 && ft->signal.channels != 0)
    sox_report("PRC only supports 1 channel; overriding.");
  ft->signal.channels = 1;

  p->data_start = sox_tell(ft);
  ft->length = p->nsamp / ft->signal.channels;

  if (ft->signal.encoding == SOX_ENCODING_ALAW) {
    ft->signal.size = SOX_SIZE_BYTE;
    if (sox_rawstartread(ft))
      return SOX_EOF;
  } else if (ft->signal.encoding == SOX_ENCODING_IMA_ADPCM) {
    p->frame_samp = 0;
    if (sox_adpcm_ima_start(ft, &p->adpcm))
      return SOX_EOF;
  }

  return (SOX_SUCCESS);
}

/* Read a variable-length encoded count */
/* Ignore return code of sox_readb, as it doesn't really matter if EOF
   is delayed until the caller. */
static unsigned read_cardinal(ft_t ft)
{
  unsigned a;
  uint8_t byte;

  if (sox_readb(ft, &byte) == SOX_EOF)
    return (unsigned)SOX_EOF;
  sox_debug_more("Cardinal byte 1: %x", byte);
  a = byte;
  if (!(a & 1))
    a >>= 1;
  else {
    if (sox_readb(ft, &byte) == SOX_EOF)
      return (unsigned)SOX_EOF;
    sox_debug_more("Cardinal byte 2: %x", byte);
    a |= byte << 8;
    if (!(a & 2))
      a >>= 2;
    else if (!(a & 4)) {
      if (sox_readb(ft, &byte) == SOX_EOF)
        return (unsigned)SOX_EOF;
      sox_debug_more("Cardinal byte 3: %x", byte);
      a |= byte << 16;
      if (sox_readb(ft, &byte) == SOX_EOF)
        return (unsigned)SOX_EOF;
      sox_debug_more("Cardinal byte 4: %x", byte);
      a |= byte << 24;
      a >>= 3;
    }
  }

  return a;
}

static sox_size_t read(ft_t ft, sox_ssample_t *buf, sox_size_t samp)
{
  prc_t p = (prc_t)ft->priv;

  sox_debug_more("length now = %d", p->nsamp);

  if (ft->signal.encoding == SOX_ENCODING_IMA_ADPCM) {
    sox_size_t nsamp, read;

    if (p->frame_samp == 0) {
      unsigned framelen = read_cardinal(ft);
      uint32_t trash;

      if (framelen == (unsigned)SOX_EOF)
        return 0;

      sox_debug_more("frame length %d", framelen);
      p->frame_samp = framelen;

      /* Discard length of compressed data */
      sox_debug_more("compressed length %d", read_cardinal(ft));
      /* Discard length of BListL */
      sox_readdw(ft, &trash);
      sox_debug_more("list length %d", trash);

      /* Reset CODEC for start of frame */
      sox_adpcm_reset(&p->adpcm, ft->signal.encoding);
    }
    nsamp = min(p->frame_samp, samp);
    p->nsamp += nsamp;
    read = sox_adpcm_read(ft, &p->adpcm, buf, nsamp);
    p->frame_samp -= read;
    sox_debug_more("samples left in this frame: %d", p->frame_samp);
    return read;
  } else {
    p->nsamp += samp;
    return sox_rawread(ft, buf, samp);
  }
}

static int stopread(ft_t ft)
{
  prc_t p = (prc_t)ft->priv;

  if (ft->signal.encoding == SOX_ENCODING_IMA_ADPCM)
    return sox_adpcm_stopread(ft, &p->adpcm);
  else
    return sox_rawstopread(ft);
}

/* When writing, the header is supposed to contain the number of
   data bytes written, unless it is written to a pipe.
   Since we don't know how many bytes will follow until we're done,
   we first write the header with an unspecified number of bytes,
   and at the end we rewind the file and write the header again
   with the right size.  This only works if the file is seekable;
   if it is not, the unspecified size remains in the header
   (this is illegal). */

static int startwrite(ft_t ft)
{
  prc_t p = (prc_t)ft->priv;

  if (ft->signal.encoding != SOX_ENCODING_ALAW &&
      ft->signal.encoding != SOX_ENCODING_IMA_ADPCM) {
    sox_report("PRC only supports A-law and ADPCM encoding; choosing A-law");
    ft->signal.encoding = SOX_ENCODING_ALAW;
  }
        
  if (ft->signal.encoding == SOX_ENCODING_ALAW) {
    if (sox_rawstartwrite(ft))
      return SOX_EOF;
  } else if (ft->signal.encoding == SOX_ENCODING_IMA_ADPCM) {
    if (sox_adpcm_ima_start(ft, &p->adpcm))
      return SOX_EOF;
  }
        
  p->nsamp = 0;
  p->nbytes = 0;
  if (p->repeats == 0)
    p->repeats = 1;

  if (ft->signal.rate != 0 && ft->signal.rate != 8000)
    sox_report("PRC only supports 8 kHz sample rate; overriding.");
  ft->signal.rate = 8000;

  if (ft->signal.channels != 1 && ft->signal.channels != 0)
    sox_report("PRC only supports 1 channel; overriding.");
  ft->signal.channels = 1;

  ft->signal.size = SOX_SIZE_BYTE;

  prcwriteheader(ft);

  p->data_start = sox_tell(ft);

  return SOX_SUCCESS;
}

static void write_cardinal(ft_t ft, unsigned a)
{
  uint8_t byte;

  if (a < 0x80) {
    byte = a << 1;
    sox_debug_more("Cardinal byte 1: %x", byte);
    sox_writeb(ft, byte);
  } else if (a < 0x8000) {
    byte = (a << 2) | 1;
    sox_debug_more("Cardinal byte 1: %x", byte);
    sox_writeb(ft, byte);
    byte = a >> 6;
    sox_debug_more("Cardinal byte 2: %x", byte);
    sox_writeb(ft, byte);
  } else {
    byte = (a << 3) | 3;
    sox_debug_more("Cardinal byte 1: %x", byte);
    sox_writeb(ft, byte);
    byte = a >> 5;
    sox_debug_more("Cardinal byte 2: %x", byte);
    sox_writeb(ft, byte);
    byte = a >> 13;
    sox_debug_more("Cardinal byte 3: %x", byte);
    sox_writeb(ft, byte);
    byte = a >> 21;
    sox_debug_more("Cardinal byte 4: %x", byte);
    sox_writeb(ft, byte);
  }
}

static sox_size_t write(ft_t ft, const sox_ssample_t *buf, sox_size_t samp)
{
  prc_t p = (prc_t)ft->priv;
  /* Psion Record seems not to be able to handle frames > 800 samples */
  samp = min(samp, 800);
  p->nsamp += samp;
  sox_debug_more("length now = %d", p->nsamp);
  if (ft->signal.encoding == SOX_ENCODING_IMA_ADPCM) {
    sox_size_t written;

    write_cardinal(ft, samp);
    /* Write compressed length */
    write_cardinal(ft, (samp / 2) + (samp % 2) + 4);
    /* Write length again (seems to be a BListL) */
    sox_debug_more("list length %d", samp);
    sox_writedw(ft, samp);
    sox_adpcm_reset(&p->adpcm, ft->signal.encoding);
    written = sox_adpcm_write(ft, &p->adpcm, buf, samp);
    sox_adpcm_flush(ft, &p->adpcm);
    return written;
  } else
    return sox_rawwrite(ft, buf, samp);
}

static int stopwrite(ft_t ft)
{
  prc_t p = (prc_t)ft->priv;

  /* Call before seeking to flush buffer (ADPCM has already been flushed) */
  if (ft->signal.encoding != SOX_ENCODING_IMA_ADPCM)
    sox_rawstopwrite(ft);

  p->nbytes = sox_tell(ft) - p->data_start;

  if (!ft->seekable) {
      sox_warn("Header will have invalid file length since file is not seekable");
      return SOX_SUCCESS;
  }

  if (sox_seeki(ft, 0, 0) != 0) {
      sox_fail_errno(ft,errno,"Can't rewind output file to rewrite Psion header.");
      return(SOX_EOF);
  }
  prcwriteheader(ft);
  return SOX_SUCCESS;
}

static void prcwriteheader(ft_t ft)
{
  prc_t p = (prc_t)ft->priv;

  sox_writebuf(ft, prc_header, sizeof(prc_header));
  sox_writes(ft, "\x2arecord.app");

  sox_debug("Number of samples: %d",p->nsamp);
  sox_writedw(ft, p->nsamp);

  if (ft->signal.encoding == SOX_ENCODING_ALAW)
    sox_writedw(ft, 0);
  else
    sox_writedw(ft, 0x100001a1); /* ADPCM */
  
  sox_writew(ft, 0);             /* Number of repeats */
  sox_writeb(ft, 3);             /* Volume: use default value of Record.app */
  sox_writeb(ft, 0);             /* Unused and seems always zero */
  sox_writedw(ft, 0);            /* Time between repeats in usec */

  sox_debug("Number of bytes: %d", p->nbytes);
  sox_writedw(ft, p->nbytes);    /* Number of bytes of data */
}

/* Psion .prc */
static const char *prcnames[] = {
  "prc",
  NULL
};

static sox_format_t sox_prc_format = {
  prcnames,
  SOX_FILE_SEEK | SOX_FILE_LIT_END,
  startread,
  read,
  stopread,
  startwrite,
  write,
  stopwrite,
  seek
};

const sox_format_t *sox_prc_format_fn(void)
{
  return &sox_prc_format;
}
