/*
 * libSoX Macintosh HCOM format.
 * These are really FSSD type files with Huffman compression,
 * in MacBinary format.
 * To do: make the MacBinary format optional (so that .data files
 * are also acceptable).  (How to do this on output?)
 *
 * September 25, 1991
 * Copyright 1991 Guido van Rossum And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Guido van Rossum And Sundry Contributors are not responsible for
 * the consequences of using this software.
 *
 * April 28, 1998 - Chris Bagwell (cbagwell@sprynet.com)
 *
 *  Rearranged some functions so that they are declared before they are
 *  used, clearing up some compiler warnings.  Because these functions
 *  passed floats, it helped some dumb compilers pass stuff on the
 *  stack correctly.
 *
 */

#include "sox_i.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

/* Dictionary entry for Huffman (de)compression */
typedef struct {
        long frequ;
        short dict_leftson;
        short dict_rightson;
} dictent;

/* Private data used by reader */
struct readpriv {
        /* Static data from the header */
        dictent *dictionary;
        int32_t checksum;
        int deltacompression;
        /* Engine state */
        long huffcount;
        long cksum;
        int dictentry;
        int nrbits;
        uint32_t current;
        short sample;
        /* Dictionary */
        dictent *de;
        int32_t new_checksum;
        int nbits;
        int32_t curword;
};

static int sox_hcomstartread(ft_t ft)
{
        struct readpriv *p = (struct readpriv *) ft->priv;
        int i;
        char buf[5];
        uint32_t datasize, rsrcsize;
        uint32_t huffcount, checksum, compresstype, divisor;
        unsigned short dictsize;
        int rc;


        /* Skip first 65 bytes of header */
        rc = sox_skipbytes(ft, 65);
        if (rc)
            return rc;

        /* Check the file type (bytes 65-68) */
        if (sox_reads(ft, buf, 4) == SOX_EOF || strncmp(buf, "FSSD", 4) != 0)
        {
                sox_fail_errno(ft,SOX_EHDR,"Mac header type is not FSSD");
                return (SOX_EOF);
        }

        /* Skip to byte 83 */
        rc = sox_skipbytes(ft, 83-69);
        if (rc)
            return rc;

        /* Get essential numbers from the header */
        sox_readdw(ft, &datasize); /* bytes 83-86 */
        sox_readdw(ft, &rsrcsize); /* bytes 87-90 */

        /* Skip the rest of the header (total 128 bytes) */
        rc = sox_skipbytes(ft, 128-91);
        if (rc != 0)
            return rc;

        /* The data fork must contain a "HCOM" header */
        if (sox_reads(ft, buf, 4) == SOX_EOF || strncmp(buf, "HCOM", 4) != 0)
        {
                sox_fail_errno(ft,SOX_EHDR,"Mac data fork is not HCOM");
                return (SOX_EOF);
        }

        /* Then follow various parameters */
        sox_readdw(ft, &huffcount);
        sox_readdw(ft, &checksum);
        sox_readdw(ft, &compresstype);
        if (compresstype > 1)
        {
                sox_fail_errno(ft,SOX_EHDR,"Bad compression type in HCOM header");
                return (SOX_EOF);
        }
        sox_readdw(ft, &divisor);
        if (divisor == 0 || divisor > 4)
        {
                sox_fail_errno(ft,SOX_EHDR,"Bad sampling rate divisor in HCOM header");
                return (SOX_EOF);
        }
        sox_readw(ft, &dictsize);

        /* Translate to sox parameters */
        ft->signal.encoding = SOX_ENCODING_UNSIGNED;
        ft->signal.size = SOX_SIZE_BYTE;
        ft->signal.rate = 22050 / divisor;
        ft->signal.channels = 1;

        /* Allocate memory for the dictionary */
        p->dictionary = (dictent *)xmalloc(511 * sizeof(dictent));

        /* Read dictionary */
        for(i = 0; i < dictsize; i++) {
                sox_readw(ft, (unsigned short *)&(p->dictionary[i].dict_leftson));
                sox_readw(ft, (unsigned short *)&(p->dictionary[i].dict_rightson));
                sox_debug("%d %d",
                       p->dictionary[i].dict_leftson,
                       p->dictionary[i].dict_rightson);
        }
        rc = sox_skipbytes(ft, 1); /* skip pad byte */
        if (rc)
            return rc;

        /* Initialized the decompression engine */
        p->checksum = checksum;
        p->deltacompression = compresstype;
        if (!p->deltacompression)
                sox_debug("HCOM data using value compression");
        p->huffcount = huffcount;
        p->cksum = 0;
        p->dictentry = 0;
        p->nrbits = -1; /* Special case to get first byte */

        return (SOX_SUCCESS);
}

static sox_size_t sox_hcomread(ft_t ft, sox_ssample_t *buf, sox_size_t len)
{
        register struct readpriv *p = (struct readpriv *) ft->priv;
        int done = 0;
        unsigned char sample_rate;

        if (p->nrbits < 0) {
                /* The first byte is special */
                if (p->huffcount == 0)
                        return 0; /* Don't know if this can happen... */
                if (sox_readb(ft, &sample_rate) == SOX_EOF)
                {
                        sox_fail_errno(ft,SOX_EOF,"unexpected EOF at start of HCOM data");
                        return (0);
                }
                p->sample = sample_rate;
                *buf++ = SOX_UNSIGNED_BYTE_TO_SAMPLE(p->sample,);
                p->huffcount--;
                p->nrbits = 0;
                done++;
                len--;
                if (len == 0)
                        return done;
        }

        while (p->huffcount > 0) {
                if(p->nrbits == 0) {
                        sox_readdw(ft, &(p->current));
                        if (sox_eof(ft))
                        {
                                sox_fail_errno(ft,SOX_EOF,"unexpected EOF in HCOM data");
                                return (0);
                        }
                        p->cksum += p->current;
                        p->nrbits = 32;
                }
                if(p->current & 0x80000000) {
                        p->dictentry =
                                p->dictionary[p->dictentry].dict_rightson;
                } else {
                        p->dictentry =
                                p->dictionary[p->dictentry].dict_leftson;
                }
                p->current = p->current << 1;
                p->nrbits--;
                if(p->dictionary[p->dictentry].dict_leftson < 0) {
                        short datum;
                        datum = p->dictionary[p->dictentry].dict_rightson;
                        if (!p->deltacompression)
                                p->sample = 0;
                        p->sample = (p->sample + datum) & 0xff;
                        p->huffcount--;
                        *buf++ = SOX_UNSIGNED_BYTE_TO_SAMPLE(p->sample,);
                        p->dictentry = 0;
                        done++;
                        len--;
                        if (len == 0)
                                break;
                }
        }

        return done;
}

static int sox_hcomstopread(ft_t ft)
{
        register struct readpriv *p = (struct readpriv *) ft->priv;

        if (p->huffcount != 0)
        {
                sox_fail_errno(ft,SOX_EFMT,"not all HCOM data read");
                return (SOX_EOF);
        }
        if(p->cksum != p->checksum)
        {
                sox_fail_errno(ft,SOX_EFMT,"checksum error in HCOM data");
                return (SOX_EOF);
        }
        free((char *)p->dictionary);
        p->dictionary = NULL;
        return (SOX_SUCCESS);
}

struct writepriv {
  unsigned char *data;          /* Buffer allocated with xmalloc */
  sox_size_t size;               /* Size of allocated buffer */
  sox_size_t pos;                /* Where next byte goes */
};

#define BUFINCR (10*BUFSIZ)

static int sox_hcomstartwrite(ft_t ft)
{
        register struct writepriv *p = (struct writepriv *) ft->priv;

        switch (ft->signal.rate) {
        case 22050:
        case 22050/2:
        case 22050/3:
        case 22050/4:
                break;
        default:
                sox_fail_errno(ft,SOX_EFMT,"unacceptable output rate for HCOM: try 5512, 7350, 11025 or 22050 hertz");
                return (SOX_EOF);
        }
        ft->signal.size = SOX_SIZE_BYTE;
        ft->signal.encoding = SOX_ENCODING_UNSIGNED;
        ft->signal.channels = 1;

        p->size = BUFINCR;
        p->pos = 0;
        p->data = (unsigned char *) xmalloc(p->size);
        return (SOX_SUCCESS);
}

static sox_size_t sox_hcomwrite(ft_t ft, const sox_ssample_t *buf, sox_size_t len)
{
  struct writepriv *p = (struct writepriv *) ft->priv;
  sox_ssample_t datum;
  sox_size_t i;

  if (len == 0)
    return 0;

  if (p->pos + len > p->size) {
    p->size = ((p->pos + len) / BUFINCR + 1) * BUFINCR;
    p->data = (unsigned char *)xrealloc(p->data, p->size);
  }

  for (i = 0; i < len; i++) {
    datum = *buf++;
    p->data[p->pos++] = SOX_SAMPLE_TO_UNSIGNED_BYTE(datum, ft->clips);
  }

  return len;
}

static void makecodes(int e, int c, int s, int b, dictent newdict[511], long codes[256], long codesize[256])
{
  assert(b);                    /* Prevent stack overflow */
  if (newdict[e].dict_leftson < 0) {
    codes[newdict[e].dict_rightson] = c;
    codesize[newdict[e].dict_rightson] = s;
  } else {
    makecodes(newdict[e].dict_leftson, c, s + 1, b << 1, newdict, codes, codesize);
    makecodes(newdict[e].dict_rightson, c + b, s + 1, b << 1, newdict, codes, codesize);
  }
}

static void putcode(ft_t ft, long codes[256], long codesize[256], unsigned c, unsigned char **df)
{
  struct readpriv *p = (struct readpriv *) ft->priv;
  long code, size;
  int i;

  code = codes[c];
  size = codesize[c];
  for(i = 0; i < size; i++) {
    p->curword <<= 1;
    if (code & 1)
      p->curword += 1;
    p->nbits++;
    if (p->nbits == 32) {
      put32_be(df, p->curword);
      p->new_checksum += p->curword;
      p->nbits = 0;
      p->curword = 0;
    }
    code >>= 1;
  }
}

static void compress(ft_t ft, unsigned char **df, int32_t *dl, sox_rate_t fr)
{
  struct readpriv *p = (struct readpriv *) ft->priv;
  int32_t samplerate;
  unsigned char *datafork = *df;
  unsigned char *ddf, *dfp;
  short dictsize;
  int frequtable[256];
  long codes[256], codesize[256];
  dictent newdict[511];
  int i, sample, j, k, d, l, frequcount;

  sample = *datafork;
  memset(frequtable, 0, sizeof(frequtable));
  memset(codes, 0, sizeof(codes));
  memset(codesize, 0, sizeof(codesize));
  memset(newdict, 0, sizeof(newdict));
  
  for (i = 1; i < *dl; i++) {
    d = (datafork[i] - (sample & 0xff)) & 0xff; /* creates absolute entries LMS */
    sample = datafork[i];
    datafork[i] = d;
    assert(d >= 0 && d <= 255); /* check our table is accessed correctly */
    frequtable[d]++;
  }
  p->de = newdict;
  for (i = 0; i < 256; i++)
    if (frequtable[i] != 0) {
      p->de->frequ = -frequtable[i];
      p->de->dict_leftson = -1;
      p->de->dict_rightson = i;
      p->de++;
    }
  frequcount = p->de - newdict;
  for (i = 0; i < frequcount; i++) {
    for (j = i + 1; j < frequcount; j++) {
      if (newdict[i].frequ > newdict[j].frequ) {
        k = newdict[i].frequ;
        newdict[i].frequ = newdict[j].frequ;
        newdict[j].frequ = k;
        k = newdict[i].dict_leftson;
        newdict[i].dict_leftson = newdict[j].dict_leftson;
        newdict[j].dict_leftson = k;
        k = newdict[i].dict_rightson;
        newdict[i].dict_rightson = newdict[j].dict_rightson;
        newdict[j].dict_rightson = k;
      }
    }
  }
  while (frequcount > 1) {
    j = frequcount - 1;
    p->de->frequ = newdict[j - 1].frequ;
    p->de->dict_leftson = newdict[j - 1].dict_leftson;
    p->de->dict_rightson = newdict[j - 1].dict_rightson;
    l = newdict[j - 1].frequ + newdict[j].frequ;
    for (i = j - 2; i >= 0 && l < newdict[i].frequ; i--)
      newdict[i + 1] = newdict[i];
    i = i + 1;
    newdict[i].frequ = l;
    newdict[i].dict_leftson = j;
    newdict[i].dict_rightson = p->de - newdict;
    p->de++;
    frequcount--;
  }
  dictsize = p->de - newdict;
  makecodes(0, 0, 0, 1, newdict, codes, codesize);
  l = 0;
  for (i = 0; i < 256; i++)
    l += frequtable[i] * codesize[i];
  l = (((l + 31) >> 5) << 2) + 24 + dictsize * 4;
  sox_debug("  Original size: %6d bytes", *dl);
  sox_debug("Compressed size: %6d bytes", l);
  datafork = (unsigned char *)xmalloc((unsigned)l);
  ddf = datafork + 22;
  for(i = 0; i < dictsize; i++) {
    put16_be(&ddf, newdict[i].dict_leftson);
    put16_be(&ddf, newdict[i].dict_rightson);
  }
  *ddf++ = 0;
  *ddf++ = *(*df)++;
  p->new_checksum = 0;
  p->nbits = 0;
  p->curword = 0;
  for (i = 1; i < *dl; i++)
    putcode(ft, codes, codesize, *(*df)++, &ddf);
  if (p->nbits != 0) {
    codes[0] = 0;
    codesize[0] = 32 - p->nbits;
    putcode(ft, codes, codesize, 0, &ddf);
  }
  strncpy((char *)datafork, "HCOM", 4);
  dfp = datafork + 4;
  put32_be(&dfp, *dl);
  put32_be(&dfp, p->new_checksum);
  put32_be(&dfp, 1);
  samplerate = 22050 / fr;
  put32_be(&dfp, samplerate);
  put16_be(&dfp, dictsize);
  *df = datafork;               /* reassign passed pointer to new datafork */
  *dl = l;                      /* and its compressed length */
}

/* End of hcom utility routines */

static int sox_hcomstopwrite(ft_t ft)
{
  struct writepriv *p = (struct writepriv *) ft->priv;
  unsigned char *compressed_data = p->data;
  sox_size_t compressed_len = p->pos;
  int rc = SOX_SUCCESS;

  /* Compress it all at once */
  if (compressed_len)
    compress(ft, &compressed_data, (int32_t *)&compressed_len, ft->signal.rate);
  free((char *)p->data);

  /* Write the header */
  sox_writebuf(ft, (void *)"\000\001A", 3); /* Dummy file name "A" */
  sox_padbytes(ft, 65-3);
  sox_writes(ft, "FSSD");
  sox_padbytes(ft, 83-69);
  sox_writedw(ft, compressed_len); /* compressed_data size */
  sox_writedw(ft, 0); /* rsrc size */
  sox_padbytes(ft, 128 - 91);
  if (sox_error(ft)) {
    sox_fail_errno(ft, errno, "write error in HCOM header");
    rc = SOX_EOF;
  } else if (sox_writebuf(ft, compressed_data, compressed_len) != compressed_len) {
    /* Write the compressed_data fork */
    sox_fail_errno(ft, errno, "can't write compressed HCOM data");
    rc = SOX_EOF;
  }
  free((char *)compressed_data);

  if (rc == SOX_SUCCESS)
    /* Pad the compressed_data fork to a multiple of 128 bytes */
    sox_padbytes(ft, 128u - (compressed_len % 128));

  return rc;
}

/* Mac FSSD/HCOM */
static const char *hcomnames[] = {
  "hcom",
  NULL
};

static sox_format_t sox_hcom_format = {
  hcomnames,
  SOX_FILE_BIG_END,
  sox_hcomstartread,
  sox_hcomread,
  sox_hcomstopread,
  sox_hcomstartwrite,
  sox_hcomwrite,
  sox_hcomstopwrite,
  sox_format_nothing_seek
};

const sox_format_t *sox_hcom_format_fn(void)
{
    return &sox_hcom_format;
}
