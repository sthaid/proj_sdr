#include "common.h"

void radio_init(void)
{
    pthread_t tid;

    // xxx do all the init here
    //pthread_create(&tid, NULL, scan_thread, NULL);
}

// return true if event was handled
bool radio_event(sdl_event_t *ev)
{
    bool event_was_handled = true;

    switch(ev->event_id) {
    case SDL_EVENT_KEY_F(1):
        mode = MODE_FFT;
        break;
    case SDL_EVENT_KEY_F(2):
        mode = MODE_SCAN;
        break;
    case SDL_EVENT_KEY_F(3):
        mode = MODE_PLAY;
        break;
    default:
        event_was_handled = false;
        break;
    }

    return event_was_handled;
}


#if 0
// xxx also decode fm stereo

static void *scan_thread(void *cx);
static void fft_band(band_t *b);
static void play_band(band_t *b);

// -----------------------------------------------------------------

#if 0
ctrls:
- FFT RECV SCAN   3 buttons to set state  AND f1,f2,f3
- when fft
    . +/- to adjust interval
- when scanning
    . +/- to adjust play time
    . tab and ctrl-tab to go next,prev station
    . 'space' to pause/unpause
- when recv
    . freq ctrl
      - arrows
      - click on the fft
    . 'z' toggles zoom

displays:
- FFT RECV SCAN
- IN FFT mode
  - the desired and actual fft interval
- IN scan mode 
  - the play intvl, and paused or playing state
  - highlight the band being played,  and the station in the band
  - the frequency and demod
- IN Recv mode
  - the desired and actual fft interval
  - the frequency and demod

---------------------

thread ...
when entering a state:
- clear the published fft

ideas
- the top waterfall data is also what is displayed in the fft graph

while 1 {
    if state == SCAN || FFT
        wait for time to do the fft
        fft all bands that are selected
        add waterfall for all other bands,  so they all push down togethor
        find stations in all selected bands
        if scan
            play next station until all bands are done
               while playing, update the fft, but do not need to include it in the waterfall
        endif
    else if state == RECV
        get data, and play it
        if time for the fft
            do the fft,  and it will be displayed
            add waterfall for all other bands,  so they all push down togethor

            might want seperate fft thread
    endif
}
#endif


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

    bool sim = strncmp(b->name, "SIM", 3) == 0;

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
        sdr_read_sync(ctr_freq, b->fft_in, fft_n, sim);
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
// xxx todo
// - display range of stations in the band being played
// - snap the freq ?
// - adjust play time
// - ctrl chars
//     skip to next
//     pause
//     continue

static void find(band_t *b, int n, double limit);
static int compare(const void *a_arg, const void *b_arg);
static int add(band_t *b, freq_t f, freq_t bw);
static void play(freq_t f, int secs, bool sim);
static complex lpf(complex x, double f_cut);
static void downsample_and_audio_out(double x);

static void play_band(band_t *b)
{
    int i;
    bool sim;

//  if (strcmp(b->name, "TEST") != 0) {
//      return;
//  }

    sim = strncmp(b->name, "SIM", 3) == 0;

    NOTICE("PLAY BAND %s  sim=%d\n", b->name, sim);

    b->max_scan_station = 0;
    find(b, 50000,  500);  // xxx use squelch
    find(b,  1000,  500);

    qsort(b->scan_station, b->max_scan_station, sizeof(struct scan_station_s), compare);

    NOTICE("FIND RESULT cnt=%d\n", b->max_scan_station);
    for (i = 0; i < b->max_scan_station; i++) {
        struct scan_station_s *ss = &b->scan_station[i];
        NOTICE("f=%ld  bw=%ld\n", ss->f, ss->bw);
    }

    for (i = 0; i < b->max_scan_station; i++) {
        struct scan_station_s *ss = &b->scan_station[i];

        b->f_play = ss->f;
        play(ss->f, 1, sim);
        b->f_play = 0;
    }
}

static void find(band_t *b, int avg_freq_span, double threshold)
{
    void   *ma_cx = NULL;
    bool   found  = false;
    double v, ma, max=0;
    int    i, idx_of_max=0, idx_start=0, idx_end, n;
    freq_t f, bw;

//  if (strcmp(b->name, "TEST") != 0) {
//      return;
//  }

    n = avg_freq_span / FFT_ELEMENT_HZ;
    n |= 1;

    NOTICE("FIND CALLED AVG_FREQ_SPAN=%d  THRESHOLD=%f  --  N=%d\n", avg_freq_span, threshold, n);

    for (i = 0; i < b->max_cabs_fft; i++) {
        v = b->cabs_fft[i];
        ma = moving_avg(v, n, &ma_cx);
        if (i < n-1) continue;

        if (!found) {
            if (ma > threshold) {
                found = true;
                max = ma;
                idx_of_max = i;
                idx_start = i;
            }
        } else {
            if (ma > max) {
                max = ma;
                idx_of_max = i;
            }

            if (ma < threshold) {
                idx_end = i;
                bw = (idx_end - idx_start) *  (b->f_max - b->f_min) / (b->max_cabs_fft - 1);
                if (bw == 0) {
                    ERROR("bw %d %ld %d\n", (idx_end - idx_start), (b->f_max - b->f_min), b->max_cabs_fft);
                }

                idx_of_max -= n/2;
                f = b->f_min + idx_of_max * (b->f_max - b->f_min) / (b->max_cabs_fft - 1);

                // xxx snap the freq

                //NOTICE("FOUND %s f=%ld  max=%d  bw=%ld\n", b->name, f, (int)max, bw);

                add(b, f, bw);

                found = false;
            }
        }
    }

    free(ma_cx);
}

// return
//  0: success
// -1: scan_station table is full
// -2: duplicate, not added
static int add(band_t *b, freq_t f, freq_t bw)
{
    struct scan_station_s *ss = &b->scan_station[b->max_scan_station];
    int j;

    if (b->max_scan_station >= MAX_SCAN_STATION) {
        ERROR("ADD FULL\n");
        return -1;
    }

    for (j = 0; j < b->max_scan_station; j++) {
        struct scan_station_s *ss = &b->scan_station[j];
        if ((f >= ss->f - ss->bw/2) && (f <= ss->f + ss->bw/2)) {
            //NOTICE("DUP f=%ld\n", f);
            return -2;
        }
    }

    ss->f = f;
    ss->bw = bw;
    b->max_scan_station++;
    return 0;
}

static int compare(const void *a_arg, const void *b_arg)
{
    const struct scan_station_s *a = a_arg;
    const struct scan_station_s *b = b_arg;

    return a->f < b->f ? -1 : a->f == b->f ? 0 : 1;
}

// ---------------------------------------------------------------------

static void play(freq_t f, int secs, bool sim)//xxx secs not used
{
    #define VOLUME_SCALE 10

    sdr_async_rb_t *rb;
    complex         data, data_lpf;
    double          data_demod, data_demod_volscale;
    //int             max = secs * SDR_SAMPLE_RATE;

    if (play_time == 0) {
        return;
    }

    rb = malloc(sizeof(sdr_async_rb_t));
    sdr_read_async(f, rb, sim);

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
        if (rb->head >= play_time * SDR_SAMPLE_RATE) {
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
#endif
