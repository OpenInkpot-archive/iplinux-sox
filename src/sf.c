/*
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained.
 * Lance Norskog And Sundry Contributors are not responsible for
 * the consequences of using this software.
 */

/*
 * Sound Tools IRCAM SoundFile format handler.
 *
 * Derived from: Sound Tools skeleton handler file.
 */

#include "sox_i.h"
#include "sfircam.h"

#include <string.h>
#include <stdlib.h>

/* Private data for SF file */
typedef struct sfstuff {
        struct sfinfo info;
        /* needed for seek */
        sox_size_t dataStart;
} *sf_t;

/*
 * Read the codes from the sound file, allocate space for the comment and
 * assign its pointer to the comment field in ft.
 */
static void readcodes(ft_t ft, SFHEADER *sfhead)
{
        char *commentbuf = NULL, *sfcharp, *newline;
        short bsize, finished = 0;
        SFCODE *sfcodep;

        sfcodep = (SFCODE *) &sfcodes(sfhead);
        do {
                sfcharp = (char *) sfcodep + sizeof(SFCODE);
                if (ft->signal.reverse_bytes) {
                        sfcodep->bsize = sox_swapdw(sfcodep->bsize);
                        sfcodep->code = sox_swapdw(sfcodep->code);
                }
                bsize = sfcodep->bsize - sizeof(SFCODE);
                switch(sfcodep->code) {
                case SF_END:
                        finished = 1;
                        break;
                case SF_COMMENT:
                        commentbuf = (char *) xmalloc(bsize + 1);
                        memcpy(commentbuf, sfcharp, bsize);
                        sox_report("IRCAM comment: %s", sfcharp);
                        commentbuf[bsize] = '\0';
                        if((newline = strchr(commentbuf, '\n')) != NULL)
                                *newline = '\0';
                        break;
                }
                sfcodep = (SFCODE *) (sfcharp + bsize);
        } while(!finished);
        if(commentbuf != NULL)  /* handles out of memory condition as well */
                ft->comment = commentbuf;
}

static int sox_sfseek(ft_t ft, sox_size_t offset)
{
    sox_size_t new_offset, channel_block, alignment;

    sf_t sf = (sf_t ) ft->priv;
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
    new_offset += sf->dataStart;

    return sox_seeki(ft, new_offset, SEEK_SET);
}

/*
 * Do anything required before you start reading samples.
 * Read file header.
 *      Find out sampling rate,
 *      size and encoding of samples,
 *      mono/stereo/quad.
 */
static int sox_sfstartread(ft_t ft)
{
        sf_t sf = (sf_t) ft->priv;
        SFHEADER sfhead;
        int rc;
        int samplesize = 0;

        if (sox_readbuf(ft, &sfhead, 1, sizeof(sfhead)) != sizeof(sfhead))
        {
                sox_fail("unexpected EOF in SF header");
                return(SOX_EOF);
        }
        memcpy(&sf->info, &sfhead.sfinfo, sizeof(struct sfinfo));
        if (ft->signal.reverse_bytes) {
                sf->info.sf_srate = sox_swapf(sf->info.sf_srate);
                sf->info.sf_packmode = sox_swapdw(sf->info.sf_packmode);
                sf->info.sf_chans = sox_swapdw(sf->info.sf_chans);
        }
        if ((sfmagic1(&sfhead) != SF_MAGIC1) ||
            (sfmagic2(&sfhead) != SF_MAGIC2))
                sox_fail(
"SF %s file: can't read, it is byte-swapped or it is not an IRCAM SoundFile",
                        ft->filename);


        /*
         * If your format specifies or your file header contains
         * any of the following information.
         */
        ft->signal.rate = sf->info.sf_srate;
        switch(sf->info.sf_packmode) {
                case SF_SHORT:
                        ft->signal.size = SOX_SIZE_16BIT;
                        ft->signal.encoding = SOX_ENCODING_SIGN2;
                        samplesize = ft->signal.size;
                        break;
                case SF_FLOAT:
                        ft->signal.size = SOX_SIZE_32BIT;
                        ft->signal.encoding = SOX_ENCODING_FLOAT;
                        samplesize = sizeof(float);
                        break;
                default:
                        sox_fail("Soundfile input: unknown format 0x%x",
                                sf->info.sf_packmode);
                        return(SOX_EOF);
        }
        ft->signal.channels = (int) sf->info.sf_chans;

        if (ft->signal.channels == 0)
            ft->signal.channels = 1;

        /* Read codes and print as comments. */
        readcodes(ft, &sfhead);

        /* Needed for rawread() */
        rc = sox_rawstartread(ft);

/* Need length for seeking */
        if(ft->seekable){
                ft->length = sox_filelength(ft)/samplesize;
                sf->dataStart = sox_tell(ft);
        } else {
                ft->length = 0;
        }

        return(rc);
}

static int sox_sfstartwrite(ft_t ft)
{
        sf_t sf = (sf_t) ft->priv;
        SFHEADER sfhead;
        SFCODE *sfcodep;
        char *sfcharp;
        int rc;

        /* Needed for rawwrite() */
        rc = sox_rawstartwrite(ft);
        if (rc)
            return rc;

        sf->info.magic_union._magic_bytes.sf_magic1 = SF_MAGIC1;
        sf->info.magic_union._magic_bytes.sf_magic2 = SF_MAGIC2;
        sf->info.magic_union._magic_bytes.sf_param = 0;

        /* This file handler can handle both big and little endian data */
        if (SOX_IS_LITTLEENDIAN)
            sf->info.magic_union._magic_bytes.sf_machine = SF_VAX;
        else
            sf->info.magic_union._magic_bytes.sf_machine = SF_SUN;

        sf->info.sf_srate = ft->signal.rate;
        if (ft->signal.size == SOX_SIZE_32BIT &&
            ft->signal.encoding == SOX_ENCODING_FLOAT) {
                sf->info.sf_packmode = SF_FLOAT;
        } else {
                sf->info.sf_packmode = SF_SHORT;
                /* Default to signed words */
                ft->signal.size = SOX_SIZE_16BIT;
                ft->signal.encoding = SOX_ENCODING_SIGN2;
        }

        sf->info.sf_chans = ft->signal.channels;

        /* Clean out structure so unused areas will remain constain  */
        /* between different coverts and not rely on memory contents */
        memset (&sfhead, 0, sizeof(SFHEADER));
        memcpy(&sfhead.sfinfo, &sf->info, sizeof(struct sfinfo));
        sfcodep = (SFCODE *) &sfcodes(&sfhead);
        sfcodep->code = SF_COMMENT;
        sfcodep->bsize = strlen(ft->comment) + sizeof(SFCODE);
        while (sfcodep->bsize % 4)
                sfcodep->bsize++;
        sfcharp = (char *) sfcodep;
        strcpy(sfcharp + sizeof(SFCODE), ft->comment);
        sfcodep = (SFCODE *) (sfcharp + sfcodep->bsize);
        sfcodep->code = SF_END;
        sfcodep->bsize = sizeof(SFCODE);
        sfcharp = (char *) sfcodep + sizeof(SFCODE);
        while(sfcharp < (char *) &sfhead + SIZEOF_HEADER)
                *sfcharp++ = '\0';
        sox_writebuf(ft, &sfhead, 1, sizeof(SFHEADER));

        return(SOX_SUCCESS);
}

/* Read and write are supplied by raw.c */
/* IRCAM Sound File */
static const char *sfnames[] = {
  "sf",
  "ircam",
  NULL
};

static sox_format_t sox_sf_format = {
  sfnames,
  NULL,
  SOX_FILE_SEEK,
  sox_sfstartread,
  sox_rawread,
  sox_rawstopread,
  sox_sfstartwrite,
  sox_rawwrite,
  sox_rawstopwrite,
  sox_sfseek
};

const sox_format_t *sox_sf_format_fn(void)
{
    return &sox_sf_format;
}
