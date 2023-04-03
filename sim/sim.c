#include "common.h"

//
// defines 
//

#define KHZ 1000

//
// variables
//

char *progname = "sim";

//
// prototypes
//

double modulate(int id, double fc, double t);
double de_modulate(double y, double ftune, double t);

// -----------------  MAIN  -------------------------------------------

int main(int argc, char **argv)
{
    double       t, y, ftune, fc[10];
    float        out[1000];
    int          max = 0;
    const double dt = 1 / 5e6;

    // xxx cleanup
    int cnt = 0;
    int num_usleep = 0;
    unsigned long start_us, t_us, real_us;
    start_us = microsec_timer();
    
    fc[0] = 500 * KHZ;
    fc[1] = 520 * KHZ;  // xxx move to 10khz
    ftune = 520 * KHZ;

    // init
    init_sine_wave();
    init_audio_src();

    // xxx
    t = 0;
    while (true) {
#if 0
        if (t > 5) {
            NOTICE("5 secs %d\n", num_usleep);
            exit(1);
        }
#endif
        if (cnt++ > 1000) {
            t_us = t * 10000;
            real_us = microsec_timer() - start_us;
            if (t_us > real_us + 1000) {
                usleep(t_us - real_us);
                num_usleep++;
            }
            cnt = 0;
        }

#if 1
        // xxx call get_src here
        y = modulate(0, fc[0], t) + 
            modulate(1, fc[1], t);

        static int cnt1;
        if (cnt1++ == 227) {  // 454
            out[max++] = de_modulate(y, ftune, t);
            cnt1 = 0;
        }
#else
        //printf("t = %f\n", t);
        static int cnt1;
        if (cnt1++ == 454) {
            out[max++] = get_src(0,t);
            cnt1 = 0;
        }
#endif
        //printf("%f\n", out[max-1]);

        if (max == 1000) {
            fwrite(out, sizeof(float), 1000, stdout);
            max = 0;
        }

        t += dt;
    }
}

// -----------------  xxx  --------------------------------------------

double modulate(int id, double fc, double t)
{
    double y;

    y = (1 + get_src(id,t)) * (0.5 * sine_wave(fc,t));  // xxx optimize

    return y;
}

// -----------------  xxx  --------------------------------------------

double de_modulate(double y, double ftune, double t)
{
    double tmp;

    tmp = y * sine_wave(ftune,t);

    return lpf(tmp,1,.90);
}

