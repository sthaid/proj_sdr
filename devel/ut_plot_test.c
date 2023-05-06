#include "common.h"

// -----------------  PLOT TEST  ---------------------------

void *plot_test(void *cx)
{
    int sample_rate, f, n;
    double *sine_wave, t, dt;
    double xv_blue_cursor, xv_blue_cursor_delta;
    double xv_red_cursor, xv_red_cursor_delta;

    #define SECS 1

    sprintf(tc.info, "Hello World");

    sample_rate = 20000;
    f = 10;
    n = SECS * sample_rate;
    sine_wave = fftw_alloc_real(n);
    dt = (1. / sample_rate);
    t = 0;
    xv_blue_cursor = 0;
    xv_red_cursor = 1;
    xv_blue_cursor_delta = .001;
    xv_red_cursor_delta = .001;

    for (int i = 0; i < n; i++) {
        sine_wave[i] = sin(TWO_PI * f * t);
        t += dt;
    }

    while (true) {
        plot_real(0, sine_wave, n,   
                  0, 1,  -1, +1,       
                  xv_blue_cursor, xv_red_cursor, "SINE WAVE", NULL,            
                  0,  0, 50, 25);
        plot_real(1, sine_wave, n,   
                  0, 1,  -0.5, +0.5,   
                  NOC, NOC, "SINE WAVE", NULL,   
                  0, 25, 50, 25);;
        plot_real(2, sine_wave, n,   
                  0, 1,  0.5, +1.5,    
                  NOC, NOC, "SINE WAVE", NULL,   
                  0, 50, 50, 25);;

        xv_blue_cursor += xv_blue_cursor_delta;
        if (xv_blue_cursor > 1) xv_blue_cursor_delta = -.001;
        if (xv_blue_cursor < 0) xv_blue_cursor_delta = +.001;

        xv_red_cursor += xv_red_cursor_delta;
        if (xv_red_cursor > 1) xv_red_cursor_delta = -.001;
        if (xv_red_cursor < 0) xv_red_cursor_delta = +.001;

        usleep(10000);
    }

    return NULL;
}

