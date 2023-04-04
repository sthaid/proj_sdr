#include "common.h"

//
// defines 
//

#define KHZ 1000

#define SAMPLE_RATE 2400000
#define AUDIO_SAMPLE_RATE 22000

#define FC 50000

//
// variables
//

char *progname = "sim";

//
// prototypes
//

void audio_out(double yo);

// -----------------  MAIN  -------------------------------------------

int main(int argc, char **argv)
{
    double dt = 1. / SAMPLE_RATE;
    double yo = 0;
    double t = 0;
    int    cnt = 0;
    double y;

    // init
    init_sine_wave();
    init_audio_src();

    // xxx comment
    while (true) {
        // modulate
        y = 0;
        y += (1 + get_src(0,t)) * (0.5 * sine_wave(FC,t));

        // detector
        if (y > yo) {
            yo = y;
        }
        yo = .999 * yo;

        // audio out
        if (cnt++ == (SAMPLE_RATE / AUDIO_SAMPLE_RATE)) {
            audio_out(yo);
            cnt = 0;
        }

        // advance time
        t += dt;
    }
}

// -----------------  xxx  --------------------------------------------

void audio_out(double yo)
{
    #define MAX_MA 1000
    #define MAX_OUT 1000

    static void *ma_cx;
    static float out[MAX_OUT];
    static int max;

    double ma;

    ma = moving_avg(yo, MAX_MA, &ma_cx);
    out[max++] = yo - ma;

    if (max == MAX_OUT) {
        fwrite(out, sizeof(float), MAX_OUT, stdout);
#if 0
        double out_min, out_max, out_avg;
        average_float(out, MAX_OUT, &out_min, &out_max, &out_avg);        
        fprintf(stderr, "min=%f max=%f avg=%f\n", out_min, out_max, out_avg);
#endif
        max = 0;
    }
}

// -----------------  xxx  --------------------------------------------
// -----------------  save --------------------------------------------
#if 0
double de_modulate(double y, double ftune, double t)
{
    double tmp;

    tmp = y * sine_wave(ftune,t);
    //return tmp;

    return lpf(tmp,1,.90);
}

    // xxx cleanup
    int cnt = 0;
    int num_usleep = 0;
    unsigned long start_us, t_us, real_us;
    start_us = microsec_timer();

        if (t > 5) {
            NOTICE("5 secs %d\n", num_usleep);
            exit(1);
        }

        if (cnt++ > 1000) {
            t_us = t * 2000;
            real_us = microsec_timer() - start_us;
            if (t_us > real_us + 1000) {
                usleep(t_us - real_us);
                num_usleep++;
            }
            cnt = 0;
        }
double modulate(double m, double fc, double t)
{
    double y;

    y = (1 + m) * (0.5 * sine_wave(fc,t));  // xxx optimize

    return y;
}

double average(float *array, int cnt)
{
    double sum = 0;
    for (int i = 0; i < cnt; i++) {
        sum += array[i];
    }
    return sum / cnt;
}

double moving_avg(float v)
{
    #define MAX_VALS 10000

    static float vals[MAX_VALS];
    static int idx;
    static double sum;

    sum += (v - vals[idx]);
    vals[idx] = v;
    if (++idx == MAX_VALS) idx = 0;
    return sum / MAX_VALS;
}
#endif
