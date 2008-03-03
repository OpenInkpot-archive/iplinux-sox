/*
 * libSoX MAUD file format handler, by Lutz Vieweg 1993
 *
 * supports: mono and stereo, linear, a-law and u-law reading and writing
 *
 * Copyright 1998-2006 Chris Bagwell and SoX Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

#include "sox_i.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

/* Private data for MAUD file */
struct maudstuff { /* max. 100 bytes!!!! */
        uint32_t nsamples;
};

static void maudwriteheader(sox_format_t *);

/*
 * Do anything required before you start reading samples.
 * Read file header. 
 *      Find out sampling rate, 
 *      size and encoding of samples, 
 *      mono/stereo/quad.
 */
static int startread(sox_format_t * ft) 
{
        struct maudstuff * p = (struct maudstuff *) ft->priv;
        
        char buf[12];
        char *chunk_buf;
        
        unsigned short bitpersam;
        uint32_t nom;
        unsigned short denom;
        unsigned short chaninf;
        
        uint32_t chunksize;
        uint32_t trash32;
        uint16_t trash16;
        int rc;

        /* Needed for rawread() */
        rc = sox_rawstartread(ft);
        if (rc)
            return rc;

        /* read FORM chunk */
        if (sox_reads(ft, buf, 4) == SOX_EOF || strncmp(buf, "FORM", 4) != 0)
        {
                sox_fail_errno(ft,SOX_EHDR,"MAUD: header does not begin with magic word 'FORM'");
                return (SOX_EOF);
        }
        
        sox_readdw(ft, &trash32); /* totalsize */
        
        if (sox_reads(ft, buf, 4) == SOX_EOF || strncmp(buf, "MAUD", 4) != 0)
        {
                sox_fail_errno(ft,SOX_EHDR,"MAUD: 'FORM' chunk does not specify 'MAUD' as type");
                return(SOX_EOF);
        }
        
        /* read chunks until 'BODY' (or end) */
        
        while (sox_reads(ft, buf, 4) == SOX_SUCCESS && strncmp(buf,"MDAT",4) != 0) {
                
                /*
                buf[4] = 0;
                sox_debug("chunk %s",buf);
                */
                
                if (strncmp(buf,"MHDR",4) == 0) {
                        
                        sox_readdw(ft, &chunksize);
                        if (chunksize != 8*4) 
                        {
                            sox_fail_errno(ft,SOX_EHDR,"MAUD: MHDR chunk has bad size");
                            return(SOX_EOF);
                        }
                        
                        /* fseeko(ft->fp,12,SEEK_CUR); */

                        /* number of samples stored in MDAT */
                        sox_readdw(ft, &(p->nsamples));

                        /* number of bits per sample as stored in MDAT */
                        sox_readw(ft, &bitpersam);

                        /* number of bits per sample after decompression */
                        sox_readw(ft, (unsigned short *)&trash16);

                        sox_readdw(ft, &nom);         /* clock source frequency */
                        sox_readw(ft, &denom);       /* clock devide           */
                        if (denom == 0) 
                        {
                            sox_fail_errno(ft,SOX_EHDR,"MAUD: frequency denominator == 0, failed");
                            return (SOX_EOF);
                        }
                        
                        ft->signal.rate = nom / denom;
                        
                        sox_readw(ft, &chaninf); /* channel information */
                        switch (chaninf) {
                        case 0:
                                ft->signal.channels = 1;
                                break;
                        case 1:
                                ft->signal.channels = 2;
                                break;
                        default:
                                sox_fail_errno(ft,SOX_EFMT,"MAUD: unsupported number of channels in file");
                                return (SOX_EOF);
                        }
                        
                        sox_readw(ft, &chaninf); /* number of channels (mono: 1, stereo: 2, ...) */
                        if (chaninf != ft->signal.channels) 
                        {
                                sox_fail_errno(ft,SOX_EFMT,"MAUD: unsupported number of channels in file");
                            return(SOX_EOF);
                        }
                        
                        sox_readw(ft, &chaninf); /* compression type */
                        
                        sox_readdw(ft, &trash32); /* rest of chunk, unused yet */
                        sox_readdw(ft, &trash32);
                        sox_readdw(ft, &trash32);
                        
                        if (bitpersam == 8 && chaninf == 0) {
                                ft->encoding.bits_per_sample = 8;
                                ft->encoding.encoding = SOX_ENCODING_UNSIGNED;
                        }
                        else if (bitpersam == 8 && chaninf == 2) {
                                ft->encoding.bits_per_sample = 8;
                                ft->encoding.encoding = SOX_ENCODING_ALAW;
                        }
                        else if (bitpersam == 8 && chaninf == 3) {
                                ft->encoding.bits_per_sample = 8;
                                ft->encoding.encoding = SOX_ENCODING_ULAW;
                        }
                        else if (bitpersam == 16 && chaninf == 0) {
                                ft->encoding.bits_per_sample = 16;
                                ft->encoding.encoding = SOX_ENCODING_SIGN2;
                        }
                        else 
                        {
                                sox_fail_errno(ft,SOX_EFMT,"MAUD: unsupported compression type detected");
                                return(SOX_EOF);
                        }
                        
                        continue;
                }
                
                if (strncmp(buf,"ANNO",4) == 0) {
                        sox_readdw(ft, &chunksize);
                        if (chunksize & 1)
                                chunksize++;
                        chunk_buf = (char *) xmalloc(chunksize + 1);
                        if (sox_readbuf(ft, chunk_buf, chunksize) 
                            != chunksize)
                        {
                                sox_fail_errno(ft,SOX_EOF,"MAUD: Unexpected EOF in ANNO header");
                                return(SOX_EOF);
                        }
                        chunk_buf[chunksize] = '\0';
                        sox_debug("%s",chunk_buf);
                        free(chunk_buf);
                        
                        continue;
                }
                
                /* some other kind of chunk */
                sox_readdw(ft, &chunksize);
                if (chunksize & 1)
                        chunksize++;
                sox_seeki(ft, (sox_ssize_t)chunksize, SEEK_CUR);
                continue;
                
        }
        
        if (strncmp(buf,"MDAT",4) != 0) 
        {
            sox_fail_errno(ft,SOX_EFMT,"MAUD: MDAT chunk not found");
            return(SOX_EOF);
        }
        sox_readdw(ft, &(p->nsamples));
        return(SOX_SUCCESS);
}

static int startwrite(sox_format_t * ft) 
{
        struct maudstuff * p = (struct maudstuff *) ft->priv;
        int rc;

        /* Needed for rawwrite() */
        rc = sox_rawstartwrite(ft);
        if (rc)
            return rc;

        /* If you have to seek around the output file */
        if (! ft->seekable) 
        {
            sox_fail_errno(ft,SOX_EOF,"Output .maud file must be a file, not a pipe");
            return (SOX_EOF);
        }
        p->nsamples = 0x7f000000;
        maudwriteheader(ft);
        p->nsamples = 0;
        return (SOX_SUCCESS);
}

static sox_size_t write_samples(sox_format_t * ft, const sox_sample_t *buf, sox_size_t len) 
{
        struct maudstuff * p = (struct maudstuff *) ft->priv;
        
        p->nsamples += len;
        
        return sox_rawwrite(ft, buf, len);
}

static int stopwrite(sox_format_t * ft) 
{
        /* All samples are already written out. */
        
        if (sox_seeki(ft, 0, 0) != 0) 
        {
            sox_fail_errno(ft,errno,"can't rewind output file to rewrite MAUD header");
            return(SOX_EOF);
        }
        
        maudwriteheader(ft);
        return(SOX_SUCCESS);
}

#define MAUDHEADERSIZE (4+(4+4+32)+(4+4+32)+(4+4))
static void maudwriteheader(sox_format_t * ft)
{
        struct maudstuff * p = (struct maudstuff *) ft->priv;
        
        sox_writes(ft, "FORM");
        sox_writedw(ft, (p->nsamples* (ft->encoding.bits_per_sample >> 3)) + MAUDHEADERSIZE);  /* size of file */
        sox_writes(ft, "MAUD"); /* File type */
        
        sox_writes(ft, "MHDR");
        sox_writedw(ft,  8*4); /* number of bytes to follow */
        sox_writedw(ft, p->nsamples);  /* number of samples stored in MDAT */
        
        switch (ft->encoding.encoding) {
                
        case SOX_ENCODING_UNSIGNED:
          sox_writew(ft, 8); /* number of bits per sample as stored in MDAT */
          sox_writew(ft, 8); /* number of bits per sample after decompression */
          break;
                
        case SOX_ENCODING_SIGN2:
          sox_writew(ft, 16); /* number of bits per sample as stored in MDAT */
          sox_writew(ft, 16); /* number of bits per sample after decompression */
          break;
                
        case SOX_ENCODING_ALAW:
        case SOX_ENCODING_ULAW:
          sox_writew(ft, 8); /* number of bits per sample as stored in MDAT */
          sox_writew(ft, 16); /* number of bits per sample after decompression */
          break;

        default:
          break;
        }
        
        sox_writedw(ft, (unsigned)(ft->signal.rate + .5)); /* sample rate, Hz */
        sox_writew(ft, (int) 1); /* clock devide */
        
        if (ft->signal.channels == 1) {
          sox_writew(ft, 0); /* channel information */
          sox_writew(ft, 1); /* number of channels (mono: 1, stereo: 2, ...) */
        }
        else {
          sox_writew(ft, 1);
          sox_writew(ft, 2);
        }
        
        switch (ft->encoding.encoding) {
                
        case SOX_ENCODING_UNSIGNED:
        case SOX_ENCODING_SIGN2:
          sox_writew(ft, 0); /* no compression */
          break;
                
        case SOX_ENCODING_ULAW:
          sox_writew(ft, 3);
          break;
                
        case SOX_ENCODING_ALAW:
          sox_writew(ft, 2);
          break;

        default:
          break;
        }
        
        sox_writedw(ft, 0); /* reserved */
        sox_writedw(ft, 0); /* reserved */
        sox_writedw(ft, 0); /* reserved */
        
        sox_writes(ft, "ANNO");
        sox_writedw(ft, 30); /* length of block */
        sox_writes(ft, "file create by Sound eXchange ");
        
        sox_writes(ft, "MDAT");
        sox_writedw(ft, p->nsamples * (ft->encoding.bits_per_sample >> 3)); /* samples in file */
}

SOX_FORMAT_HANDLER(maud)
{
  static char const * const names[] = {"maud", NULL};
  static unsigned const write_encodings[] = {
    SOX_ENCODING_SIGN2, 16, 0,
    SOX_ENCODING_UNSIGNED, 8, 0,
    SOX_ENCODING_ULAW, 8, 0,
    SOX_ENCODING_ALAW, 8, 0,
    0};
  static sox_format_handler_t const handler = {
    names, SOX_FILE_BIG_END | SOX_FILE_MONO | SOX_FILE_STEREO,
    startread, sox_rawread, sox_rawstopread,
    startwrite, write_samples, stopwrite,
    NULL, write_encodings, NULL
  };
  return &handler;
}
