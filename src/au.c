/*
 * Copyright 1991, 1992, 1993 Guido van Rossum And Sundry Contributors.
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Guido van Rossum And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 *
 * October 7, 1998 - cbagwell@sprynet.com
 *   G.723 was using incorrect # of bits.  Corrected to 3 and 5 bits.
 */

/*
 * libSoX Sun format with header (SunOS 4.1; see /usr/demo/SOUND).
 * NeXT uses this format also, but has more format codes defined.
 * DEC uses a slight variation and swaps bytes.
 * We only support the common formats.
 * CCITT G.721 (32 kbit/s) and G.723 (24/40 kbit/s) are also supported,
 * courtesy Sun's public domain implementation.
 * Output is always in big-endian (Sun/NeXT) order.
 */

#include "sox_i.h"
#include "g72x.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* Magic numbers used in Sun and NeXT audio files */
#define SUN_MAGIC       0x2e736e64              /* Really '.snd' */
#define SUN_INV_MAGIC   0x646e732e              /* '.snd' upside-down */
#define DEC_MAGIC       0x2e736400              /* Really '\0ds.' (for DEC) */
#define DEC_INV_MAGIC   0x0064732e              /* '\0ds.' upside-down */
#define SUN_HDRSIZE     24                      /* Size of minimal header */
#define SUN_UNSPEC      ((unsigned)(~0))        /* Unspecified data size */
#define SUN_ENCODING_UNKNOWN 0
#define SUN_ULAW        1                       /* u-law encoding */
#define SUN_LIN_8       2                       /* Linear 8 bits */
#define SUN_LIN_16      3                       /* Linear 16 bits */
#define SUN_LIN_24      4                       /* Linear 24 bits */
#define SUN_LIN_32      5                       /* Linear 32 bits */
#define SUN_FLOAT       6                       /* IEEE FP 32 bits */
#define SUN_DOUBLE      7                       /* IEEE FP 64 bits */
#define SUN_G721        23                      /* CCITT G.721 4-bits ADPCM */
#define SUN_G723_3      25                      /* CCITT G.723 3-bits ADPCM */
#define SUN_G723_5      26                      /* CCITT G.723 5-bits ADPCM */
#define SUN_ALAW        27                      /* a-law encoding */
/* The other formats are not supported by sox at the moment */

/* Private data */
typedef struct aupriv {
        /* For writer: size in bytes */
        sox_size_t data_size;
        /* For seeking */
        sox_size_t dataStart;
        /* For G72x decoding: */
        struct g72x_state state;
        int (*dec_routine)(int i, int out_coding, struct g72x_state *state_ptr);
        int dec_bits;
        unsigned int in_buffer;
        int in_bits;
} *au_t;

static void auwriteheader(sox_format_t * ft, sox_size_t data_size);

static int sox_auencodingandsize(uint32_t sun_encoding, sox_encoding_t * encoding, int * size)
{
    switch (sun_encoding) {
    case SUN_ULAW:
            *encoding = SOX_ENCODING_ULAW;
            *size = SOX_SIZE_BYTE;
            break;
    case SUN_ALAW:
            *encoding = SOX_ENCODING_ALAW;
            *size = SOX_SIZE_BYTE;
            break;
    case SUN_LIN_8:
            *encoding = SOX_ENCODING_SIGN2;
            *size = SOX_SIZE_BYTE;
            break;
    case SUN_LIN_16:
            *encoding = SOX_ENCODING_SIGN2;
            *size = SOX_SIZE_16BIT;
            break;
    case SUN_LIN_24:
            *encoding = SOX_ENCODING_SIGN2;
            *size = SOX_SIZE_24BIT;
            break;
    case SUN_G721:
            *encoding = SOX_ENCODING_SIGN2;
            *size = SOX_SIZE_16BIT;
            break;
    case SUN_G723_3:
            *encoding = SOX_ENCODING_SIGN2;
            *size = SOX_SIZE_16BIT;
            break;
    case SUN_G723_5:
            *encoding = SOX_ENCODING_SIGN2;
            *size = SOX_SIZE_16BIT;
            break;
    case SUN_FLOAT:
            *encoding = SOX_ENCODING_FLOAT;
            *size = SOX_SIZE_32BIT;
            break;
    default:
            sox_debug("encoding: 0x%lx", sun_encoding);
            return(SOX_EOF);
    }
    return(SOX_SUCCESS);
}

static int sox_auseek(sox_format_t * ft, sox_size_t offset) 
{
    au_t au = (au_t ) ft->priv;

    if (au->dec_routine != NULL)
    {
        sox_fail_errno(ft,SOX_ENOTSUP,"Sorry, DEC unsupported");
    }
    else 
    {
        sox_size_t new_offset, channel_block, alignment;

        new_offset = offset * ft->signal.size;
        /* Make sure request aligns to a channel block (ie left+right) */
        channel_block = ft->signal.channels * ft->signal.size;
        alignment = new_offset % channel_block;
        /* Most common mistaken is to compute something like
         * "skip everthing upto and including this sample" so
         * advance to next sample block in this case.
         */
        if (alignment != 0)
            new_offset += (channel_block - alignment);
        new_offset += au->dataStart;

        ft->sox_errno = sox_seeki(ft, (sox_ssize_t)new_offset, SEEK_SET);
    }

    return(ft->sox_errno);
}

static int sox_austartread(sox_format_t * ft) 
{
        /* The following 6 variables represent a Sun sound header on disk.
           The numbers are written as big-endians.
           Any extra bytes (totalling hdr_size - 24) are an
           "info" field of unspecified nature, usually a string.
           By convention the header size is a multiple of 4. */
        uint32_t magic;
        uint32_t hdr_size;
        uint32_t data_size;
        uint32_t encoding;
        uint32_t sample_rate;
        uint32_t channels;

        unsigned int i;
        char *buf;
        au_t p = (au_t ) ft->priv;

        int rc;

        /* Check the magic word */
        sox_readdw(ft, &magic);
        if (magic == DEC_INV_MAGIC) {
            sox_debug("Found inverted DEC magic word.");
            /* Inverted headers are not standard.  Code was probably
             * left over from pre-standardize period of testing for
             * endianess.  Its not hurting though.
             */
            ft->signal.reverse_bytes = SOX_IS_BIGENDIAN;
        }
        else if (magic == SUN_INV_MAGIC) {
            sox_debug("Found inverted Sun/NeXT magic word.");
            ft->signal.reverse_bytes = SOX_IS_BIGENDIAN;
        }
        else if (magic == SUN_MAGIC) {
            sox_debug("Found Sun/NeXT magic word");
            ft->signal.reverse_bytes = SOX_IS_LITTLEENDIAN;
        }
        else if (magic == DEC_MAGIC) {
            sox_debug("Found DEC magic word");
            ft->signal.reverse_bytes = SOX_IS_LITTLEENDIAN;
        }
        else
        {
                sox_fail_errno(ft,SOX_EHDR,"Did not detect valid Sun/NeXT/DEC magic number in header.");
                return(SOX_EOF);
        }

        /* Read the header size */
        sox_readdw(ft, &hdr_size);
        if (hdr_size < SUN_HDRSIZE)
        {
                sox_fail_errno(ft,SOX_EHDR,"Sun/NeXT header size too small.");
                return(SOX_EOF);
        }

        /* Read the data size; may be ~0 meaning unspecified */
        sox_readdw(ft, &data_size);

        /* Read the encoding; there are some more possibilities */
        sox_readdw(ft, &encoding);


        /* Translate the encoding into encoding and size parameters */
        /* (Or, for G.72x, set the decoding routine and parameters) */
        p->dec_routine = NULL;
        p->in_buffer = 0;
        p->in_bits = 0;
        if(sox_auencodingandsize(encoding, &(ft->signal.encoding),
                             &(ft->signal.size)) == SOX_EOF)
        {
            sox_fail_errno(ft,SOX_EFMT,"Unsupported encoding in Sun/NeXT header.\nOnly U-law, signed bytes, signed words, ADPCM, and 32-bit floats are supported.");
            return(SOX_EOF);
        }
        switch (encoding) {
        case SUN_G721:
                g72x_init_state(&p->state);
                p->dec_routine = g721_decoder;
                p->dec_bits = 4;
                break;
        case SUN_G723_3:
                g72x_init_state(&p->state);
                p->dec_routine = g723_24_decoder;
                p->dec_bits = 3;
                break;
        case SUN_G723_5:
                g72x_init_state(&p->state);
                p->dec_routine = g723_40_decoder;
                p->dec_bits = 5;
                break;
        }


        /* Read the sampling rate */
        sox_readdw(ft, &sample_rate);
        if (ft->signal.rate == 0 || ft->signal.rate == sample_rate)
            ft->signal.rate = sample_rate;
        else
            sox_report("User options overriding rate read in .au header");

        /* Read the number of channels */
        sox_readdw(ft, &channels);
        if (ft->signal.channels == 0 || ft->signal.channels == channels)
            ft->signal.channels = channels;
        else
            sox_report("User options overriding channels read in .au header");


        /* Skip the info string in header; print it if verbose */
        hdr_size -= SUN_HDRSIZE; /* #bytes already read */
        if (hdr_size > 0) {
                /* Allocate comment buffer */
                buf = (char *) xmalloc(hdr_size+1);
                
                for(i = 0; i < hdr_size; i++) {
                        sox_readb(ft, (unsigned char *)&(buf[i]));
                        if (sox_eof(ft))
                        {
                                sox_fail_errno(ft,SOX_EOF,"Unexpected EOF in Sun/NeXT header info.");
                                return(SOX_EOF);
                        }
                }
                /* Buffer should already be null terminated but
                 * just in case we xmalloced an extra byte and 
                 * force the last byte to be 0 anyways.
                 * This should help work with a greater array of
                 * software.
                 */
                buf[hdr_size] = '\0';

                ft->comment = buf;
        }
        /* Needed for seeking */
        ft->length = data_size/ft->signal.size;
        if(ft->seekable)
                p->dataStart = sox_tell(ft);

        /* Needed for rawread() */
        rc = sox_rawstartread(ft);
        if (rc)
            return rc;

        return(SOX_SUCCESS);
}

/* When writing, the header is supposed to contain the number of
   data bytes written, unless it is written to a pipe.
   Since we don't know how many bytes will follow until we're done,
   we first write the header with an unspecified number of bytes,
   and at the end we rewind the file and write the header again
   with the right size.  This only works if the file is seekable;
   if it is not, the unspecified size remains in the header
   (this is legal). */

static int sox_austartwrite(sox_format_t * ft) 
{
        au_t p = (au_t ) ft->priv;
        int rc;

        /* Needed because of rawwrite(); */
        rc = sox_rawstartwrite(ft);
        if (rc)
            return rc;

        p->data_size = 0;
        auwriteheader(ft, SUN_UNSPEC);
        return(SOX_SUCCESS);
}

/*
 * Unpack input codes and pass them back as bytes.
 * Returns 1 if there is residual input, returns -1 if eof, else returns 0.
 * (Adapted from Sun's decode.c.)
 */
static int unpack_input(sox_format_t * ft, unsigned char *code)
{
        au_t p = (au_t ) ft->priv;
        unsigned char           in_byte;

        if (p->in_bits < p->dec_bits) {
                if (sox_readb(ft, &in_byte) == SOX_EOF) {
                        *code = 0;
                        return (-1);
                }
                p->in_buffer |= (in_byte << p->in_bits);
                p->in_bits += 8;
        }
        *code = p->in_buffer & ((1 << p->dec_bits) - 1);
        p->in_buffer >>= p->dec_bits;
        p->in_bits -= p->dec_bits;
        return (p->in_bits > 0);
}

static sox_size_t sox_auread(sox_format_t * ft, sox_ssample_t *buf, sox_size_t samp)
{
        au_t p = (au_t ) ft->priv;
        unsigned char code;
        int done;
        if (p->dec_routine == NULL)
                return sox_rawread(ft, buf, samp);
        done = 0;
        while (samp > 0 && unpack_input(ft, &code) >= 0) {
                *buf++ = SOX_SIGNED_16BIT_TO_SAMPLE(
                        (*p->dec_routine)(code, AUDIO_ENCODING_LINEAR,
                                          &p->state),);
                samp--;
                done++;
        }
        return done;
}

static sox_size_t sox_auwrite(sox_format_t * ft, const sox_ssample_t *buf, sox_size_t samp)
{
        au_t p = (au_t ) ft->priv;
        p->data_size += samp * ft->signal.size;
        return(sox_rawwrite(ft, buf, samp));
}

static int sox_austopwrite(sox_format_t * ft)
{
        au_t p = (au_t ) ft->priv;
        int rc;

        /* Needed because of rawwrite(). Do now to flush
         * data before seeking around below.
         */
        rc = sox_rawstopwrite(ft);
        if (rc)
            return rc;

        /* Attempt to update header */
        if (ft->seekable)
        {
          if (sox_seeki(ft, 0, 0) != 0)
          {
                sox_fail_errno(ft,errno,"Can't rewind output file to rewrite Sun header.");
                return(SOX_EOF);
          }
          auwriteheader(ft, p->data_size);
        }
        return(SOX_SUCCESS);
}

static unsigned sox_ausunencoding(int size, sox_encoding_t encoding)
{
        unsigned sun_encoding;

        if (encoding == SOX_ENCODING_ULAW && size == SOX_SIZE_BYTE)
                sun_encoding = SUN_ULAW;
        else if (encoding == SOX_ENCODING_ALAW && size == SOX_SIZE_BYTE)
                sun_encoding = SUN_ALAW;
        else if (encoding == SOX_ENCODING_SIGN2 && size == SOX_SIZE_BYTE)
                sun_encoding = SUN_LIN_8;
        else if (encoding == SOX_ENCODING_SIGN2 && size == SOX_SIZE_16BIT)
                sun_encoding = SUN_LIN_16;
        else if (encoding == SOX_ENCODING_SIGN2 && size == SOX_SIZE_24BIT)
                sun_encoding = SUN_LIN_24;
        else if (encoding == SOX_ENCODING_FLOAT && size == SOX_SIZE_32BIT)
                sun_encoding = SUN_FLOAT;
        else sun_encoding = SUN_ENCODING_UNKNOWN;
        return sun_encoding;
}

static void auwriteheader(sox_format_t * ft, sox_size_t data_size)
{
        uint32_t magic;
        uint32_t hdr_size;
        uint32_t encoding;
        uint32_t sample_rate;
        uint32_t channels;
        int   x;
        int   comment_size;

        encoding = sox_ausunencoding(ft->signal.size, ft->signal.encoding);
        if (encoding == SUN_ENCODING_UNKNOWN) {
          sox_report("Unsupported output encoding/size for Sun/NeXT header or .AU format not specified.");
          sox_report("Only u-law, A-law, and signed 8/16/24 bits are supported.");
          if (ft->signal.size > 2) {
            sox_report("Defaulting to signed 24 bit");
            ft->signal.encoding = SOX_ENCODING_SIGN2;
            ft->signal.size = SOX_SIZE_24BIT;
            encoding = SUN_LIN_24;
          }
          else if (ft->signal.size == 2) {
            sox_report("Defaulting to signed 16 bit");
            ft->signal.encoding = SOX_ENCODING_SIGN2;
            ft->signal.size = SOX_SIZE_16BIT;
            encoding = SUN_LIN_16;
          }
          else {
            sox_report("Defaulting to 8khz u-law");
            encoding = SUN_ULAW;
            ft->signal.encoding = SOX_ENCODING_ULAW;
            ft->signal.size = SOX_SIZE_BYTE;
            ft->signal.rate = 8000;  /* strange but true */
          }
        }

        magic = SUN_MAGIC;
        sox_writedw(ft, magic);

        /* Info field is at least 4 bytes. Here I force it to something
         * useful when there is no comments.
         */
        if (ft->comment == NULL)
                ft->comment = xstrdup("SOX");

        hdr_size = SUN_HDRSIZE;

        comment_size = strlen(ft->comment) + 1; /*+1 = null-term. */
        if (comment_size < 4)
            comment_size = 4; /* minimum size */

        hdr_size += comment_size;

        sox_writedw(ft, hdr_size);

        sox_writedw(ft, data_size);

        sox_writedw(ft, encoding);

        sample_rate = ft->signal.rate;
        sox_writedw(ft, sample_rate);

        channels = ft->signal.channels;
        sox_writedw(ft, channels);

        sox_writes(ft, ft->comment);

        /* Info must be 4 bytes at least and null terminated. */
        x = strlen(ft->comment);
        for (;x < 3; x++)
            sox_writeb(ft, 0);

        sox_writeb(ft, 0);
}

/* SPARC .au w/header */
static const char *aunames[] = {
  "au",
  "snd",
  NULL
};

/* Purposely did not specify format as big endian because
 * it can handle both.  This means we must set our own
 * default values for reverse_bytes when not specified
 * since we didn't give the hint to soxio.
 */
static sox_format_handler_t sox_au_format = {
  aunames,
  SOX_FILE_SEEK | SOX_FILE_BIG_END,
  sox_austartread,
  sox_auread,
  sox_rawstopread,
  sox_austartwrite,
  sox_auwrite,
  sox_austopwrite,
  sox_auseek
};

const sox_format_handler_t *sox_au_format_fn(void);

const sox_format_handler_t *sox_au_format_fn(void)
{
    return &sox_au_format;
}
