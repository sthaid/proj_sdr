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
    //config_write();
    //return 1;
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
    freq_t      band_freq_span;
    freq_t      fft_freq_span;
    freq_t      ctr_freq;
    int         num_fft;
    int         fft_n;
    int         fft_plot_n;
    int         i, j, k1, k2;
    unsigned long start, dur_sdr_get_data=0, dur_fft=0, dur_cabs=0;

    #define MAX_FFT_FREQ_SPAN  1200000  // must be <= SDR_SAMPLE_RATE/2
    #define FFT_ELEMENT_HZ     10

    assert(MAX_FFT_FREQ_SPAN <= SDR_SAMPLE_RATE/2);

    // divide the band into fft intervals
    band_freq_span = b->f_max - b->f_min;
    num_fft = ceil((double)band_freq_span / MAX_FFT_FREQ_SPAN);
    fft_freq_span = band_freq_span / num_fft;
    assert(fft_freq_span <= MAX_FFT_FREQ_SPAN);
    NOTICE("band_freq_span = %ld  num_fft = %d  fft_freq_span = %ld\n", band_freq_span, num_fft, fft_freq_span);

    // determine the number of samples in the fft, so that each entry is 10Hz
    fft_n = SDR_SAMPLE_RATE / FFT_ELEMENT_HZ;
    NOTICE("fft_n = %d\n", fft_n);

    // determine the number of samples from the fft_out buff that will be xfered to the plot buffer
    fft_plot_n = fft_freq_span / FFT_ELEMENT_HZ;
    NOTICE("fft_plot_n = %d\n", fft_plot_n);

    // alloc 
    if (b->cabs_fft == NULL) {
        b->max_cabs_fft = num_fft * fft_plot_n;
        NOTICE("max_cabs_fft = %d\n", b->max_cabs_fft);

        b->fft_in   = fft_alloc_complex(fft_n);
        b->fft_out  = fft_alloc_complex(fft_n);
        b->cabs_fft = calloc(b->max_cabs_fft, sizeof(double));
    }

    // loop over the intervals
    ctr_freq = b->f_min + fft_freq_span/2;
    k1 = 0;
    for (i = 0; i < num_fft; i++) {
        NOTICE("i=%d  ctr_freq=%ld\n", i, ctr_freq);

        // get a block of rtlsdr data at ctr_freq
        start = microsec_timer();
        sdr_get_data(ctr_freq, b->fft_in, fft_n);
        dur_sdr_get_data += (microsec_timer() - start);

        // run fft
        start = microsec_timer();
        fft_fwd_c2c(b->fft_in, b->fft_out, fft_n);
        dur_fft += (microsec_timer() - start);

        // save the cabs of fft result
        start = microsec_timer();
        k2 = fft_n - fft_plot_n / 2;
        for (j = 0; j < fft_plot_n; j++) {
            b->cabs_fft[k1++] = cabs(b->fft_out[k2++]);
            if (k2 == fft_n) k2 = 0;
        }
        dur_cabs += (microsec_timer() - start);

        // move to next interval in the band
        ctr_freq += fft_freq_span;
    }
    assert(k1 == b->max_cabs_fft);

    NOTICE("dur_sdr_get_data = %ld ms\n", dur_sdr_get_data/1000);
    NOTICE("dur_fft          = %ld ms\n", dur_fft/1000);
    NOTICE("dur_cabs         = %ld ms\n", dur_cabs/1000);
}
