/*
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 * libSoX skeleton effect file.
 */

#include "sox_i.h"
#include <string.h>

/* Time resolutin one millisecond */
#define TIMERES 1000

typedef struct
{
    /* options here */
    char *start_str;
    char *length_str;

    /* options converted to values */
    sox_size_t start;
    sox_size_t length;

    /* internal stuff */
    sox_size_t index;
    sox_size_t trimmed;
} * trim_t;

/*
 * Process options
 */
static int sox_trim_getopts(sox_effect_t effp, int n, char **argv) 
{
    trim_t trim = (trim_t) effp->priv;

    /* Do not know sample rate yet so hold off on completely parsing
     * time related strings.
     */
    switch (n) {
        case 2:
            trim->length_str = (char *)xmalloc(strlen(argv[1])+1);
            strcpy(trim->length_str,argv[1]);
            /* Do a dummy parse to see if it will fail */
            if (sox_parsesamples(0, trim->length_str, &trim->length, 't') == NULL)
            {
                sox_fail(effp->handler.usage);
                return(SOX_EOF);
            }
        case 1:
            trim->start_str = (char *)xmalloc(strlen(argv[0])+1);
            strcpy(trim->start_str,argv[0]);
            /* Do a dummy parse to see if it will fail */
            if (sox_parsesamples(0, trim->start_str, &trim->start, 't') == NULL)
            {
                sox_fail(effp->handler.usage);
                return(SOX_EOF);
            }
            break;
        default:
            sox_fail(effp->handler.usage);
            return SOX_EOF;
            break;

    }
    return (SOX_SUCCESS);
}

/*
 * Start processing
 */
static int sox_trim_start(sox_effect_t effp)
{
    trim_t trim = (trim_t) effp->priv;

    if (sox_parsesamples(effp->ininfo.rate, trim->start_str,
                        &trim->start, 't') == NULL)
    {
        sox_fail(effp->handler.usage);
        return(SOX_EOF);
    }
    /* Account for # of channels */
    trim->start *= effp->ininfo.channels;

    if (trim->length_str)
    {
        if (sox_parsesamples(effp->ininfo.rate, trim->length_str,
                    &trim->length, 't') == NULL)
        {
            sox_fail(effp->handler.usage);
            return(SOX_EOF);
        }
    }
    else
        trim->length = 0;

    /* Account for # of channels */
    trim->length *= effp->ininfo.channels;

    trim->index = 0;
    trim->trimmed = 0;

    return (SOX_SUCCESS);
}

/*
 * Read up to len samples from file.
 * Convert to signed longs.
 * Place in buf[].
 * Return number of samples read.
 */
static int sox_trim_flow(sox_effect_t effp, const sox_ssample_t *ibuf, sox_ssample_t *obuf, 
                 sox_size_t *isamp, sox_size_t *osamp)
{
    int result = SOX_SUCCESS;
    int start_trim = 0;
    int offset = 0;
    int done;

    trim_t trim = (trim_t) effp->priv;

    /* Compute the most samples we can process this time */
    done = ((*isamp < *osamp) ? *isamp : *osamp);

    /* Quick check to see if we are trimming off the back side yet.
     * If so then we can skip trimming from the front side.
     */
    if (!trim->trimmed) {
        if ((trim->index+done) <= trim->start) {
            /* If we haven't read more then "start" samples, return that
             * we've read all this buffer without outputing anything
             */
            *osamp = 0;
            *isamp = done;
            trim->index += done;
            return (SOX_SUCCESS);
        } else {
            start_trim = 1;
            /* We've read at least "start" samples.  Now find
             * out where our target data begins and subtract that
             * from the total to be copied this round.
             */
            offset = trim->start - trim->index;
            done -= offset;
        }
    } /* !trimmed */

    if (trim->trimmed || start_trim) {
        if (trim->length && ((trim->trimmed+done) >= trim->length)) {
            /* Since we know the end is in this block, we set done
             * to the desired length less the amount already read.
             */
            done = trim->length - trim->trimmed;
            result = SOX_EOF;
        }

        trim->trimmed += done;
    }
    memcpy(obuf, ibuf+offset, done * sizeof(*obuf));
    *osamp = done;
    *isamp = offset + done;
    trim->index += done;

    return result;
}

static int kill(sox_effect_t effp)
{
    trim_t trim = (trim_t) effp->priv;

    free(trim->start_str);
    free(trim->length_str);

    return (SOX_SUCCESS);
}

sox_size_t sox_trim_get_start(sox_effect_t effp)          
{        
    trim_t trim = (trim_t)effp->priv;    
    return trim->start;          
}        

void sox_trim_clear_start(sox_effect_t effp)     
{        
    trim_t trim = (trim_t)effp->priv;    
    trim->start = 0;     
}

static sox_effect_handler_t sox_trim_effect = {
  "trim",
  "Usage: trim start [length]",
  SOX_EFF_MCHAN|SOX_EFF_LENGTH,
  sox_trim_getopts,
  sox_trim_start,
  sox_trim_flow,
  NULL,
  NULL,
  kill
};

const sox_effect_handler_t *sox_trim_effect_fn(void)
{
    return &sox_trim_effect;
}
