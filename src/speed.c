/*
 * (c) Fabien Coelho <fabien@coelho.net> for sox, 03/2000.
 *
 * see sox copyright.
 *
 * speed up or down the sound, like a tape.
 * basically it's like resampling without resampling;-)
 * this could be done just by changing the rate of the file?
 * I don't know whether any rate is admissible.
 * implemented with a slow automaton, but it's easier than one more buffer.
 * I think it is especially inefficient.
 */

#include "st_i.h"

#include <math.h> /* pow */
#include <string.h>

static st_effect_t st_speed_effect;

/* automaton status
 */
typedef enum { sp_input, sp_transfer, sp_compute } buffer_state_t;

/* internal structure
 */
typedef struct
{
    /* options
     */
    double factor;   /* speed factor. */

    /* internals.
     */
    double rate;     /* rate of buffer sweep */

    int compression;      /* integer compression of the signal. */
    int index;            /* how much of the input buffer is filled */
    st_sample_t *ibuf;    /* small internal input buffer for compression */

    double cbuf[4];  /* computation buffer for interpolation */
    double frac;     /* current index position in cbuf */
    int icbuf;            /* available position in cbuf */

    buffer_state_t state; /* automaton status */

} * speed_t;

/*
static void debug(char * where, speed_t s)
{
    st_debug("%s: f=%f r=%f comp=%d i=%d ic=%d frac=%f state=%d v=%f",
            where, s->factor, s->rate, s->compression, s->index,
            s->icbuf, s->frac, s->state, s->cbuf[0]);
}
*/

/* compute f(x) with a cubic interpolation...
 */
static double cub(
  double fm1, /* f(-1) */
  double f0,  /* f(0)  */
  double f1,  /* f(1)  */
  double f2,  /* f(2)  */
  double x)   /* 0.0 <= x < 1.0 */
{
    /* a x^3 + b x^2 + c x + d */
    register double a, b, c, d;

    d = f0;
    b = 0.5 * (f1+fm1) - f0;
    a = (1.0/6.0) * (f2-f1+fm1-f0-4.0*b);
    c = f1 - a - b - d;
    
    return ((a * x + b) * x + c) * x + d;
}

/* get options. */
static int st_speed_getopts(eff_t effp, int n, char **argv)
{
    speed_t speed = (speed_t) effp->priv;
    int cent = 0;

    speed->factor = 1.0; /* default */

    if (n>0 && !strcmp(argv[0], "-c"))
    {
        cent = 1;
        argv++; n--;
    }

    if (n && (!sscanf(argv[0], "%lf", &speed->factor) ||
              (cent==0 && speed->factor<=0.0)))
    {
        st_debug("n = %d cent = %d speed = %f",n,cent,speed->factor);
        st_fail(st_speed_effect.usage);
        return ST_EOF;
    }
    else if (cent != 0) /* CONST==2**(1/1200) */
    {
        speed->factor = pow((double)1.00057778950655, speed->factor);
        /* st_debug("Speed factor: %f", speed->factor);*/
    }

    return ST_SUCCESS;
}

/* start processing. */
static int st_speed_start(eff_t effp)
{
    speed_t speed = (speed_t) effp->priv;

    if (speed->factor >= 1.0)
    {
        speed->compression = (int) speed->factor; /* floor */
        speed->rate = speed->factor / speed->compression;
    }
    else
    {
        speed->compression = 1;
        speed->rate = speed->factor;
    }

    speed->ibuf   = (st_sample_t *) malloc(speed->compression*
                                           sizeof(st_sample_t));
    speed->index  = 0;

    speed->state = sp_input;
    speed->cbuf[0] = 0.0; /* default previous value for interpolation */
    speed->icbuf = 1;
    speed->frac = 0.0;

    if (!speed->ibuf) {
        st_fail("malloc failed");
        return ST_EOF;
    }

    return ST_SUCCESS;
}

/* transfer input buffer to computation buffer.
 */
static void transfer(speed_t speed)
{
    register int i;
    register double s = 0.0;

    for (i=0; i<speed->index; i++)
        s += (double) speed->ibuf[i];
    
    speed->cbuf[speed->icbuf++] = s / ((double) speed->index);
    
    if (speed->icbuf == 4)
        speed->state = sp_compute;
    else
        speed->state = sp_input;
    
    speed->index = 0;
}

/* interpolate values
 */
static st_size_t compute(eff_t effp, speed_t speed, st_sample_t *obuf, st_size_t olen)
{
    st_size_t i;

    for(i = 0;
        i<olen && speed->frac < 1.0;
        i++, speed->frac += speed->rate)
    {
        float f;

        f = cub(speed->cbuf[0], speed->cbuf[1],
                speed->cbuf[2], speed->cbuf[3], 
                speed->frac);
        ST_SAMPLE_CLIP_COUNT(f, effp->clippedCount);
        obuf[i] = f;
    }
    
    if (speed->frac >= 1.0)
    {
        speed->frac -= 1.0;
        speed->cbuf[0] = speed->cbuf[1];
        speed->cbuf[1] = speed->cbuf[2];
        speed->cbuf[2] = speed->cbuf[3];
        speed->icbuf = 3;
        speed->state = sp_input;
    }

    return i; /* number of data out */
}

/* handle a flow.
 */
static int st_speed_flow(eff_t effp, const st_sample_t *ibuf, st_sample_t *obuf, 
                  st_size_t *isamp, st_size_t *osamp)
{
    speed_t speed;
    st_size_t len, iindex, oindex;
    st_sample_t *ibuf_copy;

    speed = (speed_t) effp->priv;

    ibuf_copy = (st_sample_t *)malloc(*isamp * sizeof(st_sample_t));
    memcpy(ibuf_copy, ibuf, *isamp * sizeof(st_sample_t));

    len = min(*isamp, *osamp);
    iindex = 0;
    oindex = 0;

    while (iindex<len && oindex<len)
    {
        /* store to input buffer. */
        if (speed->state==sp_input)
        {
            speed->ibuf[speed->index++] = ibuf_copy[iindex++];
            if (speed->index==speed->compression)
                speed->state = sp_transfer;
        }

        /* transfer to compute buffer. */
        if (speed->state==sp_transfer)
            transfer(speed);

        /* compute interpolation. */
        if (speed->state==sp_compute)
            oindex += compute(effp, speed, obuf+oindex, len-oindex);
    }

    *isamp = iindex;
    *osamp = oindex;

    free(ibuf_copy);
    
    return ST_SUCCESS;
}

/* end of stuff. 
 */
static int st_speed_drain(eff_t effp, st_sample_t *obuf, st_size_t *osamp)
{
    speed_t speed = (speed_t) effp->priv;
    st_size_t i, oindex;

    transfer(speed);

    /* fix up trail by emptying cbuf */
    for (oindex=0, i=0; i<2 && oindex<*osamp;)
    {
        if (speed->state==sp_input)
        {
            speed->ibuf[speed->index++] = 0.0;
            i++;
            if (speed->index==speed->compression)
                speed->state = sp_transfer;
        }

        /* transfer to compute buffer. */
        if (speed->state==sp_transfer)
            transfer(speed);

        /* compute interpolation. */
        if (speed->state==sp_compute)
            oindex += compute(effp, speed, obuf+oindex, *osamp-oindex);
    }

    *osamp = oindex; /* report how much was generated. */

    if (speed->state==sp_input)
        return ST_EOF;
    else
        return ST_SUCCESS;
}

/* stop processing. report overflows. 
 */
static int st_speed_stop(eff_t effp)
{
    speed_t speed = (speed_t) effp->priv;

    free(speed->ibuf);
    
    return ST_SUCCESS;
}

static st_effect_t st_speed_effect = {
  "speed",
  "Usage: speed [-c] factor (default 1.0, <1 slows, -c: factor in cent)",
  0,
  st_speed_getopts,
  st_speed_start,
  st_speed_flow,
  st_speed_drain,
  st_speed_stop
};

const st_effect_t *st_speed_effect_fn(void)
{
    return &st_speed_effect;
}
