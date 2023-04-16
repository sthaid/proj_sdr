
#include <complex.h>  // xxx comment
#include <fftw3.h>

void lpf_complex(complex *data, int n, double sample_rate, double f, char *str);
void lpf_real(double *data, int n, double sample_rate, double f, char *str);
