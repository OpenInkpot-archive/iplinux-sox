
/*
 * July 5, 1991
 * Copyright 1991 Lance Norskog And Sundry Contributors
 * This source code is freely redistributable and may be used for
 * any purpose.  This copyright notice must be maintained. 
 * Lance Norskog And Sundry Contributors are not responsible for 
 * the consequences of using this software.
 */

/*
 * Sound Tools statistics "effect" file.
 *
 * Build various statistics on file and print them.
 * No output.
 */

#include <math.h>
#include "st.h"

/* Private data for STAT effect */
typedef struct statstuff {
	double	min, max;
	double	asum;
	double	sum1, sum2;	/* amplitudes */
	double	dmin, dmax;
	double	dsum1, dsum2;	/* deltas */
	double	scale;		/* scale-factor    */
	double	last;		/* previous sample */
	ULONG	read;		/* samples processed */
	int	volume;
	int	srms;
	ULONG   bin[4];
} *stat_t;


/*
 * Process options
 */
int st_stat_getopts(effp, n, argv) 
eff_t effp;
int n;
char **argv;
{
	stat_t stat = (stat_t) effp->priv;

	stat->scale = MAXLONG;
	stat->volume = 0;
	stat->srms = 0;
	while (n>0)
	{
		if (!(strcmp(argv[0], "-v"))) {
			stat->volume = 1;
			goto did1;
		}
		if (!(strcmp(argv[0], "-s"))) {
			double scale;

			if (n <= 1) 
			{
			  fail("-s option: invalid argument");
			  return (ST_EOF);
			}
			if (!strcmp(argv[1],"rms")) {
				stat->srms=1;
				goto did2;
			}
			if (!sscanf(argv[1], "%lf", &scale))
			{
			  fail("-s option: invalid argument");
			  return (ST_EOF);
			}
			stat->scale = scale;
			goto did2;
		}
		if (!(strcmp(argv[0], "-rms"))) {
			double scale;
			if (n <= 1 || !sscanf(argv[1], "%lf", &scale))
			{
			  fail("-s option expects float argument");
			  return(ST_EOF);
			}
			stat->srms = 1;
			goto did2;
		}
		if (!(strcmp(argv[0], "debug"))) {
			stat->volume = 2;
			goto did1;
		}
		else
		{
			fail("Summary effect: unknown option");
			return(ST_EOF);
		}
	  did2: --n; ++argv;
	  did1: --n; ++argv;
	}
	return (ST_SUCCESS);
}

/*
 * Prepare processing.
 */
int st_stat_start(effp)
eff_t effp;
{
	stat_t stat = (stat_t) effp->priv;
	int i;

	stat->min = stat->max = 0;
	stat->asum = 0;
	stat->sum1 = stat->sum2 = 0;

	stat->dmin = stat->dmax = 0;
	stat->dsum1 = stat->dsum2 = 0;

	stat->last = 0;
	stat->read = 0;

	for (i = 0; i < 4; i++)
		stat->bin[i] = 0;

	return (ST_SUCCESS);
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */

int st_stat_flow(effp, ibuf, obuf, isamp, osamp)
eff_t effp;
LONG *ibuf, *obuf;
LONG *isamp, *osamp;
{
	stat_t stat = (stat_t) effp->priv;
	int len, done;
	short count;

	count = 0;
	len = ((*isamp > *osamp) ? *osamp : *isamp);
	if (len==0) return (ST_SUCCESS);

	if (stat->read == 0)	/* 1st sample */
		stat->min = stat->max = stat->last = (*ibuf)/stat->scale;

	for(done = 0; done < len; done++) {
		long lsamp;
		double samp, delta;
		/* work in scaled levels for both sample and delta */
		lsamp = *ibuf++;
		samp = (double)lsamp/stat->scale;
		stat->bin[RIGHT(lsamp,30)+2]++;
		*obuf++ = lsamp;

		if (stat->volume == 2)
		{
		    fprintf(stderr,"%f ",samp);
		    if (count++ == 5)
		    {
			fprintf(stderr,"\n");
			count = 0;
		    }
		}

		/* update min/max */
		if (stat->min > samp)
			stat->min = samp;
		else if (stat->max < samp)
			stat->max = samp;

		stat->sum1 += samp;
		stat->sum2 += samp*samp;
		stat->asum += fabs(samp);
		
		delta = fabs(samp - stat->last);
		if (delta < stat->dmin)
			stat->dmin = delta;
		else if (delta > stat->dmax)
			stat->dmax = delta;

		stat->dsum1 += delta;
		stat->dsum2 += delta*delta;

		stat->last = samp;
	}
	stat->read += len;
	*isamp = *osamp = len;
	/* Process all samples */
	return (ST_SUCCESS);
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 */
int st_stat_stop(effp)
eff_t effp;
{
	stat_t stat = (stat_t) effp->priv;
	double amp, scale, rms = 0, freq;
	double x, ct;

	ct = stat->read;

	if (stat->srms) {  /* adjust results to units of rms */
		double f;
		rms = sqrt(stat->sum2/ct);
		f = 1.0/rms;
		stat->max *= f;
		stat->min *= f;
		stat->asum *= f;
		stat->sum1 *= f;
		stat->sum2 *= f*f;
		stat->dmax *= f;
		stat->dmin *= f;
		stat->dsum1 *= f;
		stat->dsum2 *= f*f;
		stat->scale *= rms;
	}

	scale = stat->scale;

	amp = -stat->min;
	if (amp < stat->max)
		amp = stat->max;

	/* Just print the volume adjustment */
	if (stat->volume == 1 && amp > 0) {
		fprintf(stderr, "%.3f\n", MAXLONG/(amp*scale));
		return (ST_SUCCESS);
	}
	if (stat->volume == 2) {
		fprintf(stderr, "\n");
	}
	/* print out the info */
	fprintf(stderr, "Samples read:      %12lu\n", stat->read);
	fprintf(stderr, "Length (seconds):  %12.6f\n", (double)stat->read/effp->ininfo.rate);
	if (stat->srms)
		fprintf(stderr, "Scaled by rms:     %12.6f\n", rms);
	else
		fprintf(stderr, "Scaled by:         %12.1f\n", scale);
	fprintf(stderr, "Maximum amplitude: %12.6f\n", stat->max);
	fprintf(stderr, "Minimum amplitude: %12.6f\n", stat->min);
	fprintf(stderr, "Mean    norm:      %12.6f\n", stat->asum/ct);
	fprintf(stderr, "Mean    amplitude: %12.6f\n", stat->sum1/ct);
	fprintf(stderr, "RMS     amplitude: %12.6f\n", sqrt(stat->sum2/ct));

	fprintf(stderr, "Maximum delta:     %12.6f\n", stat->dmax);
	fprintf(stderr, "Minimum delta:     %12.6f\n", stat->dmin);
	fprintf(stderr, "Mean    delta:     %12.6f\n", stat->dsum1/(ct-1));
	fprintf(stderr, "RMS     delta:     %12.6f\n", sqrt(stat->dsum2/(ct-1)));
	freq = sqrt(stat->dsum2/stat->sum2)*effp->ininfo.rate/(M_PI*2);
	fprintf(stderr, "Rough   frequency: %12d\n", (int)freq);

	if (amp>0) fprintf(stderr, "Volume adjustment: %12.3f\n", MAXLONG/(amp*scale));

        if (stat->bin[2] == 0 && stat->bin[3] == 0)
                fprintf(stderr, "\nProbably text, not sound\n");
        else {

                x = (float)(stat->bin[0] + stat->bin[3]) / (float)(stat->bin[1] + stat->bin[2]);

                if (x >= 3.0)                  /* use opposite encoding */
		{
                        if (effp->ininfo.encoding == ST_ENCODING_UNSIGNED)
			{
                                fprintf (stderr,"\nTry: -t raw -b -s \n");
			}
                        else
			{
                                fprintf (stderr,"\nTry: -t raw -b -u \n");
			}

		}
                else if (x <= 1.0/3.0)
		{ 
		    ;;              /* correctly decoded */
		}
                else if (x >= 0.5 && x <= 2.0)       /* use ULAW */
		{
                        if (effp->ininfo.encoding == ST_ENCODING_ULAW)
			{
                                fprintf (stderr,"\nTry: -t raw -b -u \n");
			}
                        else
			{
                                fprintf (stderr,"\nTry: -t raw -b -U \n");
			}
		}
                else    
		{
                        fprintf (stderr, "\nCan't guess the type\n");
		}
        }
	return (ST_SUCCESS);

}
