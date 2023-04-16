
#include <complex.h>  // xxx comment
#include <fftw3.h>

void init_fft(void);

void lpf_complex(complex *data, int n, double sample_rate, double f, char *str);

void lpf_real(double *in, double *out, int n, double sample_rate, double f, char *str);

void fft_fwd_r2c(double *in, complex *out, int n);
void fft_back_c2r(complex *in, double *out, int n);

void fft_fwd_r2r(double *in, double *out, int n);

