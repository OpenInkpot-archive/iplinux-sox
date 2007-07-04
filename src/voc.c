/*
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Lance Norskog And Sundry Contributors are not responsible for
 * the consequences of using this software.
 *
 * September 8, 1993
 * Copyright 1993 T. Allen Grider - for changes to support block type 9
 * and word sized samples.  Same caveats and disclaimer as above.
 *
 * February 22, 1996
 * by Chris Bagwell (cbagwell@sprynet.com)
 * Added support for block type 8 (extended) which allows for 8-bit stereo
 * files.  Added support for saving stereo files and 16-bit files.
 * Added VOC format info from audio format FAQ so I don't have to keep
 * looking around for it.
 *
 * February 5, 2001
 * For sox-12-17 by Annonymous (see notes ANN)
 * Added comments and notes for each procedure.
 * Fixed so this now works with pipes, input does not have to
 * be seekable anymore (in sox_vocstartread() )
 * Added support for uLAW and aLaw (aLaw not tested).
 * Fixed support of multi-part VOC files, and files with
 * block 9 but no audio in the block....
 * The following need to be tested:  16-bit, 2 channel, and aLaw.
 *
 * December 10, 2001
 * For sox-12-17-3 by Annonymous (see notes ANN)
 * Patch for sox-12-17 merged with sox-12-17-3-pre3 code.
 *
 */

/*
 * libSoX Sound Blaster VOC handler sources.
 */

/*------------------------------------------------------------------------
The following is taken from the Audio File Formats FAQ dated 2-Jan-1995
and submitted by Guido van Rossum <guido@cwi.nl>.
--------------------------------------------------------------------------
Creative Voice (VOC) file format
--------------------------------

From: galt@dsd.es.com

(byte numbers are hex!)

    HEADER (bytes 00-19)
    Series of DATA BLOCKS (bytes 1A+) [Must end w/ Terminator Block]

- ---------------------------------------------------------------

HEADER:
-------
     byte #     Description
     ------     ------------------------------------------
     00-12      "Creative Voice File"
     13         1A (eof to abort printing of file)
     14-15      Offset of first datablock in .voc file (std 1A 00
                in Intel Notation)
     16-17      Version number (minor,major) (VOC-HDR puts 0A 01)
     18-19      2's Comp of Ver. # + 1234h (VOC-HDR puts 29 11)

- ---------------------------------------------------------------

DATA BLOCK:
-----------

   Data Block:  TYPE(1-byte), SIZE(3-bytes), INFO(0+ bytes)
   NOTE: Terminator Block is an exception -- it has only the TYPE byte.

      TYPE   Description     Size (3-byte int)   Info
      ----   -----------     -----------------   -----------------------
      00     Terminator      (NONE)              (NONE)
      01     Sound data      2+length of data    *
      02     Sound continue  length of data      Voice Data
      03     Silence         3                   **
      04     Marker          2                   Marker# (2 bytes)
      05     ASCII           length of string    null terminated string
      06     Repeat          2                   Count# (2 bytes)
      07     End repeat      0                   (NONE)
      08     Extended        4                   ***
      09     New Header      16                  see below


      *Sound Info Format:       **Silence Info Format:
       ---------------------      ----------------------------
       00   Sample Rate           00-01  Length of silence - 1
       01   Compression Type      02     Sample Rate
       02+  Voice Data

    ***Extended Info Format:
       ---------------------
       00-01  Time Constant: Mono: 65536 - (256000000/sample_rate)
                             Stereo: 65536 - (25600000/(2*sample_rate))
       02     Pack
       03     Mode: 0 = mono
                    1 = stereo


  Marker#           -- Driver keeps the most recent marker in a status byte
  Count#            -- Number of repetitions + 1
                         Count# may be 1 to FFFE for 0 - FFFD repetitions
                         or FFFF for endless repetitions
  Sample Rate       -- SR byte = 256-(1000000/sample_rate)
  Length of silence -- in units of sampling cycle
  Compression Type  -- of voice data
                         8-bits    = 0
                         4-bits    = 1
                         2.6-bits  = 2
                         2-bits    = 3
                         Multi DAC = 3+(# of channels) [interesting--
                                       this isn't in the developer's manual]

Detailed description of new data blocks (VOC files version 1.20 and above):

        (Source is fax from Barry Boone at Creative Labs, 405/742-6622)

BLOCK 8 - digitized sound attribute extension, must preceed block 1.
          Used to define stereo, 8 bit audio
        BYTE bBlockID;       // = 8
        BYTE nBlockLen[3];   // 3 byte length
        WORD wTimeConstant;  // time constant = same as block 1
        BYTE bPackMethod;    // same as in block 1
        BYTE bVoiceMode;     // 0-mono, 1-stereo

        Data is stored left, right

BLOCK 9 - data block that supersedes blocks 1 and 8.
          Used for stereo, 16 bit (and uLaw, aLaw).

        BYTE bBlockID;          // = 9
        BYTE nBlockLen[3];      // length 12 plus length of sound
        DWORD dwSamplesPerSec;  // samples per second, not time const.
        BYTE bBitsPerSample;    // e.g., 8 or 16
        BYTE bChannels;         // 1 for mono, 2 for stereo
        WORD wFormat;           // see below
        BYTE reserved[4];       // pad to make block w/o data
                                // have a size of 16 bytes

        Valid values of wFormat are:

                0x0000  8-bit unsigned PCM
                0x0001  Creative 8-bit to 4-bit ADPCM
                0x0002  Creative 8-bit to 3-bit ADPCM
                0x0003  Creative 8-bit to 2-bit ADPCM
                0x0004  16-bit signed PCM
                0x0006  CCITT a-Law
                0x0007  CCITT u-Law
                0x02000 Creative 16-bit to 4-bit ADPCM

        Data is stored left, right

        ANN:  Multi-byte quantities are in Intel byte order (Little Endian).

------------------------------------------------------------------------*/

#include "sox_i.h"
#include "g711.h"
#include <string.h>

/* Private data for VOC file */
typedef struct vocstuff {
    long           rest;        /* bytes remaining in current block */
    long           rate;        /* rate code (byte) of this chunk */
    int            silent;      /* sound or silence? */
    long           srate;       /* rate code (byte) of silence */
    sox_size_t     blockseek;   /* start of current output block */
    long           samples;     /* number of samples output */
    uint16_t       format;      /* VOC audio format */
    int            size;        /* word length of data */
    unsigned char  channels;    /* number of sound channels */
    long           total_size;  /* total size of all audio in file */
    int            extended;    /* Has an extended block been read? */
} *vs_t;

#define VOC_TERM        0
#define VOC_DATA        1
#define VOC_CONT        2
#define VOC_SILENCE     3
#define VOC_MARKER      4
#define VOC_TEXT        5
#define VOC_LOOP        6
#define VOC_LOOPEND     7
#define VOC_EXTENDED    8
#define VOC_DATA_16     9

/* ANN:  Format encoding types */
#define VOC_FMT_LIN8U          0   /* 8 bit unsigned linear PCM */
#define VOC_FMT_CRLADPCM4      1   /* Creative 8-bit to 4-bit ADPCM */
#define VOC_FMT_CRLADPCM3      2   /* Creative 8-bit to 3-bit ADPCM */
#define VOC_FMT_CRLADPCM2      3   /* Creative 8-bit to 2-bit ADPCM */
#define VOC_FMT_LIN16          4   /* 16-bit signed PCM */
#define VOC_FMT_ALAW           6   /* CCITT a-Law 8-bit PCM */
#define VOC_FMT_MU255          7   /* CCITT u-Law 8-bit PCM */
#define VOC_FMT_CRLADPCM4A 0x200   /* Creative 16-bit to 4-bit ADPCM */

/* Prototypes for internal functions */
static int getblock(sox_format_t *);
static void blockstart(sox_format_t *);

/* Conversion macros (from raw.c) */
#define SOX_ALAW_BYTE_TO_SAMPLE(d) ((sox_ssample_t)(sox_alaw2linear16(d)) << 16)
#define SOX_ULAW_BYTE_TO_SAMPLE(d) ((sox_ssample_t)(sox_ulaw2linear16(d)) << 16)

/* public VOC functions for SOX */
/*-----------------------------------------------------------------
 * sox_vocstartread() -- start reading a VOC file
 *-----------------------------------------------------------------*/
static int sox_vocstartread(sox_format_t * ft)
{
        int rtn = SOX_SUCCESS;
        char header[20];
        vs_t v = (vs_t) ft->priv;
        unsigned short sbseek;
        int rc;
        int ii;  /* for getting rid of lseek */
        unsigned char uc;

        if (sox_readbuf(ft, header, 20) != 20)
        {
                sox_fail_errno(ft,SOX_EHDR,"unexpected EOF in VOC header");
                return(SOX_EOF);
        }
        if (strncmp(header, "Creative Voice File\032", 19))
        {
                sox_fail_errno(ft,SOX_EHDR,"VOC file header incorrect");
                return(SOX_EOF);
        }

        /* read the offset to data, from start of file */
        /* after this read we have read 20 bytes of header + 2 */
        sox_readw(ft, &sbseek);

        /* ANN:  read to skip the header, instead of lseek */
        /* this should allow use with pipes.... */
        for (ii=22; ii<sbseek; ii++)
            sox_readb(ft, &uc);

        v->rate = -1;
        v->rest = 0;
        v->total_size = 0;  /* ANN added */
        v->extended = 0;
        v->format = VOC_FMT_LIN8U;

        /* read until we get the format information.... */
        rc = getblock(ft);
        if (rc)
            return rc;

        /* get rate of data */
        if (v->rate == -1)
        {
                sox_fail_errno(ft,SOX_EOF,"Input .voc file had no sound!");
                return(SOX_EOF);
        }

        /* setup word length of data */
        ft->signal.size = v->size;

        /* ANN:  Check VOC format and map to the proper libSoX format value */
        switch (v->format) {
        case VOC_FMT_LIN8U:      /*     0    8 bit unsigned linear PCM */
            ft->signal.encoding = SOX_ENCODING_UNSIGNED;
            break;
        case VOC_FMT_CRLADPCM4:  /*     1    Creative 8-bit to 4-bit ADPCM */
            sox_fail ("Unsupported VOC format CRLADPCM4 %d", v->format);
            rtn=SOX_EOF;
            break;
        case VOC_FMT_CRLADPCM3:  /*     2    Creative 8-bit to 3-bit ADPCM */
            sox_fail ("Unsupported VOC format CRLADPCM3 %d", v->format);
            rtn=SOX_EOF;
            break;
        case VOC_FMT_CRLADPCM2:  /*     3    Creative 8-bit to 2-bit ADPCM */
            sox_fail ("Unsupported VOC format CRLADPCM2 %d", v->format);
            rtn=SOX_EOF;
            break;
        case VOC_FMT_LIN16:      /*     4    16-bit signed PCM */
            ft->signal.encoding = SOX_ENCODING_SIGN2;
            break;
        case VOC_FMT_ALAW:       /*     6    CCITT a-Law 8-bit PCM */
            ft->signal.encoding = SOX_ENCODING_ALAW;
            break;
        case VOC_FMT_MU255:      /*     7    CCITT u-Law 8-bit PCM */
            ft->signal.encoding = SOX_ENCODING_ULAW;
            break;
        case VOC_FMT_CRLADPCM4A: /*0x200    Creative 16-bit to 4-bit ADPCM */
            sox_fail ("Unsupported VOC format CRLADPCM4A %d", v->format);
            rtn=SOX_EOF;
            break;
        default:
            sox_fail ("Unknown VOC format %d", v->format);
            rtn=SOX_EOF;
            break;
        }

        /* setup number of channels */
        if (ft->signal.channels == 0)
                ft->signal.channels = v->channels;

        return(SOX_SUCCESS);
}

/*-----------------------------------------------------------------
 * sox_vocread() -- read data from a VOC file
 * ANN:  Major changes here to support multi-part files and files
 *       that do not have audio in block 9's.
 *-----------------------------------------------------------------*/
static sox_size_t sox_vocread(sox_format_t * ft, sox_ssample_t *buf, sox_size_t len)
{
        vs_t v = (vs_t) ft->priv;
        sox_size_t done = 0;
        int rc = 0;
        int16_t sw;
        unsigned char  uc;

        /* handle getting another cont. buffer */
        if (v->rest == 0)
        {
                rc = getblock(ft);
                if (rc)
                    return 0;
        }

        /* if no more data, return 0, i.e., done */
        if (v->rest == 0)
                return 0;

        /* if silence, fill it in with 0's */
        if (v->silent) {
                /* Fill in silence */
                for(;v->rest && (done < len); v->rest--, done++)
                        *buf++ = 0x80000000;
        }
        /* else, not silence, read the block */
        else {
            /* read len samples of audio from the file */

            /* for(;v->rest && (done < len); v->rest--, done++) { */
            for(; (done < len); done++) {

                /* IF no more in this block, get another */
                if (v->rest == 0) {

                    /* DO until we have either EOF or a block with data */
                    while (v->rest == 0) {
                        rc = getblock(ft);
                        if (rc)
                            break;
                    }
                    /* ENDDO ... */

                    /* IF EOF, break out, no more data, next will return 0 */
                    if (rc)
                        break;
                }
                /* ENDIF no more data in block */

                /* Read the data in the file */
                switch(v->size) {
                case SOX_SIZE_BYTE:
                    if (sox_readb(ft, &uc) == SOX_EOF) {
                        sox_warn("VOC input: short file");
                        v->rest = 0;
                        return done;
                    }
                    /* IF uLaw,alaw, expand to linear, else convert??? */
                    /* ANN:  added uLaw and aLaw support */
                    if (v->format == VOC_FMT_MU255) {
                        *buf++ =  SOX_ULAW_BYTE_TO_SAMPLE(uc);
                    } else if (v->format == VOC_FMT_ALAW) {
                        *buf++ =  SOX_ALAW_BYTE_TO_SAMPLE(uc);
                    } else {
                        *buf++ = SOX_UNSIGNED_8BIT_TO_SAMPLE(uc,);
                    }
                    break;
                case SOX_SIZE_16BIT:
                    sox_readw(ft, (unsigned short *)&sw);
                    if (sox_eof(ft))
                        {
                            sox_warn("VOC input: short file");
                            v->rest = 0;
                            return done;
                        }
                    *buf++ = SOX_SIGNED_16BIT_TO_SAMPLE(sw,);
                    v->rest--; /* Processed 2 bytes so update */
                    break;
                }
                /* decrement count of processed bytes */
                v->rest--; /* Processed 2 bytes so update */
            }
        }
        v->total_size+=done;
        return done;
}

/* When saving samples in VOC format the following outline is followed:
 * If an 8-bit mono sample then use a VOC_DATA header.
 * If an 8-bit stereo sample then use a VOC_EXTENDED header followed
 * by a VOC_DATA header.
 * If a 16-bit sample (either stereo or mono) then save with a
 * VOC_DATA_16 header.
 *
 * ANN:  Not supported:  uLaw and aLaw output VOC files....
 *
 * This approach will cause the output to be an its most basic format
 * which will work with the oldest software (eg. an 8-bit mono sample
 * will be able to be played with a really old SB VOC player.)
 */
static int sox_vocstartwrite(sox_format_t * ft)
{
        vs_t v = (vs_t) ft->priv;

        if (! ft->seekable)
        {
                sox_fail_errno(ft,SOX_EOF,
                              "Output .voc file must be a file, not a pipe");
                return(SOX_EOF);
        }

        v->samples = 0;

        /* File format name and a ^Z (aborts printing under DOS) */
        sox_writes(ft, "Creative Voice File\032");
        sox_writew(ft, 26);                      /* size of header */
        sox_writew(ft, 0x10a);              /* major/minor version number */
        sox_writew(ft, 0x1129);          /* checksum of version number */

        if (ft->signal.size == SOX_SIZE_BYTE)
          ft->signal.encoding = SOX_ENCODING_UNSIGNED;
        else
          ft->signal.encoding = SOX_ENCODING_SIGN2;
        if (ft->signal.channels == 0)
                ft->signal.channels = 1;

        return(SOX_SUCCESS);
}

/*-----------------------------------------------------------------
 * sox_vocwrite() -- write a VOC file
 *-----------------------------------------------------------------*/
static sox_size_t sox_vocwrite(sox_format_t * ft, const sox_ssample_t *buf, sox_size_t len)
{
        vs_t v = (vs_t) ft->priv;
        unsigned char uc;
        int16_t sw;
        sox_size_t done = 0;

        if (v->samples == 0) {
          /* No silence packing yet. */
          v->silent = 0;
          blockstart(ft);
        }
        v->samples += len;
        while(done < len) {
          if (ft->signal.size == SOX_SIZE_BYTE) {
            uc = SOX_SAMPLE_TO_UNSIGNED_8BIT(*buf++, ft->clips);
            sox_writeb(ft, uc);
          } else {
            sw = (int) SOX_SAMPLE_TO_SIGNED_16BIT(*buf++, ft->clips);
            sox_writew(ft,sw);
          }
          done++;
        }
        return done;
}

/*-----------------------------------------------------------------
 * blockstop() -- stop an output block
 * End the current data or silence block.
 *-----------------------------------------------------------------*/
static void blockstop(sox_format_t * ft)
{
        vs_t v = (vs_t) ft->priv;
        sox_ssample_t datum;

        sox_writeb(ft, 0);                     /* End of file block code */
        sox_seeki(ft, (sox_ssize_t)v->blockseek, 0); /* seek back to block length */
        sox_seeki(ft, 1, 1);                    /* seek forward one */
        if (v->silent) {
                sox_writew(ft, v->samples);
        } else {
          if (ft->signal.size == SOX_SIZE_BYTE) {
            if (ft->signal.channels > 1) {
              sox_seeki(ft, 8, 1); /* forward 7 + 1 for new block header */
            }
          }
                v->samples += 2;                /* adjustment: SBDK pp. 3-5 */
                datum = (v->samples * ft->signal.size) & 0xff;
                sox_writeb(ft, (int)datum);       /* low byte of length */
                datum = ((v->samples * ft->signal.size) >> 8) & 0xff;
                sox_writeb(ft, (int)datum);  /* middle byte of length */
                datum = ((v->samples  * ft->signal.size)>> 16) & 0xff;
                sox_writeb(ft, (int)datum); /* high byte of length */
        }
}

/*-----------------------------------------------------------------
 * sox_vocstopwrite() -- stop writing a VOC file
 *-----------------------------------------------------------------*/
static int sox_vocstopwrite(sox_format_t * ft)
{
        blockstop(ft);
        return(SOX_SUCCESS);
}

/*-----------------------------------------------------------------
 * Voc-file handlers (static, private to this module)
 *-----------------------------------------------------------------*/

/*-----------------------------------------------------------------
 * getblock() -- Read next block header, save info,
 *               leave position at start of dat
 *-----------------------------------------------------------------*/
static int getblock(sox_format_t * ft)
{
        vs_t v = (vs_t) ft->priv;
        unsigned char uc, block;
        uint32_t sblen;
        uint16_t new_rate_16;
        uint32_t new_rate_32;
        uint32_t i;
        uint32_t trash;

        v->silent = 0;
        /* DO while we have no audio to read */
        while (v->rest == 0) {
                /* IF EOF, return EOF
                 * ANN:  was returning SUCCESS */
                if (sox_eof(ft))
                        return SOX_EOF;

                if (sox_readb(ft, &block) == SOX_EOF)
                        return SOX_EOF;

                /* IF TERM block (end of file), return EOF */
                if (block == VOC_TERM)
                        return SOX_EOF;

                /* IF EOF after reading block type, return EOF
                 * ANN:  was returning SUCCESS */
                if (sox_eof(ft))
                        return SOX_EOF;
                /*
                 * Size is an 24-bit value.  Currently there is no util
                 * func to read this so do it this cross-platform way
                 *
                 */
                sox_readb(ft, &uc);
                sblen = uc;
                sox_readb(ft, &uc);
                sblen |= ((uint32_t) uc) << 8;
                sox_readb(ft, &uc);
                sblen |= ((uint32_t) uc) << 16;

                /* Based on VOC block type, process the block */
                /* audio may be in one or multiple blocks */
                switch(block) {
                case VOC_DATA:
                        sox_readb(ft, &uc);
                        /* When DATA block preceeded by an EXTENDED     */
                        /* block, the DATA blocks rate value is invalid */
                        if (!v->extended) {
                          if (uc == 0)
                          {
                            sox_fail_errno(ft, SOX_EFMT, "Sample rate is zero?");
                            return(SOX_EOF);
                          }
                          if ((v->rate != -1) && (uc != v->rate))
                          {
                            sox_fail_errno(ft,SOX_EFMT,
                              "sample rate codes differ: %d != %d", v->rate, uc);
                            return(SOX_EOF);
                          }
                          v->rate = uc;
                          ft->signal.rate = 1000000.0/(256 - v->rate);
                          v->channels = 1;
                        }
                        sox_readb(ft, &uc);
                        if (uc != 0)
                        {
                          sox_fail_errno(ft,SOX_EFMT,
                            "only interpret 8-bit data!");
                          return(SOX_EOF);
                        }
                        v->extended = 0;
                        v->rest = sblen - 2;
                        v->size = SOX_SIZE_BYTE;
                        return (SOX_SUCCESS);
                case VOC_DATA_16:
                        sox_readdw(ft, &new_rate_32);
                        if (new_rate_32 == 0)
                        {
                            sox_fail_errno(ft,SOX_EFMT,
                              "Sample rate is zero?");
                            return(SOX_EOF);
                        }
                        if ((v->rate != -1) && ((long)new_rate_32 != v->rate))
                        {
                            sox_fail_errno(ft,SOX_EFMT,
                              "sample rate codes differ: %d != %d",
                                v->rate, new_rate_32);
                            return(SOX_EOF);
                        }
                        v->rate = new_rate_32;
                        ft->signal.rate = new_rate_32;
                        sox_readb(ft, &uc);
                        switch (uc)
                        {
                            case 8:     v->size = SOX_SIZE_BYTE; break;
                            case 16:    v->size = SOX_SIZE_16BIT; break;
                            default:
                                sox_fail_errno(ft,SOX_EFMT,
                                              "Don't understand size %d", uc);
                                return(SOX_EOF);
                        }
                        sox_readb(ft, &(v->channels));
                        sox_readw(ft, &(v->format));  /* ANN: added format */
                        sox_readb(ft, (unsigned char *)&trash); /* notused */
                        sox_readb(ft, (unsigned char *)&trash); /* notused */
                        sox_readb(ft, (unsigned char *)&trash); /* notused */
                        sox_readb(ft, (unsigned char *)&trash); /* notused */
                        v->rest = sblen - 12;
                        return (SOX_SUCCESS);
                case VOC_CONT:
                        v->rest = sblen;
                        return (SOX_SUCCESS);
                case VOC_SILENCE:
                        {
                        unsigned short period;

                        sox_readw(ft, &period);
                        sox_readb(ft, &uc);
                        if (uc == 0)
                        {
                          sox_fail_errno(ft,SOX_EFMT, "Silence sample rate is zero");
                          return(SOX_EOF);
                        }
                        /*
                         * Some silence-packed files have gratuitously
                         * different sample rate codes in silence.
                         * Adjust period.
                         */
                        if ((v->rate != -1) && (uc != v->rate))
                                period = (period * (256 - uc))/(256 - v->rate);
                        else
                                v->rate = uc;
                        v->rest = period;
                        v->silent = 1;
                        return (SOX_SUCCESS);
                        }
                case VOC_MARKER:
                        sox_readb(ft, &uc);
                        sox_readb(ft, &uc);
                        /* Falling! Falling! */
                case VOC_TEXT:
                        { 
                          /* TODO: Could add to comment in SF? */

                          /* Note, if this is sent to stderr, studio */
                          /* will not be able to read the VOC file */

                          uint32_t i = sblen;
                          char c/*, line_buf[80];
                          int len = 0*/;

                          while (i--)
                          {
                            sox_readb(ft, (unsigned char *)&c);
                            /* FIXME: this needs to be tested but I couldn't
                             * find a voc file with a VOC_TEXT chunk :(
                            if (c != '\0' && c != '\r')
                              line_buf[len++] = c;
                            if (len && (c == '\0' || c == '\r' ||
                                i == 0 || len == sizeof(line_buf) - 1))
                            {
                              sox_report("%s", line_buf);
                              line_buf[len] = '\0';
                              len = 0;
                            }
                            */
                          }
                        }
                        continue;       /* get next block */
                case VOC_LOOP:
                case VOC_LOOPEND:
                        sox_debug("skipping repeat loop");
                        for(i = 0; i < sblen; i++)
                            sox_readb(ft, (unsigned char *)&trash);
                        break;
                case VOC_EXTENDED:
                        /* An Extended block is followed by a data block */
                        /* Set this byte so we know to use the rate      */
                        /* value from the extended block and not the     */
                        /* data block.                                   */
                        v->extended = 1;
                        sox_readw(ft, &new_rate_16);
                        if (new_rate_16 == 0)
                        {
                           sox_fail_errno(ft,SOX_EFMT, "Sample rate is zero?");
                           return(SOX_EOF);
                        }
                        if ((v->rate != -1) && (new_rate_16 != v->rate))
                        {
                           sox_fail_errno(ft,SOX_EFMT,
                             "sample rate codes differ: %d != %d",
                                        v->rate, new_rate_16);
                           return(SOX_EOF);
                        }
                        v->rate = new_rate_16;
                        sox_readb(ft, &uc);
                        if (uc != 0)
                        {
                                sox_fail_errno(ft,SOX_EFMT,
                                  "only interpret 8-bit data!");
                                return(SOX_EOF);
                        }
                        sox_readb(ft, &uc);
                        if (uc)
                                ft->signal.channels = 2;  /* Stereo */
                        /* Needed number of channels before finishing
                           compute for rate */
                        ft->signal.rate = (256000000/(65536 - v->rate))/
                            ft->signal.channels;
                        /* An extended block must be followed by a data */
                        /* block to be valid so loop back to top so it  */
                        /* can be grabed.                               */
                        continue;
                default:
                        sox_debug("skipping unknown block code %d", block);
                        for(i = 0; i < sblen; i++)
                            sox_readb(ft, (unsigned char *)&trash);
                }
        }
        return SOX_SUCCESS;
}

/*-----------------------------------------------------------------
 * vlockstart() -- start an output block
 *-----------------------------------------------------------------*/
static void blockstart(sox_format_t * ft)
{
        vs_t v = (vs_t) ft->priv;

        v->blockseek = sox_tell(ft);
        if (v->silent) {
                sox_writeb(ft, VOC_SILENCE);     /* Silence block code */
                sox_writeb(ft, 0);               /* Period length */
                sox_writeb(ft, 0);               /* Period length */
                sox_writeb(ft, v->rate);         /* Rate code */
        } else {
          if (ft->signal.size == SOX_SIZE_BYTE) {
            /* 8-bit sample section.  By always setting the correct     */
            /* rate value in the DATA block (even when its preceeded    */
            /* by an EXTENDED block) old software can still play stereo */
            /* files in mono by just skipping over the EXTENDED block.  */
            /* Prehaps the rate should be doubled though to make up for */
            /* double amount of samples for a given time????            */
            if (ft->signal.channels > 1) {
              sox_writeb(ft, VOC_EXTENDED);      /* Voice Extended block code */
              sox_writeb(ft, 4);                /* block length = 4 */
              sox_writeb(ft, 0);                /* block length = 4 */
              sox_writeb(ft, 0);                /* block length = 4 */
                  v->rate = 65536 - (256000000.0/(2*(float)ft->signal.rate));
              sox_writew(ft,v->rate);    /* Rate code */
              sox_writeb(ft, 0);         /* File is not packed */
              sox_writeb(ft, 1);         /* samples are in stereo */
            }
            sox_writeb(ft, VOC_DATA);    /* Voice Data block code */
            sox_writeb(ft, 0);           /* block length (for now) */
            sox_writeb(ft, 0);           /* block length (for now) */
            sox_writeb(ft, 0);           /* block length (for now) */
            v->rate = 256 - (1000000.0/(float)ft->signal.rate);
            sox_writeb(ft, (int) v->rate);/* Rate code */
            sox_writeb(ft, 0);           /* 8-bit raw data */
        } else {
            sox_writeb(ft, VOC_DATA_16); /* Voice Data block code */
            sox_writeb(ft, 0);           /* block length (for now) */
            sox_writeb(ft, 0);           /* block length (for now) */
            sox_writeb(ft, 0);           /* block length (for now) */
            v->rate = ft->signal.rate;
            sox_writedw(ft, (unsigned)v->rate);    /* Rate code */
            sox_writeb(ft, 16);          /* Sample Size */
            sox_writeb(ft, (signed)ft->signal.channels);   /* Sample Size */
            sox_writew(ft, 0x0004);      /* Encoding */
            sox_writeb(ft, 0);           /* Unused */
            sox_writeb(ft, 0);           /* Unused */
            sox_writeb(ft, 0);           /* Unused */
            sox_writeb(ft, 0);           /* Unused */
          }
        }
}

/* Sound Blaster .VOC */
static const char *vocnames[] = {
  "voc",
  NULL
};

static sox_format_handler_t sox_voc_format = {
  vocnames,
  SOX_FILE_LIT_END,
  sox_vocstartread,
  sox_vocread,
  sox_format_nothing,
  sox_vocstartwrite,
  sox_vocwrite,
  sox_vocstopwrite,
  sox_format_nothing_seek
};

const sox_format_handler_t *sox_voc_format_fn(void);

const sox_format_handler_t *sox_voc_format_fn(void)
{
    return &sox_voc_format;
}
