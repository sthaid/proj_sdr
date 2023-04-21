
#include <complex.h>  // xxx comment
#include <fftw3.h>

void init_fft(void);

void fft_fwd_r2c(double *in, complex *out, int n);
void fft_back_c2r(complex *in, double *out, int n);
void fft_fwd_r2r(double *in, double *out, int n);
void fft_fwd_c2c(complex *in, complex *out, int n);
void fft_back_c2c(complex *in, complex *out, int n);

void fft_lpf_complex(complex *in, complex *out, int n, double sample_rate, double f);
void fft_lpf_real(double *in, double *out, int n, double sample_rate, double f);

void fft_bpf_complex(complex *in, complex *out, int n, double sample_rate, double f_low, double f_high);

