#include "fft4g.h"
#define  FIFO_SIZE_T int
#include "fifo.h"

typedef struct {
  int        dft_length, num_taps, post_peak;
  double   * coefs;
} dft_filter_filter_t;

typedef struct {
  size_t     samples_in, samples_out;
  fifo_t     input_fifo, output_fifo;
  dft_filter_filter_t   filter, * filter_ptr;
} dft_filter_priv_t;

