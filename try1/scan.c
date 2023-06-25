#include "common.h"

static void *scan_thread(void *cx);
static void fft_band(band_t *b);
static void play_band(band_t *b);

// -----------------------------------------------------------------

void scan_init(void)
{
    pthread_t tid;

    // xxx do all the init here

    pthread_create(&tid, NULL, scan_thread, NULL);
}

// -----------------------------------------------------------------

static void *scan_thread(void *cx)
{
    int i;
    //int cnt=0;

    pthread_setname_np(pthread_self(), "sdr_scan");

    while (true) {
        //NOTICE("SCAN %d\n", ++cnt);

        for (i = 0; i < max_band; i++) {
            if (program_terminating) {
                goto terminate;
            }

            band_t *b = band[i];
            if (!b->selected) {
                continue;
            }

            fft_band(b);
        }

#if 1
        for (i = 0; i < max_band; i++) {
            if (program_terminating) {
                goto terminate;
            }

            band_t *b = band[i];
            if (!b->selected) {
                continue;
            }

            play_band(b);
        }
#endif
    }

terminate:
    return NULL;
}

// -----------------------------------------------------------------

// xxx clean this up
// xxx display dot where the play is occurring

static void fft_band(band_t *b)
{
    freq_t      band_freq_span;
    freq_t      fft_freq_span;
    freq_t      ctr_freq;
    int         num_fft;
    int         fft_n;
    int         fft_plot_n;
    int         i, j, k1, k2;
    unsigned long start, dur_sdr_read_sync=0, dur_fft=0, dur_cabs=0;

    #define MAX_FFT_FREQ_SPAN  1200000  // must be <= SDR_SAMPLE_RATE/2
    #define FFT_ELEMENT_HZ     100

    assert(MAX_FFT_FREQ_SPAN <= SDR_SAMPLE_RATE/2);

    // divide the band into fft intervals
    band_freq_span = b->f_max - b->f_min;
    num_fft = ceil((double)band_freq_span / MAX_FFT_FREQ_SPAN);
    fft_freq_span = band_freq_span / num_fft;
    assert(fft_freq_span <= MAX_FFT_FREQ_SPAN);
    //NOTICE("band_freq_span = %ld  num_fft = %d  fft_freq_span = %ld\n", band_freq_span, num_fft, fft_freq_span);

    // publish
    b->num_fft = num_fft;
    b->fft_freq_span = fft_freq_span;

    // determine the number of samples in the fft, so that each entry is 10Hz
    fft_n = SDR_SAMPLE_RATE / FFT_ELEMENT_HZ;
    //NOTICE("fft_n = %d\n", fft_n);

    // determine the number of samples from the fft_out buff that will be xfered to the plot buffer
    fft_plot_n = fft_freq_span / FFT_ELEMENT_HZ;
    //NOTICE("fft_plot_n = %d\n", fft_plot_n);

    // alloc 
    if (b->cabs_fft == NULL) {
        b->max_cabs_fft = num_fft * fft_plot_n;
        //NOTICE("max_cabs_fft = %d\n", b->max_cabs_fft);

        b->fft_in   = fft_alloc_complex(fft_n);
        b->fft_out  = fft_alloc_complex(fft_n);
        b->cabs_fft = calloc(b->max_cabs_fft, sizeof(double));

        b->wf.data = calloc(MAX_WATERFALL * b->max_cabs_fft, 1);
    }

    // loop over the intervals
    ctr_freq = b->f_min + fft_freq_span/2;
    k1 = 0;
    for (i = 0; i < num_fft; i++) {
        //NOTICE("i=%d  ctr_freq=%ld\n", i, ctr_freq);

        // get a block of rtlsdr data at ctr_freq
        start = microsec_timer();
        sdr_read_sync(ctr_freq, b->fft_in, fft_n);
        dur_sdr_read_sync += (microsec_timer() - start);

        // run fft
        start = microsec_timer();
        fft_fwd_c2c(b->fft_in, b->fft_out, fft_n);
        dur_fft += (microsec_timer() - start);

        // save the cabs of fft result
        start = microsec_timer();
        k2 = fft_n - fft_plot_n / 2;
        for (j = 0; j < fft_plot_n; j++) {
            b->cabs_fft[k1++] = cabs(b->fft_out[k2++]);  // xxx or log?
            if (k2 == fft_n) k2 = 0;
        }
        dur_cabs += (microsec_timer() - start);

        // move to next interval in the band
        ctr_freq += fft_freq_span;
    }
    assert(k1 == b->max_cabs_fft);

    // save new waterfall entry
    unsigned char *wf;
    wf = get_waterfall(b, -1);
    for (i = 0; i < b->max_cabs_fft; i++) {
        // max = 60000
        wf[i] = b->cabs_fft[i] * (256. / 60000);
    }
    b->wf.num++;

    // xxx keep timing stats
    //NOTICE("dur_sdr_read_sync = %ld ms\n", dur_sdr_read_sync/1000);
    //NOTICE("dur_fft          = %ld ms\n", dur_fft/1000);
    //NOTICE("dur_cabs         = %ld ms\n", dur_cabs/1000);

    // xxx save waterfaull
}

// ---------------------------------------------------------------------

static void play(freq_t f, int secs);
static complex lpf(complex x, double f_cut);
static void downsample_and_audio_out(double x);

static void play_band(band_t *b)
{
// xxx 
// - unit test moving avg
// - in display.c,  print the max to display
// - keep track of station bandwidth
#if 1
    #define N 21  // should be odd
    #define LIMIT 500

    void   *ma_cx = NULL;
    bool   found  = false;
    double v, ma, max=0;
    int    i, idx=0;
    freq_t f;

//  if (strcmp(b->name, "TEST") != 0) {
//      return;
//  }

    for (i = 0; i < b->max_cabs_fft; i++) {
        v = b->cabs_fft[i];
        ma = moving_avg(v, N, &ma_cx);
        if (i < N-1) continue;

        if (!found) {
            if (ma > LIMIT) {
                found = true;
                max = ma;
                idx = i;
            }
        } else {
            if (ma > max) {
                max = ma;
                idx = i;
            }

            if (ma < LIMIT) {
                idx -= N/2;
                f = b->f_min + idx * (b->f_max - b->f_min) / (b->max_cabs_fft - 1);
                if (true) {
                    NOTICE("FOUND %s f=%ld  max=%d\n", b->name, f, (int)max);
                    b->f_play = f;
                    play(f, 3);
                    //usleep(500000);
                    b->f_play = 0;
                } else {
                    NOTICE("DISCARD FOUND %s %ld\n", b->name, f);
                }
                found = false;
            }
        }
    }
    BLANKLINE;

    free(ma_cx);
#else
    if (strcmp(b->name, "TEST") != 0) {
        return;
    }

    NOTICE("PLAY BAND %s\n", b->name);

    // find strong signals
    freq_t f[] = { 500000, 540000, 460000 };
    int max = sizeof(f) / sizeof(f[0]);

    // play 
    for (int i = 0; i < max; i++) {
        b->f_play = f[i];
        play(f[i], 5);
        b->f_play = 0;
    }
#endif
}

// ---------------------------------------------------------------------

static void play(freq_t f, int secs)
{
    #define VOLUME_SCALE 10

    sdr_async_rb_t *rb;
    complex         data, data_lpf;
    double          data_demod, data_demod_volscale;
    int             max = secs * SDR_SAMPLE_RATE;

    rb = malloc(sizeof(sdr_async_rb_t));
    sdr_read_async(f, rb);

    while (true) {
        // wait for a data item to be available
        while (rb->head == rb->tail) {
            usleep(1000);
            if (program_terminating) {
                return;
            }
        }

        // process the data item
        data = rb->data[rb->head % MAX_SDR_ASYNC_RB_DATA];
        data_lpf = lpf(data, 4000);
        data_demod = cabs(data_lpf);
        data_demod_volscale = data_demod * VOLUME_SCALE;
        downsample_and_audio_out(data_demod_volscale);

        // done with this rb data item
        rb->head++;

        // if 5 secs have been processed then we are done playing
        if (rb->head == max) {
            break;
        }
    }

    sdr_cancel_async();
    free(rb);
}

#define LPF_ORDER  20 //xxx ?
static complex lpf(complex x, double f_cut)
{
    static double curr_f_cut;
    static BWLowPass *bwi, *bwq;

    if (f_cut != curr_f_cut) {
        curr_f_cut = f_cut;
        if (bwi) free_bw_low_pass(bwi);
        if (bwq) free_bw_low_pass(bwq);
        bwi = create_bw_low_pass_filter(LPF_ORDER, SDR_SAMPLE_RATE, f_cut);
        bwq = create_bw_low_pass_filter(LPF_ORDER, SDR_SAMPLE_RATE, f_cut);
    }

    return bw_low_pass(bwi, creal(x)) + bw_low_pass(bwq, cimag(x)) * I;
}

static void downsample_and_audio_out(double x)
{
    static int cnt;
    static void *ma_cx;
    double ma;

    double volume = 0.5;

    #define NUM_DS ((int)((double)SDR_SAMPLE_RATE / AUDIO_SAMPLE_RATE))

    ma = moving_avg(x, NUM_DS, &ma_cx);

    if (cnt++ == NUM_DS) {
        audio_out(ma * volume);
        cnt = 0;
    }
}
