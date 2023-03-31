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
    const double dt = 1 / 10e6;

    fc[0] = 500 * KHZ;
    ftune = 500 * KHZ;

    // init
    init_audio_src();

    // xxx
    t = 0;
    while (true) {
        y = modulate(0, fc[0], t);

        out[max++] = de_modulate(y, ftune, t);
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
    double m = 0.5;
    double y;

    y = (1 + m * get_src(id,t)) * (0.5 * sine_wave(fc,t));

    return y;
}

// -----------------  xxx  --------------------------------------------

double de_modulate(double y, double ftune, double t)
{
    double tmp;

    tmp = y * sine_wave(ftune,t);

    return lpf(tmp);
}

