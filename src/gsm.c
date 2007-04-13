/*
 * Copyright 1991, 1992, 1993 Guido van Rossum And Sundry Contributors.
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Guido van Rossum And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 * GSM 06.10 courtesy Communications and Operating Systems Research Group,
 * Technische Universitaet Berlin
 *
 * More information on this format can be obtained from
 * http://www.cs.tu-berlin.de/~jutta/toasox.html
 *
 * Source is available from ftp://ftp.cs.tu-berlin.de/pub/local/kbs/tubmik/gsm
 *
 * Written 26 Jan 1995 by Andrew Pam
 * Portions Copyright (c) 1995 Serious Cybernetics
 *
 * July 19, 1998 - Chris Bagwell (cbagwell@sprynet.com)
 *   Added GSM support to SOX from patches floating around with the help
 *   of Dima Barsky (ess2db@ee.surrey.ac.uk).
 *
 * Nov. 26, 1999 - Stan Brooks (stabro@megsinet.com)
 *   Rewritten to support multiple channels
 */

#include "sox_i.h"

#ifdef EXTERNAL_GSM
#include <gsm/gsm.h>
#else
#include "libgsm/gsm.h"
#endif

#include <errno.h>

#define MAXCHANS 16

/* sizeof(gsm_frame) */
#define FRAMESIZE 33
/* samples per gsm_frame */
#define BLOCKSIZE 160

/* Private data */
struct gsmpriv {
        unsigned        channels;
        gsm_signal      *samples;
        gsm_signal      *samplePtr;
        gsm_signal      *sampleTop;
        gsm_byte *frames;
        gsm             handle[MAXCHANS];
};

static int gsmstart_rw(ft_t ft, int w) 
{
        struct gsmpriv *p = (struct gsmpriv *) ft->priv;
        unsigned ch;
        
        ft->signal.encoding = SOX_ENCODING_GSM;
        ft->signal.size = SOX_SIZE_BYTE;
        if (!ft->signal.rate)
                ft->signal.rate = 8000;

        if (ft->signal.channels == 0)
            ft->signal.channels = 1;

        p->channels = ft->signal.channels;
        if (p->channels > MAXCHANS || p->channels <= 0)
        {
                sox_fail_errno(ft,SOX_EFMT,"gsm: channels(%d) must be in 1-16", ft->signal.channels);
                return(SOX_EOF);
        }

        for (ch=0; ch<p->channels; ch++) {
                p->handle[ch] = gsm_create();
                if (!p->handle[ch])
                {
                        sox_fail_errno(ft,errno,"unable to create GSM stream");
                        return (SOX_EOF);
                }
        }
        p->frames = (gsm_byte*) xmalloc(p->channels*FRAMESIZE);
        p->samples = (gsm_signal*) xmalloc(BLOCKSIZE * (p->channels+1) * sizeof(gsm_signal));
        p->sampleTop = p->samples + BLOCKSIZE*p->channels;
        p->samplePtr = (w)? p->samples : p->sampleTop;
        return (SOX_SUCCESS);
}

static int sox_gsmstartread(ft_t ft) 
{
        return gsmstart_rw(ft,0);
}

static int sox_gsmstartwrite(ft_t ft)
{
        return gsmstart_rw(ft,1);
}

/*
 * Read up to len samples from file.
 * Convert to signed longs.
 * Place in buf[].
 * Return number of samples read.
 */

static sox_size_t sox_gsmread(ft_t ft, sox_ssample_t *buf, sox_size_t samp)
{
        size_t done = 0, r;
        int ch, chans;
        gsm_signal *gbuff;
        struct gsmpriv *p = (struct gsmpriv *) ft->priv;

        chans = p->channels;

        while (done < samp)
        {
                while (p->samplePtr < p->sampleTop && done < samp)
                        buf[done++] = 
                            SOX_SIGNED_WORD_TO_SAMPLE(*(p->samplePtr)++,);

                if (done>=samp) break;

                r = sox_readbuf(ft, p->frames, p->channels * FRAMESIZE);
                if (r != p->channels * FRAMESIZE)
                  break;

                p->samplePtr = p->samples;
                for (ch=0; ch<chans; ch++) {
                        int i;
                        gsm_signal *gsp;

                        gbuff = p->sampleTop;
                        if (gsm_decode(p->handle[ch], p->frames + ch*FRAMESIZE, gbuff) < 0)
                        {
                                sox_fail_errno(ft,errno,"error during GSM decode");
                                return (0);
                        }
                        
                        gsp = p->samples + ch;
                        for (i=0; i<BLOCKSIZE; i++) {
                                *gsp = *gbuff++;
                                gsp += chans;
                        }
                }
        }

        return done;
}

static int gsmflush(ft_t ft)
{
        int r, ch, chans;
        gsm_signal *gbuff;
        struct gsmpriv *p = (struct gsmpriv *) ft->priv;

        chans = p->channels;

        /* zero-fill samples as needed */
        while (p->samplePtr < p->sampleTop)
                *(p->samplePtr)++ = 0;
        
        gbuff = p->sampleTop;
        for (ch=0; ch<chans; ch++) {
                int i;
                gsm_signal *gsp;

                gsp = p->samples + ch;
                for (i=0; i<BLOCKSIZE; i++) {
                        gbuff[i] = *gsp;
                        gsp += chans;
                }
                gsm_encode(p->handle[ch], gbuff, p->frames);
                r = sox_writebuf(ft, p->frames, FRAMESIZE);
                if (r != FRAMESIZE)
                {
                        sox_fail_errno(ft,errno,"write error");
                        return(SOX_EOF);
                }
        }
        p->samplePtr = p->samples;

        return (SOX_SUCCESS);
}

static sox_size_t sox_gsmwrite(ft_t ft, const sox_ssample_t *buf, sox_size_t samp)
{
        size_t done = 0;
        struct gsmpriv *p = (struct gsmpriv *) ft->priv;

        while (done < samp)
        {
                while ((p->samplePtr < p->sampleTop) && (done < samp))
                        *(p->samplePtr)++ = 
                            SOX_SAMPLE_TO_SIGNED_WORD(buf[done++], ft->clips);

                if (p->samplePtr == p->sampleTop)
                {
                        if(gsmflush(ft))
                        {
                            return 0;
                        }
                }
        }

        return done;
}

static int sox_gsmstopread(ft_t ft)
{
        struct gsmpriv *p = (struct gsmpriv *) ft->priv;
        unsigned ch;

        for (ch=0; ch<p->channels; ch++)
                gsm_destroy(p->handle[ch]);

        free(p->samples);
        free(p->frames);
        return (SOX_SUCCESS);
}

static int sox_gsmstopwrite(ft_t ft)
{
        int rc;
        struct gsmpriv *p = (struct gsmpriv *) ft->priv;

        if (p->samplePtr > p->samples)
        {
                rc = gsmflush(ft);
                if (rc)
                    return rc;
        }

        return sox_gsmstopread(ft); /* destroy handles and free buffers */
}

/* GSM 06.10 */
static const char *gsmnames[] = {
  "gsm",
  NULL
};

static sox_format_t sox_gsm_format = {
  gsmnames,
  0,
  sox_gsmstartread,
  sox_gsmread,
  sox_gsmstopread,
  sox_gsmstartwrite,
  sox_gsmwrite,
  sox_gsmstopwrite,
  sox_format_nothing_seek
};

const sox_format_t *sox_gsm_format_fn(void)
{
    return &sox_gsm_format;
}
