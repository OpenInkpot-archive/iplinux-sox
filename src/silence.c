/* Silence effect for SoX
 * by Heikki Leinonen (heilei@iki.fi) 25.03.2001
 * Major Modifications by Chris Bagwell 06.08.2001
 *
 * This effect can delete samples from the start of a sound file
 * until it sees a specified count of samples exceed a given threshold 
 * (any of the channels).
 * This effect can also delete samples from the end of a sound file
 * when it sees a specified count of samples below a given threshold
 * (all channels).
 * Theshold's can be given as either a percentage or in decibels.
 */


#include <string.h>
#include <math.h>
#include "st_i.h"

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef min
#define min(s1,s2) ((s1)<(s2)?(s1):(s2))
#endif

/* Private data for silence effect. */

#define SILENCE_TRIM        0
#define SILENCE_TRIM_FLUSH  1
#define SILENCE_COPY        2
#define SILENCE_COPY_FLUSH  3
#define SILENCE_STOP        4

typedef struct silencestuff
{
    char	start;
    int		start_periods;
    char	*start_duration_str;
    ULONG	start_duration;
    double	start_threshold;
    char	start_unit; /* "d" for decibels or "%" for percent. */

    LONG	*start_holdoff;
    ULONG	start_holdoff_offset;
    ULONG	start_holdoff_end;
    int		start_found_periods;

    char	stop;
    int		stop_periods;
    char	*stop_duration_str;
    ULONG	stop_duration;
    double	stop_threshold;
    char	stop_unit;

    LONG	*stop_holdoff;
    ULONG	stop_holdoff_offset;
    ULONG	stop_holdoff_end;
    int		stop_found_periods;

    /* State Machine */
    char	mode;
} *silence_t;

#define SILENCE_USAGE "Usage: silence above_periods [ duration thershold[d | %% | s] ] [ below_periods duration threshold[ d | %% | s ]]"

int st_silence_getopts(eff_t effp, int n, char **argv)
{
    silence_t	silence = (silence_t) effp->priv;
    int parse_count;

    if (n < 1)
    {
	st_fail(SILENCE_USAGE);
	return (ST_EOF);
    }

    /* Parse data related to trimming front side */
    silence->start = FALSE;
    if (sscanf(argv[0], "%d", &silence->start_periods) != 1)
    {
	st_fail(SILENCE_USAGE);
	return(ST_EOF);
    }
    if (silence->start_periods < 0)
    {
	st_fail("Periods must not be negative");
	return(ST_EOF);
    }
    argv++;
    n--;

    if (silence->start_periods > 0)
    {
	silence->start = TRUE;
	if (n < 2)
	{
	    st_fail(SILENCE_USAGE);
	    return ST_EOF;
	}

	/* We do not know the sample rate so we can not fully
	 * parse the duration info yet.  So save argument off
	 * for future processing.
	 */
	silence->start_duration_str = malloc(strlen(argv[0])+1);
	if (!silence->start_duration_str)
	{
	    st_fail("Could not allocate memory");
	    return(ST_EOF);
	}
	strcpy(silence->start_duration_str,argv[0]);
	/* Perform a fake parse to do error checking */
	if (st_parsesamples(0,silence->start_duration_str,
		    &silence->start_duration,'s') !=
		ST_SUCCESS)
	{
	    st_fail(SILENCE_USAGE);
	    return(ST_EOF);
	}

	parse_count = sscanf(argv[1], "%lf%c", &silence->start_threshold, 
		&silence->start_unit);
	if (parse_count < 1)
	{
	    st_fail(SILENCE_USAGE);
	    return ST_EOF;
	}
	else if (parse_count < 2)
	    silence->start_unit = 's';

	argv++; argv++;
	n--; n--;
    }
    else
    {
	st_fail(SILENCE_USAGE);
	return ST_EOF;
    }

    silence->stop = FALSE;
    /* Parse data needed for trimming of backside */
    if (n > 0)
    {
	if (n < 3)
	{
	    st_fail(SILENCE_USAGE);
	    return ST_EOF;
	}
	if (sscanf(argv[0], "%d", &silence->stop_periods) != 1)
	{
	    st_fail(SILENCE_USAGE);
	    return ST_EOF;
	}
	if (silence->stop_periods < 0)
	{
	    st_fail("Periods must not be greater then zero");
	    return(ST_EOF);
	}
	silence->stop = TRUE;
	argv++;
	n--;

	/* We do not know the sample rate so we can not fully
	 * parse the duration info yet.  So save argument off
	 * for future processing.
	 */
	silence->stop_duration_str = malloc(strlen(argv[0])+1);
	if (!silence->stop_duration_str)
	{
	    st_fail("Could not allocate memory");
	    return(ST_EOF);
	}
	strcpy(silence->stop_duration_str,argv[0]);
	/* Perform a fake parse to do error checking */
	if (st_parsesamples(0,silence->stop_duration_str,
		    &silence->stop_duration,'s') !=
		ST_SUCCESS)
	{
	    st_fail(SILENCE_USAGE);
	    return(ST_EOF);
	}

	parse_count = sscanf(argv[1], "%lf%c", &silence->stop_threshold, 
		             &silence->stop_unit);
	if (parse_count < 1)
	{
	    st_fail(SILENCE_USAGE);
	    return ST_EOF;
	}
	else if (parse_count < 2)
	    silence->stop_unit = 's';

	argv++; argv++;
	n--; n--;
    }

    /* Error checking */
    if (silence->start)
    {
	if ((silence->start_unit != '%') && (silence->start_unit != 'd') &&
		(silence->start_unit != 's'))
	{
	    st_fail("Invalid unit specified");
	    st_fail(SILENCE_USAGE);
	    return(ST_EOF);
	}
	if ((silence->start_unit == '%') && ((silence->start_threshold < 0.0)
	    || (silence->start_threshold > 100.0)))
	{
	    st_fail("silence threshold should be between 0.0 and 100.0 %%");
	    return (ST_EOF);
	}
	if ((silence->start_unit == 'd') && (silence->start_threshold >= 0.0))
	{
	    st_fail("silence threshold should be less than 0.0 dB");
	    return(ST_EOF);
	}
    }

    if (silence->stop)
    {
	if ((silence->stop_unit != '%') && (silence->stop_unit != 'd') &&
		(silence->stop_unit != 's'))
	{
	    st_fail("Invalid unit specified");
	    return(ST_EOF);
	}
	if ((silence->stop_unit == '%') && ((silence->stop_threshold < 0.0) || 
		    (silence->stop_threshold > 100.0)))
	{
	    st_fail("silence threshold should be between 0.0 and 100.0 %%");
	    return (ST_EOF);
	}
	if ((silence->stop_unit == 'd') && (silence->stop_threshold >= 0.0))
	{
	    st_fail("silence threshold should be less than 0.0 dB");
	    return(ST_EOF);
	}
    }
    return(ST_SUCCESS);
}

int st_silence_start(eff_t effp)
{
	silence_t	silence = (silence_t) effp->priv;

	/* Now that we now sample rate, reparse duration. */
	if (silence->start)
	{
	    if (st_parsesamples(effp->ininfo.rate, silence->start_duration_str,
			&silence->start_duration,'s') !=
		    ST_SUCCESS)
	    {
		st_fail(SILENCE_USAGE);
		return(ST_EOF);
	    }
	}
	if (silence->stop)
	{
	    if (st_parsesamples(effp->ininfo.rate,silence->stop_duration_str,
			&silence->stop_duration,'s') !=
		    ST_SUCCESS)
	    {
		st_fail(SILENCE_USAGE);
		return(ST_EOF);
	    }
	}

	if (silence->start)
    	    silence->mode = SILENCE_TRIM;
	else
	    silence->mode = SILENCE_COPY;

	silence->start_holdoff = malloc(sizeof(LONG)*silence->start_duration);
	silence->start_holdoff_offset = 0;
	silence->start_holdoff_end = 0;
	silence->start_found_periods = 0;

	silence->stop_holdoff = malloc(sizeof(LONG)*silence->stop_duration);
	silence->stop_holdoff_offset = 0;
	silence->stop_holdoff_end = 0;
	silence->stop_found_periods = 0;

	return(ST_SUCCESS);
}

int aboveThreshold(st_sample_t value, double threshold, char unit)
{
    double ratio;
    int rc = 0;

    ratio = (double)labs(value) / (double)MAXLONG;

    if (unit == 's')
    {
	rc = (labs(value) >= threshold);
    }
    else
    {
	if (unit == '%')
	    ratio *= 100.0;
	else if (unit == 'd')
	    ratio = log10(ratio) * 20.0;
    	rc = (ratio >= threshold);
    }

    return rc;
}

/* Process signed long samples from ibuf to obuf. */
/* Return number of samples processed in isamp and osamp. */
int st_silence_flow(eff_t effp, st_sample_t *ibuf, st_sample_t *obuf, 
                    st_size_t *isamp, st_size_t *osamp)
{
    silence_t silence = (silence_t) effp->priv;
    int	threshold, i, j;
    ULONG nrOfTicks, nrOfInSamplesRead, nrOfOutSamplesWritten;

    nrOfInSamplesRead = 0;
    nrOfOutSamplesWritten = 0;

    switch (silence->mode)
    {
	case SILENCE_TRIM:
	    /* Reads and discards all input data until it detects a
	     * sample that is above the specified threshold.  Turns on
	     * copy mode when detected.
	     */
	    nrOfTicks = min((*isamp), (*osamp)) / effp->ininfo.channels;
	    for(i = 0; i < nrOfTicks; i++)
	    {
		threshold = 0;
		for (j = 0; j < effp->ininfo.channels; j++)
		{
		    threshold |= aboveThreshold(ibuf[j], 
			    silence->start_threshold, 
			    silence->start_unit);
		}
		if (threshold)
		{
		    /* Add to holdoff buffer */
		    for (j = 0; j < effp->ininfo.channels; j++)
		    {
			silence->start_holdoff[
			    silence->start_holdoff_end++] = *ibuf++;
			nrOfInSamplesRead++;
		    }

		    if (silence->start_holdoff_end >=
			    silence->start_duration)
		    {
			if (++silence->start_found_periods >=
				silence->start_periods)
			{
			    silence->mode = SILENCE_TRIM_FLUSH;
			    goto silence_trim_flush;
			}
			/* Trash holdoff buffer since its not
			 * needed.  Start looking again.
			 */
			silence->start_holdoff_offset = 0;
			silence->start_holdoff_end = 0;
		    }
		}
		else /* !above Threshold */
		{
		    silence->start_holdoff_end = 0;
		    ibuf += effp->ininfo.channels;
    		    nrOfInSamplesRead += effp->ininfo.channels;
		}
	    } /* for nrOfTicks */
	    break;

	case SILENCE_TRIM_FLUSH:
silence_trim_flush:
	    nrOfTicks = min((silence->start_holdoff_end -
			        silence->start_holdoff_offset), 
	                    (*osamp-nrOfOutSamplesWritten)) / 
		            effp->ininfo.channels;
	    for(i = 0; i < nrOfTicks; i++)
	    {
		*obuf++ = silence->start_holdoff[silence->start_holdoff_offset++];
		nrOfOutSamplesWritten++;
	    }

	    if (silence->start_holdoff_offset == silence->start_holdoff_end)
	    {
		silence->start_holdoff_offset = 0;
		silence->start_holdoff_end = 0;
		silence->mode = SILENCE_COPY;
		goto silence_copy;
	    }
	    break;

	case SILENCE_COPY:
	    /* Attempts to copy samples into output buffer.  If not
	     * looking for silence to terminate copy then blindly
	     * copy data into output buffer.
	     *
	     * If looking for silence, then see if input sample is above
	     * threshold.  If found then flush out hold off buffer
	     * and copy over to output buffer.  Tell user about
	     * input and output processing.
	     *
	     * If not above threshold then store in hold off buffer
	     * and do not write to output buffer.  Tell user input
	     * was processed.
	     *
	     * If hold off buffer is full then stop copying data and
	     * discard data in hold off buffer.
	     */
silence_copy:
	    nrOfTicks = min((*isamp-nrOfInSamplesRead), 
	                    (*osamp-nrOfOutSamplesWritten)) / 
		            effp->ininfo.channels;
	    if (silence->stop)
	    {
	        for(i = 0; i < nrOfTicks; i++)
	        {
		    threshold = 1;
		    for (j = 0; j < effp->ininfo.channels; j++)
		    {
		        threshold &= aboveThreshold(ibuf[j], 
				                    silence->stop_threshold, 
			                            silence->stop_unit);
		    }
		    /* If above threshold, check to see if we where holding
		     * off previously.  If so then flush this buffer.
		     * We haven't incremented any pionters yet so nothing
		     * is lost.
		     */
		    if (threshold && silence->stop_holdoff_end)
		    {
			silence->mode = SILENCE_COPY_FLUSH;
			goto silence_copy_flush;
		    }
		    else if (threshold)
		    {
			/* Not holding off so copy into output buffer */
			memcpy(obuf,ibuf,sizeof(LONG)*effp->ininfo.channels);
			nrOfInSamplesRead += effp->ininfo.channels;
			nrOfOutSamplesWritten += effp->ininfo.channels;
			ibuf += effp->ininfo.channels;
		    }
		    else if (!threshold)
		    {
			/* Add to holdoff buffer */
		        for (j = 0; j < effp->ininfo.channels; j++)
		        {
			    silence->stop_holdoff[
				    silence->stop_holdoff_end++] = *ibuf++;
			    nrOfInSamplesRead++;
		        }
			/* Check if holdoff buffer is greater than duration 
			 */
			if (silence->stop_holdoff_end >= 
				silence->stop_duration)
			{
			    /* Increment found counter and see if this
			     * is the last period.  If so then exit.
			     */
			    if (++silence->stop_found_periods >= 
				    silence->stop_periods)
			    {
				silence->mode = SILENCE_STOP;
				silence->stop_holdoff_offset = 0;
				silence->stop_holdoff_end = 0;
				*isamp = nrOfInSamplesRead;
				*osamp = nrOfOutSamplesWritten;
				/* Return ST_EOF since no more processing */
				return (ST_EOF);
			    }
			    else
			    {
				/* Flush this buffer and start 
				 * looking again.
				 */
				silence->mode = SILENCE_COPY_FLUSH;
				goto silence_copy_flush;
			    }
			    break;
			}
		    }
	        }
	    }
	    else /* !(silence->stop) */
	    {
	        memcpy(obuf, ibuf, sizeof(LONG)*nrOfTicks);
	        nrOfInSamplesRead += nrOfTicks;
	        nrOfOutSamplesWritten += nrOfTicks;
	    }
	    break;

	case SILENCE_COPY_FLUSH:
silence_copy_flush:
	    nrOfTicks = min((silence->stop_holdoff_end -
			        silence->stop_holdoff_offset), 
	                    (*osamp-nrOfOutSamplesWritten)) / effp->ininfo.channels;
	    for(i = 0; i < nrOfTicks; i++)
	    {
		*obuf++ = silence->stop_holdoff[silence->stop_holdoff_offset++];
		nrOfOutSamplesWritten++;
	    }

	    if (silence->stop_holdoff_offset == silence->stop_holdoff_end)
	    {
		silence->stop_holdoff_offset = 0;
		silence->stop_holdoff_end = 0;
		silence->mode = SILENCE_COPY;
		/* Return to copy mode incase there are is more room in
		 * output buffer to copy some more data from input buffer.
		 */
		goto silence_copy;
	    }
	    break;

	case SILENCE_STOP:
	    nrOfInSamplesRead = *isamp;
	    break;
	}

	*isamp = nrOfInSamplesRead;
	*osamp = nrOfOutSamplesWritten;

	return (ST_SUCCESS);
}

int st_silence_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp)
{
    silence_t silence = (silence_t) effp->priv;
    int i;
    LONG nrOfTicks, nrOfOutSamplesWritten = 0;

    /* Only if in flush mode will there be possible samples to write
     * out during drain() call.
     */
    if (silence->mode == SILENCE_COPY_FLUSH)
    {
        nrOfTicks = min((silence->stop_holdoff_end - 
		            silence->stop_holdoff_offset), 
	                *osamp) / effp->ininfo.channels;
	for(i = 0; i < nrOfTicks; i++)
	{
	    *obuf++ = silence->stop_holdoff[silence->stop_holdoff_offset++];
	    nrOfOutSamplesWritten++;
        }

	if (silence->stop_holdoff_offset == silence->stop_holdoff_end)
	{
	    silence->stop_holdoff_offset = 0;
	    silence->stop_holdoff_end = 0;
	    silence->mode = SILENCE_STOP;
	}
    }

    *osamp = nrOfOutSamplesWritten;
    return(ST_SUCCESS);
}

int st_silence_stop(eff_t effp)
{
    silence_t silence = (silence_t) effp->priv;

    free(silence->stop_holdoff);
    return(ST_SUCCESS);
}
