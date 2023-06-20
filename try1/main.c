#define MAIN

#include "common.h"

void test(void);

int main(int argc, char **argv)
{
    // xxx
    if (setlocale(LC_ALL, "") == NULL) {
        FATAL("setlocale failed, %m\n");
    }
    if (MB_CUR_MAX <= 1) {
        FATAL("MB_CUR_MAX = %ld\n", MB_CUR_MAX);
    }

    // get options
    // xxx more options
    // - which device
    // - sample rate
    // - help
    // - ...
    while (true) {
       static struct option options[] = {
           {"list", no_argument,       NULL,  'l' },
           {"test", no_argument,       NULL,  't' },
           {0,      0,                 NULL,  0   } };

        int c = getopt_long(argc, argv, "lt", options, NULL);
        if (c == -1) {
            break;
        }
        DEBUG("option = %c\n", c);

        switch (c) {
        case 'l':
            sdr_list_devices();
            return 0;
        case 't':
            sdr_test(0, SDR_SAMPLE_RATE);  // xxx opt needed for idx
            return 0;
        case '?':
            return 1;
        default:
            FATAL("getoption returned 0x%x\n", c);
            break;
        }
    }

    // initialization
    //sdr_init(0, SDR_SAMPLE_RATE);  // xxx opt for idx
    config_init();

    test();
    return 1;

    audio_init();
    display_init();

    // runtime
    display_handler();

    // program terminating
    //config_write();
    NOTICE("program terminating\n");
}

void test(void)
{
    band_t *b = &band[0];

    // verify the 1st band is fm
    if (strcmp(b->name, "TEST") != 0) {
        FATAL("not TEST\n");
    }

    // divide the band into N intervals of less than 1.2MHz
    double full_span, intvl;
    int n;
    #define FFT_SPAN  1.0
    full_span = b->f_max - b->f_min;
    n = ceil(full_span / FFT_SPAN);
    intvl = full_span / n;
    NOTICE("full_span = %f  n = %d  intvl = %f\n", full_span, n, intvl);

    // determine the number of samples in the fft, so that each bin is 10Hz
    int samples = (intvl * 1000000 * 2) / 10;
    NOTICE("samples = %d\n", samples);

    // alloc buff for fft
    complex *data = fft_alloc_complex(samples);
    complex *fft = fft_alloc_complex(samples);

    // loop over the intervals
    // FM 88 - 108
    // TEST 0.4 to 3.4
    double ctr_freq = b->f_min + intvl/2;
    for (int i = 0; i < n; i++) {
        NOTICE("i=%d  ctr_freq=%f\n", i, ctr_freq);

        // xxx init data with a test pattern of freqs in each intvl 
        sdr_get_data(ctr_freq, data, samples);

        // do the fft
        fft_fwd_c2c(data, fft, samples);

        // save the fft result

        // move to next interval in the band
        ctr_freq += intvl;
    }
}
