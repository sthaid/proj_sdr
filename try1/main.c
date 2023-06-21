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
    //audio_init();
    display_init();

    test();

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
    full_span = b->f_max - b->f_min;  // xxx precision problem?
    n = ceil(full_span / FFT_SPAN);
    intvl = full_span / n;
    NOTICE("full_span = %f  n = %d  intvl = %f\n", full_span, n, intvl);

    // determine the number of samples in the fft, so that each bin is 10Hz
    int samples = nearbyint((intvl * 1200000 * 2) / 10);
    NOTICE("samples = %d\n", samples);

    // alloc buff for fft
    complex *data = fft_alloc_complex(samples);
    complex *fft = fft_alloc_complex(samples);

    // xxx
    if (b->cabs_fft == NULL) {
#if 1
        b->cabs_fft = malloc(n * samples / 2 * sizeof(double) + 20);
        b->max_cabs_fft = n * samples / 2;
#else
        b->cabs_fft = malloc(n * samples* sizeof(double) + 20);
        b->max_cabs_fft = n * samples;
#endif
        NOTICE("max_cabs_fft = %d\n", b->max_cabs_fft);
    }

    // loop over the intervals
    // FM 88 - 108
    // TEST 0.4 to 3.4
    double ctr_freq = b->f_min + intvl/2;
    int i, j, k=0, k2;
    unsigned long start, dur_sdr_get_data=0, dur_fft=0, dur_cabs=0;
    for (i = 0; i < n; i++) {
        NOTICE("i=%d  ctr_freq=%f\n", i, ctr_freq);

        // get a block of rtlsdr data at ctr_freq
        start = microsec_timer();
        sdr_get_data(ctr_freq, data, samples);
        dur_sdr_get_data += (microsec_timer() - start);

        // fft the data block
        start = microsec_timer();
        fft_fwd_c2c(data, fft, samples);
        dur_fft += (microsec_timer() - start);

        // save the cabs of fft result
        // xxx just the middle
        start = microsec_timer();
#if 1
        k2 = samples * 3 / 4;
        for (j = 0; j < samples/2; j++) {
            b->cabs_fft[k++] = cabs(fft[k2++]);
            if (k2 == samples) k2 = 0;
        }
#else
        for (j = 0; j < samples; j++) {
            b->cabs_fft[k++] = cabs(fft[j]);
        }
#endif
        dur_cabs += (microsec_timer() - start);

        // move to next interval in the band
        ctr_freq += intvl;
    }
    NOTICE("dur_sdr_get_data = %ld ms\n", dur_sdr_get_data/1000);
    NOTICE("dur_fft          = %ld ms\n", dur_fft/1000);
    NOTICE("dur_cabs         = %ld ms\n", dur_cabs/1000);
    NOTICE("k                = %d\n", k);
}
