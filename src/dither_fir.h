#define _ d -= p->coefs[j] * p->previous_errors[p->pos + j], ++j;
static int NAME(sox_effect_t * effp, const sox_sample_t * ibuf,
    sox_sample_t * obuf, size_t * isamp, size_t * osamp)
{
  priv_t * p = (priv_t *)effp->priv;
  size_t len = *isamp = *osamp = min(*isamp, *osamp);

  while (len--) {
    double r = floor(p->am0 * RANQD1) + floor(p->am1 * RANQD1);
    double error, d = *ibuf++;
    int j = 0;
    CONVOLVE
    assert(j == N);
    *obuf = SOX_ROUND_PREC_CLIP_COUNT(d + r, effp->clips);
    error = ((*obuf++ + (1 << (31-p->prec))) & (-1 << (32-p->prec))) - d;
    p->pos = p->pos? p->pos - 1 : p->pos - 1 + N;
    p->previous_errors[p->pos + N] = p->previous_errors[p->pos] = error;
  }
  return SOX_SUCCESS;
}
#undef CONVOLVE
#undef _
#undef NAME
#undef N
