/* Yamaha TX-16W sampler file support
 *
 * May 20, 1993
 * Copyright 1993 Rob Talley   (rob@aii.com)
 * This source code is freely redistributable and may be used for
 * any purpose. This copyright notice and the following copyright 
 * notice must be maintained intact. No warranty whatsoever is
 * provided. This code is furnished AS-IS as a component of the
 * larger work Copyright 1991 Lance Norskog and Sundry Contributors.
 * Much appreciation to ross-c  for his sampConv utility for SGI/IRIX
 * from where these methods were derived.
 *
 * Jan 24, 1994
 * Pat McElhatton, HP Media Technology Lab <patmc@apollo.hp.com>
 * Handles reading of files which do not have the sample rate field
 * set to one of the expected by looking at some other bytes in the
 * attack/loop length fields, and defaulting to 33kHz if the sample
 * rate is still unknown.
 *
 * January 12, 1995
 * Copyright 1995 Mark Lakata (lakata@physics.berkeley.edu)
 * Additions to tx16w.c SOX driver.  This version writes as well as
 * reads TX16W format.
 *
 * July 31, 1998
 * Cleaned up by Leigh Smith (leigh@psychokiller.dialix.oz.au)
 * for incorporation into the main sox distribution.
 *
 * September 24, 1998
 * Forced output to mono signed words to match input.  It was basically
 * doing this anyways but now the user will see a display that it's been
 * overridden.  cbagwell@sprynet.com
 *
 */

#define TXMAXLEN 0x3FF80

/*
 * libSoX skeleton file format driver.
 */

#include <stdio.h>
#include <string.h>
#include "sox_i.h"

/* Private data for TX16 file */
typedef struct txwstuff {
        sox_size_t rest;                 /* bytes remaining in sample file */
} *txw_t;

struct WaveHeader_ {
  char filetype[6]; /* = "LM8953", */
  unsigned char
    nulls[10],
    dummy_aeg[6],    /* space for the AEG (never mind this) */
    format,          /* 0x49 = looped, 0xC9 = non-looped */
    sample_rate,     /* 1 = 33 kHz, 2 = 50 kHz, 3 = 16 kHz */
    atc_length[3],   /* I'll get to this... */
    rpt_length[3],
    unused[2];       /* set these to null, to be on the safe side */
} ;

static const unsigned char magic1[4] = {0, 0x06, 0x10, 0xF6};
static const unsigned char magic2[4] = {0, 0x52, 0x00, 0x52};

/* SJB: dangerous static variables */
static sox_size_t tx16w_len=0;
static sox_size_t writedone=0;

/*
 * Do anything required before you start reading samples.
 * Read file header. 
 *      Find out sampling rate, 
 *      size and encoding of samples,
 *      mono/stereo/quad.
 */
static int sox_txwstartread(ft_t ft)
{
    int c;
    char filetype[7];
    char format;
    char sample_rate;
    sox_size_t num_samp_bytes = 0;
    char gunk[8];
    int blewIt;
    uint32_t trash;

    txw_t sk = (txw_t) ft->priv;
    /* If you need to seek around the input file. */
    if (! ft->seekable)
    {
        sox_fail_errno(ft,SOX_EOF,"txw input file must be a file, not a pipe");
        return(SOX_EOF);
    }

    /* This is dumb but portable, just count the bytes til EOF */
    while (sox_readb(ft, (unsigned char *)&trash) != SOX_EOF)
        num_samp_bytes++; 
    num_samp_bytes -= 32;         /* calculate num samples by sub header size */
    sox_seeki(ft, 0, 0);           /* rewind file */
    sk->rest = num_samp_bytes;    /* set how many sample bytes to read */

    /* first 6 bytes are file type ID LM8953 */
    sox_readb(ft, (unsigned char *)&filetype[0]);
    sox_readb(ft, (unsigned char *)&filetype[1]);
    sox_readb(ft, (unsigned char *)&filetype[2]);
    sox_readb(ft, (unsigned char *)&filetype[3]);
    sox_readb(ft, (unsigned char *)&filetype[4]);
    sox_readb(ft, (unsigned char *)&filetype[5]);
    filetype[6] = '\0';
    for( c = 16; c > 0 ; c-- )    /* Discard next 16 bytes */
        sox_readb(ft, (unsigned char *)&trash);
    sox_readb(ft, (unsigned char *)&format);
    sox_readb(ft, (unsigned char *)&sample_rate);
    /*
     * save next 8 bytes - if sample rate is 0, then we need
     *  to look at gunk[2] and gunk[5] to get real rate
     */
    for( c = 0; c < 8; c++ )
        sox_readb(ft, (unsigned char *)&(gunk[c]));
    /*
     * We should now be pointing at start of raw sample data in file 
     */

    /* Check to make sure we got a good filetype ID from file */
    sox_debug("Found header filetype %s",filetype);
    if(strcmp(filetype,"LM8953"))
    {
        sox_fail_errno(ft,SOX_EHDR,"Invalid filetype ID in input file header, != LM8953");
        return(SOX_EOF);
    }
    /*
     * Set up the sample rate as indicated by the header
     */

    switch( sample_rate ) {
        case 1:
            ft->signal.rate = 33333;
            break;
        case 2:
            ft->signal.rate = 50000;
            break;
        case 3:
            ft->signal.rate = 16667;
            break;
        default:
            blewIt = 1;
            switch( gunk[2] & 0xFE ) {
                case 0x06:
                    if ( (gunk[5] & 0xFE) == 0x52 ) {
                        blewIt = 0;
                        ft->signal.rate = 33333;
                    }
                    break;
                case 0x10:
                    if ( (gunk[5] & 0xFE) == 0x00 ) {
                        blewIt = 0;
                        ft->signal.rate = 50000;
                    }
                    break;
                case 0xF6:
                    if ( (gunk[5] & 0xFE) == 0x52 ) {
                        blewIt = 0;
                        ft->signal.rate = 16667;
                    }
                    break;
            }
            if ( blewIt ) {
                sox_debug("Invalid sample rate identifier found %d", (int)sample_rate);
                ft->signal.rate = 33333;
            }
    }
    sox_debug("Sample rate = %ld",ft->signal.rate);

    ft->signal.channels = 1 ; /* not sure about stereo sample data yet ??? */
    ft->signal.size = SOX_SIZE_16BIT; /* this is close enough */
    ft->signal.encoding = SOX_ENCODING_SIGN2;

    return(SOX_SUCCESS);
}

/*
 * Read up to len samples from file.
 * Convert to sox_sample_ts.
 * Place in buf[].
 * Return number of samples read.
 */

static sox_size_t sox_txwread(ft_t ft, sox_sample_t *buf, sox_size_t len)
{
    txw_t sk = (txw_t) ft->priv;
    sox_size_t done = 0;
    unsigned char uc1,uc2,uc3;
    unsigned short s1,s2;

    /*
     * This gets called by the top level 'process' routine.
     * We will essentially get called with a buffer pointer
     * and a max length to read. Graciously, it is always
     * an even amount so we don't have to worry about
     * hanging onto the left over odd samples since there
     * won't be any. Something to look out for though :-(
     * We return the number of samples we read.
     * We will get called over and over again until we return
     *  0 bytes read.
     */

    /*
     * This is ugly but it's readable!
     * Read three bytes from stream, then decompose these into
     * two unsigned short samples. 
     * TCC 3.0 appeared to do unwanted things, so we really specify
     *  exactly what we want to happen.
     * Convert unsigned short to sox_sample_t then shift up the result
     *  so that the 12-bit sample lives in the most significant
     *  12-bits of the sox_sample_t.
     * This gets our two samples into the internal format which we
     * deposit into the given buffer and adjust our counts respectivly.
     */
    for(done = 0; done < len; ) {
        if(sk->rest < 3) break; /* Finished reading from file? */
        sox_readb(ft, &uc1);
        sox_readb(ft, &uc2);
        sox_readb(ft, &uc3);
        sk->rest -= 3; /* adjust remaining for bytes we just read */
        s1 = (unsigned short) (uc1 << 4) | (((uc2 >> 4) & 017));
        s2 = (unsigned short) (uc3 << 4) | (( uc2 & 017 ));
        *buf = (sox_sample_t) s1;
        *buf = (*buf << 20);
        buf++; /* sample one is done */
        *buf = (sox_sample_t) s2;
        *buf = (*buf << 20);
        buf++; /* sample two is done */
        done += 2; /* adjust converted & stored sample count */
    }
    return done;
}

static int sox_txwstartwrite(ft_t ft)
{
    struct WaveHeader_ WH;

    sox_debug("tx16w selected output");

    memset(&WH, 0, sizeof(struct WaveHeader_));

    if (ft->signal.channels != 1)
        sox_report("tx16w is overriding output format to 1 channel.");
    ft->signal.channels = 1 ; /* not sure about stereo sample data yet ??? */
    if (ft->signal.size != SOX_SIZE_16BIT || ft->signal.encoding != SOX_ENCODING_SIGN2)
        sox_report("tx16w is overriding output format to size Signed Word format.");
    ft->signal.size = SOX_SIZE_16BIT; /* this is close enough */
    ft->signal.encoding = SOX_ENCODING_SIGN2;

    /* If you have to seek around the output file */
    if (! ft->seekable)
    {
        sox_fail_errno(ft,SOX_EOF,"Output .txw file must be a file, not a pipe");
        return(SOX_EOF);
    }

    /* dummy numbers, just for place holder, real header is written
       at end of processing, since byte count is needed */

    sox_writebuf(ft, &WH, 1, 32);
    writedone = 32;
    return(SOX_SUCCESS);
}

static sox_size_t sox_txwwrite(ft_t ft, const sox_sample_t *buf, sox_size_t len)
{
    sox_size_t i;
    sox_sample_t w1,w2;

    tx16w_len += len;
    if (tx16w_len > TXMAXLEN) return 0;

    for (i=0;i<len;i+=2) {
        w1 =  *buf++ >> 20;
        if (i+1==len)
            w2 = 0;
        else {
            w2 =  *buf++ >> 20;
        }
        sox_writeb(ft, (w1 >> 4) & 0xFF);
        sox_writeb(ft, (((w1 & 0x0F) << 4) | (w2 & 0x0F)) & 0xFF);
        sox_writeb(ft, (w2 >> 4) & 0xFF);
        writedone += 3;
    }
    return(len);
}

static int sox_txwstopwrite(ft_t ft)
{
    struct WaveHeader_ WH;
    int AttackLength, LoopLength, i;

    /* All samples are already written out. */
    /* If file header needs fixing up, for example it needs the */
    /* the number of samples in a field, seek back and write them here. */

    sox_debug("tx16w:output finished");

    memset(&WH, 0, sizeof(struct WaveHeader_));
    strncpy(WH.filetype,"LM8953",6);
    for (i=0;i<10;i++) WH.nulls[i]=0;
    for (i=0;i<6;i++)  WH.dummy_aeg[i]=0;
    for (i=0;i<2;i++)  WH.unused[i]=0;
    for (i=0;i<2;i++)  WH.dummy_aeg[i] = 0;
    for (i=2;i<6;i++)  WH.dummy_aeg[i] = 0x7F;

    WH.format = 0xC9;   /* loop off */

    /* the actual sample rate is not that important ! */  
    if (ft->signal.rate < 24000)      WH.sample_rate = 3;
    else if (ft->signal.rate < 41000) WH.sample_rate = 1;
    else                            WH.sample_rate = 2;

    if (tx16w_len >= TXMAXLEN) {
        sox_warn("Sound too large for TX16W. Truncating, Loop Off");
        AttackLength       = TXMAXLEN/2;
        LoopLength         = TXMAXLEN/2;
    }
    else if (tx16w_len >=TXMAXLEN/2) {
        AttackLength       = TXMAXLEN/2;
        LoopLength         = tx16w_len - TXMAXLEN/2;
        if (LoopLength < 0x40) {
            LoopLength   +=0x40;
            AttackLength -= 0x40;
        }
    }    
    else if (tx16w_len >= 0x80) {
        AttackLength                       = tx16w_len -0x40;
        LoopLength                         = 0x40;
    }
    else {
        AttackLength                       = 0x40;
        LoopLength                         = 0x40;
        for(i=tx16w_len;i<0x80;i++) {
            sox_writeb(ft, 0);
            sox_writeb(ft, 0);
            sox_writeb(ft, 0);
            writedone += 3;
        }
    }

    /* Fill up to 256 byte blocks; the TX16W seems to like that */

    while ((writedone % 0x100) != 0) {
        sox_writeb(ft, 0);
        writedone++;
    }

    WH.atc_length[0] = 0xFF & AttackLength;
    WH.atc_length[1] = 0xFF & (AttackLength >> 8);
    WH.atc_length[2] = (0x01 & (AttackLength >> 16)) +
        magic1[WH.sample_rate];

    WH.rpt_length[0] = 0xFF & LoopLength;
    WH.rpt_length[1] = 0xFF & (LoopLength >> 8);
    WH.rpt_length[2] = (0x01 & (LoopLength >> 16)) +
        magic2[WH.sample_rate];

    sox_rewind(ft);
    sox_writebuf(ft, &WH, 1, 32);

    return(SOX_SUCCESS);
}

/* Yamaha TX16W and SY99 waves */
static const char *txwnames[] = {
  "txw",
  NULL
};

static sox_format_t sox_txw_format = {
   txwnames,
   NULL,
   0,
   sox_txwstartread,
   sox_txwread,
   sox_format_nothing,
   sox_txwstartwrite,
   sox_txwwrite,
   sox_txwstopwrite,
   sox_format_nothing_seek
};

const sox_format_t *sox_txw_format_fn(void)
{
    return &sox_txw_format;
}
