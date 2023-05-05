#include "common.h"

// -----------------  PLOT TEST  ---------------------------

void *plot_test(void *cx)
{
    int sample_rate, f, n;
    double *sine_wave, t, dt;
    double xv_cursor, xv_cursor_delta;

    #define SECS 1

    sprintf(tc.info, "Hello World");

    sample_rate = 20000;
    f = 10;
    n = SECS * sample_rate;
    sine_wave = fftw_alloc_real(n);
    dt = (1. / sample_rate);
    t = 0;
    xv_cursor = 0;
    xv_cursor_delta = .001;

    for (int i = 0; i < n; i++) {
        sine_wave[i] = sin(TWO_PI * f * t);
        t += dt;
    }

    while (true) {
        plot_real(0, sine_wave, n,   0, 1,  -1, +1,       xv_cursor, "SINE WAVE", NULL,            0,  0, 50, 25);
        plot_real(1, sine_wave, n,   0, 1,  -0.5, +0.5,   SDL_PLOT_NO_CURSOR, "SINE WAVE", NULL,   0, 25, 50, 25);;
        plot_real(2, sine_wave, n,   0, 1,  0.5, +1.5,    SDL_PLOT_NO_CURSOR, "SINE WAVE", NULL,   0, 50, 50, 25);;

        xv_cursor += xv_cursor_delta;
        if (xv_cursor > 1) xv_cursor_delta = -.001;
        if (xv_cursor < 0) xv_cursor_delta = +.001;

        usleep(10000);
    }

    return NULL;
}

