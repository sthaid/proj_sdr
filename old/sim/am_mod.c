#include "common.h"

#define MAX_SIG 1000
#define MAX_Y   1000

char *progname = "am_mod";

int main(int argc, char ** argv)
{
    float  sig[MAX_SIG];
    float  y[MAX_Y];
    int    max_y;
    double fc;
    double dt_carrier, dt_sig;
    double t, t_sig_start, t_sig_end;
    int    total_sig;

    int n, i;
    float s;

    //fc            = 1.0e6;  // 1 MHz
    fc            = 0.5e6;  // 1 MHz
    dt_carrier    = 1. / (16 * fc);
    dt_sig        = 1. / SAMPLE_RATE;
    t             = 0;
    t_sig_start   = -1;
    t_sig_end     = -1;
    max_y         = 0;
    total_sig     = 0;

    // xxx cleanup
    int cnt = 0;
    int num_usleep = 0;
    unsigned long start_us, t_us, real_us;
    start_us = microsec_timer();

    while (true) {
        for (t = 0; ; t += dt_carrier) { // xxx maybe t can be us
#if 0
            if (t > 5) {
                NOTICE("5 secs %d\n", num_usleep);
                exit(1);
            }
#endif

#if 1
            if (cnt++ > 1000) {
                t_us = t * 1000000;
                real_us = microsec_timer() - start_us;
                if (t_us > real_us + 10) {
                    usleep(t_us - real_us);
                    num_usleep++;
                }
                cnt = 0;
            }
#endif

            // if more signal values are needed then get them from stdin
            if (t > (t_sig_end + dt_sig / 2)) {
                // read signal values from stdin
                n = fread(sig, sizeof(float), MAX_SIG, stdin);
                if (n != MAX_SIG) {
                    NOTICE("program terminating\n");
                    exit(0);
                }
                total_sig += n;

                // determine the start and end times for the signal values read
                t_sig_end = (total_sig - 1) * dt_sig;
                t_sig_start = t_sig_end - ((MAX_SIG - 1) * dt_sig);
                //NOTICE("t_sig %f %f\n", t_sig_start, t_sig_end);

                // reduce signal amplitude by moduleation factor
                for (i = 0; i < MAX_SIG; i++) {
                    sig[i] = sig[i] * 0.5;  // xxx m
                }
            }

#if 1
            // get signal value (s) at time t
            i = rint((t - t_sig_start) / dt_sig);
            if (i < 0 || i >= MAX_SIG) {
                ERROR("invalid sig array idx %d\n", i);
                exit(1);
            }
            s = sig[i];

            // calculate amplitude modulated output at time t
            // xxx optimize
            //y[max_y++] = (1 + s) * sin(TWO_PI * fc * t);
            y[max_y++] = (1 + s) * 1;
#endif

            // if y buff is full then write it
            if (max_y == MAX_Y) {
                n = fwrite(y, sizeof(float), MAX_Y, stdout);
                if (n != MAX_Y) {
                    ERROR("fwrite failed, %s\n", strerror(errno));
                    exit(1);
                }
                max_y = 0;
            }
        }
    }
}
    
#if 0
    while (true) {
        long secs;
        scanf("%ld", &secs);
        printf("secs = %ld\n", secs);
        double angle = 10000000. * TWO_PI * secs;
        printf("angle %f\n", angle);
        printf("%f\n", sin(angle));
        printf("%f\n", sin(angle + M_PI/2));
        printf("%f\n", sin(angle - M_PI/2));
        printf("%f\n", sin(angle + M_PI));
    }
#endif
