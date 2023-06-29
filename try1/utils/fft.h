#include <stdbool.h>
#include <complex.h>  // xxx comment this needs to be before fftw3.h
#include <fftw3.h>

complex * fft_alloc_complex(int n);
double * fft_alloc_real(int n);

void fft_fwd_r2c(double *in, complex *out, int n);
void fft_back_c2r(complex *in, double *out, int n, bool normalize);
void fft_fwd_c2c(complex *in, complex *out, int n);
void fft_back_c2c(complex *in, complex *out, int n, bool normalize);

void fft_bpf_complex(complex *in, complex *out, int n, double sample_rate, double f_low, double f_high);
void fft_lpf_real(double *in, double *out, int n, double sample_rate, double f_cutoff);

