/*
 * Sound Tools Low-Pass effect file.
 *
 * (C) 2000 Chris Bagwell <cbagwell@sprynet.com>
 * See License file for further copyright information.
 *
 * Algorithm:  Recursive single pole lowpass filter
 *
 * Reference: The Scientist and Engineer's Guide to Digital Signal Processing
 *
 *      output[N] = input[N] * A + output[N-1] * B
 *
 *      X = exp(-2.0 * pi * Fc)
 *      A = 1 - X
 *      B = X
 *      Fc = cutoff freq / sample rate
 *
 * Mimics an RC low-pass filter:   
 *
 *     ---/\/\/\/\----------->
 *                    |
 *                   --- C
 *                   ---
 *                    |
 *                    |
 *                    V
 *
 */

#include <math.h>
#include "st_i.h"

static st_effect_t st_lowp_effect;

/* Private data for Lowpass effect */
typedef struct lowpstuff {
        float   cutoff;
        double  A, B;
        double  outm1;
} *lowp_t;

/*
 * Process options
 */
int st_lowp_getopts(eff_t effp, int n, char **argv) 
{
        lowp_t lowp = (lowp_t) effp->priv;

        if ((n < 1) || !sscanf(argv[0], "%f", &lowp->cutoff))
        {
                st_fail(st_lowp_effect.usage);
                return (ST_EOF);
        }
        return (ST_SUCCESS);
}

/*
 * Prepare processing.
 */
int st_lowp_start(eff_t effp)
{
        lowp_t lowp = (lowp_t) effp->priv;
        if (lowp->cutoff > effp->ininfo.rate / 2)
        {
                st_fail("Lowpass: cutoff must be < sample rate / 2 (Nyquest rate)");
                return (ST_EOF);
        }

        lowp->B = exp((-2.0 * M_PI * (lowp->cutoff / effp->ininfo.rate)));
        lowp->A = 1 - lowp->B;
        lowp->outm1 = 0.0;

        if (effp->globalinfo.octave_plot_effect)
        {
          printf(
            "title('SoX effect: %s cutoff=%g (rate=%u)')\n"
            "xlabel('Frequency (Hz)')\n"
            "ylabel('Amplitude Response (dB)')\n"
            "Fs=%u;minF=10;maxF=Fs/2;\n"
            "axis([minF maxF -95 5])\n"
            "sweepF=logspace(log10(minF),log10(maxF),200);\n"
            "grid on\n"
            "[h,w]=freqz([%f 0],[1 %f],sweepF,Fs);\n"
            "semilogx(w,20*log10(h),'b')\n"
            "pause\n"
            , effp->name, lowp->cutoff
            , effp->ininfo.rate, effp->ininfo.rate
            , lowp->A, -lowp->B
            );
          exit(0);
        }
        return (ST_SUCCESS);
}

/*
 * Processed signed long samples from ibuf to obuf.
 * Return number of samples processed.
 */
int st_lowp_flow(eff_t effp, const st_sample_t *ibuf, st_sample_t *obuf, 
                 st_size_t *isamp, st_size_t *osamp)
{
        lowp_t lowp = (lowp_t) effp->priv;
        int len, done;
        double d;
        st_sample_t l;

        len = ((*isamp > *osamp) ? *osamp : *isamp);

        for(done = 0; done < len; done++) {
                l = *ibuf++;
                d = lowp->A * l + lowp->B * lowp->outm1;
                ST_EFF_SAMPLE_CLIP_COUNT(d);
                lowp->outm1 = d;
                *obuf++ = d;
        }
        *isamp = len;
        *osamp = len;
        return (ST_SUCCESS);
}

/*
 * Do anything required when you stop reading samples.  
 * Don't close input file! 
 */
int st_lowp_stop(eff_t effp)
{
        /* nothing to do */
    return (ST_SUCCESS);
}

static st_effect_t st_lowp_effect = {
  "lowp",
  "Usage: lowp cutoff",
  0,
  st_lowp_getopts,
  st_lowp_start,
  st_lowp_flow,
  st_effect_nothing_drain,
  st_lowp_stop
};

const st_effect_t *st_lowp_effect_fn(void)
{
    return &st_lowp_effect;
}
