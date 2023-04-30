#include "common.h"

// -----------------  PLOT TEST  ---------------------------

void *plot_test(void *cx)
{
    int sample_rate, f, n;
    double *sw, t, dt;

    #define SECS 1

    sprintf(tc.info, "Hello World");

    sample_rate = 20000;
    f = 10;
    n = SECS * sample_rate;
    sw = fftw_alloc_real(n);
    dt = (1. / sample_rate);
    t = 0;

    for (int i = 0; i < n; i++) {
        sw[i] = sin(TWO_PI * f * t);
        t += dt;
    }

    plot_real(0, sw, n,  0, 1,  -1, +1,     "SINE WAVE", NULL,  0,  0, 50, 25);
    plot_real(1, sw, n,  0, 1,  -0.5, +0.5, "SINE WAVE", NULL,  0, 25, 50, 25);;
    plot_real(2, sw, n,  0, 1,  0.5, +1.5,  "SINE WAVE", NULL,  0, 50, 50, 25);;

    return NULL;
}

