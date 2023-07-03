#include "common.h"

#if 0
// xxx todo list
// - should log() be used
// - what LPF_ORDER TO use
// - can MAX_FFT_FREQ_SPAN be larger, say up to 2400000
// - at exit wait for threads

// more todo ?
// - display range of stations in the band being played
// - snap the freq ?
// - adjust play time
// - ctrl chars
//     skip to next
//     pause
//     continue

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
#endif

//
// defines
//

#define MAX_FFT_FREQ_SPAN  1200000  // must be <= SDR_SAMPLE_RATE/2
#define FFT_ELEMENT_HZ     100
#define FFT_N              (SDR_SAMPLE_RATE / FFT_ELEMENT_HZ)

#define THREAD_STATE_STOPPED         0
#define THREAD_STATE_RUNNING         1
#define THREAD_STATE_STOP_REQUESTED  2

_Static_assert(MAX_FFT_FREQ_SPAN <= SDR_SAMPLE_RATE/2);

//
// typedefs
//

typedef struct {
    void *(*proc)(void*);
    void *cx;
    char name[20];
} thread_cx_t;

//
// variables
//

static int     threads_running;
static bool    stop_threads_req;
static band_t *active_band;

//
// prototypes
//

static void exit_hndlr(void);
 
static band_t *get_active_band(void);
static void set_active_band(band_t *b) ATTRIB_UNUSED;
static void set_active_band_to_next(void) ATTRIB_UNUSED;
static void set_active_band_to_previous(void) ATTRIB_UNUSED;
static void set_band_selected(band_t *b) ATTRIB_UNUSED;
static void clear_band_selected(band_t *b) ATTRIB_UNUSED;

static void start_thread(void *(*proc)(void *cx), char *name);
static void stop_threads(void);
static void stop_other_threads(void);
static void *thread_wrapper(void *cx_arg);

static void *fft_mode_thread(void *cx);
static void *play_mode_thread(void *cx);
static void *scan_mode_thread(void *cx);

static void fft_entire_band(band_t *b);
static void add_to_waterfall(band_t *b);

static void player(void);
static void demod_and_audio_out(complex data_freq_shift);
static complex lpf(complex x, double f_cut);
static void downsample_and_audio_out(double x);

static void find_stations(band_t *b);
static void find(band_t *b, int avg_freq_span, double threshold);
static int add(band_t *b, freq_t f, freq_t bw);
static int compare(const void *a_arg, const void *b_arg);

// -----------------  INIT  --------------------------------------------

void radio_init(void)
{
    for (int i = 0; i < max_band; i++) {
        band_t *b = band[i];

        // determine size of cabs_fft array
        b->max_cabs_fft = b->f_span / FFT_ELEMENT_HZ;

        // allocate buffers
        b->fft_in   = fft_alloc_complex(FFT_N);
        b->fft_out  = fft_alloc_complex(FFT_N);
        b->cabs_fft = calloc(b->max_cabs_fft, sizeof(double));
        b->wf.data  = calloc(MAX_WATERFALL * b->max_cabs_fft, 1);
    }

    set_active_band(band[0]);

    scan_pause = false;
    scan_go_next = false;
    scan_go_prior = false;
    scan_intvl = 3;
    mode = MODE_FFT;
    start_thread(fft_mode_thread, "sdr_fft_mode");

    atexit(exit_hndlr);
}

static void exit_hndlr(void)
{
    NOTICE("%s exit_hndlr\n", __FILE__);

    stop_threads();
}

// -----------------  HANDLE EVENTS FROM DISPLAY  ----------------------

bool radio_event(sdl_event_t *ev)
{
    bool event_was_handled = true;
    freq_t tmp_freq;
    band_t *b;

    switch(ev->event_id) {
    case SDL_EVENT_KEY_F(1):
        stop_threads();
        mode = MODE_FFT;
        start_thread(fft_mode_thread, "sdr_fft_mode");
        break;
    case SDL_EVENT_KEY_F(2):
        stop_threads();
        mode = MODE_PLAY;
        start_thread(play_mode_thread, "sdr_play_mode");
        break;
    case SDL_EVENT_KEY_F(3):
        stop_threads();
        mode = MODE_SCAN;
        start_thread(scan_mode_thread, "sdr_scan_mode");
        break;
    case SDL_EVENT_KEY_F(4):
        stop_threads();
        mode = MODE_STOP;
        break;

    case SDL_EVENT_KEY_TAB:
        set_active_band_to_next();
        break;
    case SDL_EVENT_KEYMOD_CTRL | SDL_EVENT_KEY_TAB:
        set_active_band_to_previous();
        break;

    case SDL_EVENT_KEY_LEFT_ARROW:
        if ((b = get_active_band()) == NULL) break;
        if (mode == MODE_PLAY) {
            tmp_freq = b->f_play - 10000; //xxx step
            if (tmp_freq < b->f_min) tmp_freq = b->f_min;
            b->f_play = tmp_freq;
        } else if (mode == MODE_SCAN) {
            scan_go_prior = true;
            scan_pause = false;
        }
        break;
    case SDL_EVENT_KEY_RIGHT_ARROW:
        if ((b = get_active_band()) == NULL) break;
        if (mode == MODE_PLAY) {
            tmp_freq = b->f_play + 10000;  // xxx step
            if (tmp_freq < b->f_min) tmp_freq = b->f_min;
            b->f_play = tmp_freq;
        } else if (mode == MODE_SCAN) {
            scan_go_next = true;
            scan_pause = false;
        }
        break;
    case ' ':
        if (mode == MODE_SCAN) {
            scan_pause = true;
        }
        break;

    default:
        event_was_handled = false;
        break;
    }

    // return true if event was handled
    return event_was_handled;
}

// -----------------  ACTIVE AND SELECTED BAND SUPPORT  ----------------

static band_t *get_active_band(void)
{
    return active_band;
}

static void set_active_band(band_t *b)
{
    if (active_band) {
        active_band->active = false;
        active_band = NULL;
    }

    b->active = true;
    b->selected = true;
    active_band = b;
}

static void set_active_band_to_next(void)
{
    int i, idx;

    idx = active_band ? active_band->idx : -1;
    for (i = 0; i < max_band; i++) {
        if (++idx == max_band) idx = 0;
        if (band[idx]->selected) {
            set_active_band(band[idx]);
            return;
        }
    }

    set_active_band(band[0]);
}

static void set_active_band_to_previous(void)
{
    int i, idx;

    idx = active_band ? active_band->idx : max_band;
    for (i = 0; i < max_band; i++) {
        if (--idx == -1) idx = max_band - 1;
        if (band[idx]->selected) {
            set_active_band(band[idx]);
            return;
        }
    }

    set_active_band(band[0]);
}

static void set_band_selected(band_t *b)
{
    b->selected = true;
}

static void clear_band_selected(band_t *b)
{
    b->selected = false;

    if (b->active) {
        b->active = false;
        active_band = NULL;
    }
}

// -----------------  THREAD CONTROL  ----------------------------------

static void start_thread(void *(*proc)(void *cx), char *name)
{
    pthread_t tid;
    thread_cx_t *cx;
    int tmp;

    cx = malloc(sizeof(thread_cx_t));
    cx->proc = proc;
    cx->cx = NULL;
    snprintf(cx->name, sizeof(cx->name), "%s", name);

    pthread_create(&tid, NULL, thread_wrapper, cx);

    tmp = threads_running;
    while (threads_running == tmp) {
        usleep(1000);
    }
}

static void stop_threads(void)
{
    stop_threads_req = true;
    while (threads_running) {
        usleep(1000);
    }
    stop_threads_req = false;
}

static void stop_other_threads(void)
{
    stop_threads_req = true;
    while (threads_running > 1) {
        usleep(1000);
    }
    stop_threads_req = false;
}

static void *thread_wrapper(void *cx_arg)
{
    thread_cx_t *cx = cx_arg;

    NOTICE("starting thread %s\n", cx->name);
    __sync_fetch_and_add(&threads_running, 1);

    pthread_detach(pthread_self());
    pthread_setname_np(pthread_self(), cx->name);

    cx->proc(cx->cx);

    __sync_fetch_and_sub(&threads_running, 1);
    NOTICE("terminating thread %s\n", cx->name);

    free(cx);

    return NULL;
}

// -----------------  THREADS  -----------------------------------------

static void *fft_mode_thread(void *cx)
{
    while (true) {
        for (int i = 0; i < max_band; i++) {
            if (stop_threads_req) {
                return NULL;
            }

            if (band[i]->selected) {
                fft_entire_band(band[i]);
            }
        }
    }

    return NULL;
}

static void *play_mode_thread(void *cx)
{
    player();
    return NULL;
}

static void *scan_mode_thread(void *cx)
{
    int           bidx, sidx;
    band_t       *first;
    bool          go_next;
    unsigned long t;

    // clear max_scan_stations that may be remaining from earlier scans
    for (bidx = 0; bidx < max_band; bidx++) {
        band[bidx]->max_scan_station = 0;
    }

    // loop
    // - for all selected bands, perform fft and find band stations
    // - play all the found stations
    while (true) {
        // perform fft and find stations, on all selected bands
        NOTICE("ffting\n");
        first = NULL;
        for (bidx = 0; bidx < max_band; bidx++) {
            band_t *b = band[bidx];

            if (stop_threads_req) {
                return NULL;
            }

            if (b->selected) {
                fft_entire_band(b);

                find_stations(b);

                if (b->max_scan_station > 0 && !first) {
                    first = b;
                }
            }
        }

        // if no stations found then do fft again
        if (first == NULL) {
            NOTICE("no stations found\n");
            continue;
        }

        // start the play_mod_thread
        set_active_band(first);
        first->f_play = first->scan_station[0].f;
        start_thread(play_mode_thread, "sdr_play_mode");

        // loop until all found stations have been played
        bidx = first->idx;
        sidx = 0;
        while (true) {
            // play station identified by bidx,sidx
            band[bidx]->f_play = band[bidx]->scan_station[sidx].f;
            __sync_synchronize();
            set_active_band(band[bidx]);

            // wait for play interval to expire, or go_next/go_prior control
            t = 0;
            while (true) {
                if (stop_threads_req) {
                    return NULL;
                }
                if (scan_pause) {
                    usleep(10000);
                    continue;
                }
                if (scan_go_next) {
                    go_next = true;
                    scan_go_next = false;
                    break;
                }
                if (scan_go_prior) {
                    go_next = false;
                    scan_go_prior = false;
                    break;
                }
                usleep(10000);
                t += 10000;
                if (t > scan_intvl * 1000000L) {
                    go_next = true;
                    break;
                }
            }

            // adjust bidx,sidx to be the next/prior station
            if (go_next) {
                while (true) {
                    sidx++; 
                    if (sidx < band[bidx]->max_scan_station) {
                        break;
                    } else {
                        if (++bidx == max_band) bidx = 0;
                        sidx = -1;
                    }
                }
            } else {
                while (true) {
                    sidx--; 
                    if (sidx >= 0) {
                        break;
                    } else {
                        if (--bidx == -1) bidx = max_band-1;
                        sidx = band[bidx]->max_scan_station;
                    }
                }
            }

            // if back to first then break
            if (bidx == first->idx && sidx == 0) {
                break;
            }
        }

        // stop the play_mode_thread
        stop_other_threads();
    }

    return NULL;
}

// -----------------  FFT ENTIRE BAND  ---------------------------------

static void fft_entire_band(band_t *b)
{
    freq_t        ctr_freq, fft_freq_span;
    int           k1, k2, i, j, n, num_fft;
    unsigned long start, dur_sdr_read_sync=0, dur_fft=0, dur_cabs=0;

    // divide the band into fft intervals
    num_fft = (b->f_span + MAX_FFT_FREQ_SPAN - 1) / MAX_FFT_FREQ_SPAN;
    fft_freq_span = b->f_span / num_fft;
    display_print_debug_line("%s: num_fft=%d fft_freq_span=%ld max_cabs_fft=%d max_needed=%ld", 
                              b->name, num_fft, fft_freq_span,
                              b->max_cabs_fft,
                              (fft_freq_span / FFT_ELEMENT_HZ) * num_fft);

    ASSERT_MSG(fft_freq_span <= MAX_FFT_FREQ_SPAN, 
               "idx=%d fft_freq_span=%ld", 
               b->idx, fft_freq_span);
    ASSERT_MSG((fft_freq_span / FFT_ELEMENT_HZ) * num_fft <= b->max_cabs_fft,
               "idx=%d fft_freq_span=%ld num_fft=%d max_cabs_fft=%d",
               b->idx, fft_freq_span, num_fft, b->max_cabs_fft);

    // loop over the intervals
    ctr_freq = b->f_min + fft_freq_span/2;
    k1 = 0;
    for (i = 0; i < num_fft; i++) {
        // get a block of rtlsdr data at ctr_freq
        start = microsec_timer();
        sdr_set_ctr_freq(ctr_freq, b->sim);
        sdr_read_sync(b->fft_in, FFT_N, b->sim);
        dur_sdr_read_sync += (microsec_timer() - start);

        // run fft
        start = microsec_timer();
        fft_fwd_c2c(b->fft_in, b->fft_out, FFT_N);
        dur_fft += (microsec_timer() - start);

        // save the cabs of fft result
        start = microsec_timer();
        n = fft_freq_span / FFT_ELEMENT_HZ;
        k2 = FFT_N - n / 2;
        for (j = 0; j < n; j++) {
            b->cabs_fft[k1++] = cabs(b->fft_out[k2++]);
            if (k2 == FFT_N) k2 = 0;
        }
        dur_cabs += (microsec_timer() - start);

        // move to next interval in the band
        ctr_freq += fft_freq_span;
    }

    //xxx may not always work ?
    ASSERT_MSG(k1 == b->max_cabs_fft, "idx=%d k1=%d max_kabs_fft=%d",
               b->idx, k1, b->max_cabs_fft);

    // save new waterfall entry
    add_to_waterfall(b);

    // print timing stats
    DEBUG("dur_sdr_read_sync = %ld us   %ld ms\n", dur_sdr_read_sync, dur_sdr_read_sync/1000);
    DEBUG("dur_fft           = %ld us   %ld ms\n", dur_fft, dur_fft/1000);
    DEBUG("dur_cabs          = %ld us   %ld ms\n", dur_cabs, dur_cabs/1000);

    display_clear_debug_line();
}

static void add_to_waterfall(band_t *b)
{
    unsigned char *wf;
    int i;

    wf = get_waterfall(b, -1);
    for (i = 0; i < b->max_cabs_fft; i++) {
        wf[i] = b->cabs_fft[i] ?  1 + b->cabs_fft[i] * (256. / 60000) : 0;
    }

    b->wf.num++;
}

// -----------------  PLAYER - PLAY AUDIO & FFT CONCURRENTLY  ----------

static void player(void)
{
    freq_t          last_play_freq = 0;
    freq_t          last_ctr_freq  = 0;
    band_t         *last_actv_band = NULL;
    freq_t          ctr_freq       = 0;
    double          offset_w       = 0;
    band_t         *b              = NULL;
    freq_t          play_freq      = 0;
    int             fft_pause      = 0;
    int             fft_n          = 0;
    double          t              = 0;
    bool            sdr_started    = false;
    sdr_async_rb_t *rb             = NULL;

    rb = malloc(sizeof(sdr_async_rb_t));

    while (true) {
        if (stop_threads_req) {
            goto terminate;
        }

        // get active_band and play_freq
        b = get_active_band();
        if (b == NULL) {
            usleep(1000);
            continue;
        }
        play_freq = b->f_play;
        ASSERT_MSG(play_freq >= b->f_min && play_freq <= b->f_max, "idx=%d play_freq=%ld", b->idx, play_freq);

        // if band has changed or play_freq has changed then
        //   determine new ctr_freq, and offset freq
        //   if ctr_freq has changed
        //     start rtlsdr async reader, and/or change rtlsdr ctr_freq
        //   endif
        // endif
        if (b != last_actv_band || play_freq != last_play_freq) {
            if ((b->f_span) <= MAX_FFT_FREQ_SPAN) {
                ctr_freq = (b->f_max + b->f_min) / 2;
            } else if (play_freq - MAX_FFT_FREQ_SPAN/2 < b->f_min) {
                ctr_freq = b->f_min + MAX_FFT_FREQ_SPAN/2;
            } else if (play_freq + MAX_FFT_FREQ_SPAN/2 >= b->f_max) {
                ctr_freq = b->f_max - MAX_FFT_FREQ_SPAN/2;
            } else {
                ctr_freq = play_freq;
            }
            offset_w = TWO_PI * (play_freq - ctr_freq);

            b->f_play_fft_min = ctr_freq - MAX_FFT_FREQ_SPAN / 2;
            b->f_play_fft_max = ctr_freq + MAX_FFT_FREQ_SPAN / 2;
            if (b->f_play_fft_min < b->f_min) b->f_play_fft_min = b->f_min;
            if (b->f_play_fft_max >= b->f_max) b->f_play_fft_max = b->f_max;

            display_print_debug_line("%s  FREQ - PLAY = %ld  CTR = %ld  OFFSET = %.0f", 
                                     b->name, play_freq, ctr_freq, offset_w/TWO_PI);

            if (ctr_freq != last_ctr_freq) {
                sdr_set_ctr_freq(ctr_freq, b->sim);
                if (!sdr_started) {
                    sdr_read_async(rb, b->sim);
                    sdr_started = true;
                }
                fft_pause = 300000;
            }

            last_ctr_freq = ctr_freq;
            last_play_freq = play_freq;
            last_actv_band = b;
        }

        // if no rtlsdr data avail then continue
        if (rb->head == rb->tail) {
            usleep(1000);
            continue;
        }

        // get rtlsdr data item from the ring buffer
        complex data, data_freq_shift;
        data = rb->data[rb->head % MAX_SDR_ASYNC_RB_DATA];

        // shift frequency
        if (offset_w) {
            data_freq_shift = data * cexp(-I * offset_w * t);
        } else {
            data_freq_shift = data;
        }

        // process the rtlsdr data item
        demod_and_audio_out(data_freq_shift);
        
        // if fft is paused, becuase ctr freq was changed then 
        //   decrement the pause counter
        // else
        //   add data item to the fft input buffer, and
        //   if the input buffer is full then perform the fft
        // endif
        if (fft_pause) {
            fft_pause--;
            fft_n = 0;
        }  else {
            // add data item to the fft input buffer
            b->fft_in[fft_n++] = data;

            // if fft input buffer is full then 
            //   perform the fft
            //   copy the fft result to the appropriate section of cabs_fft buffer
            //   add it to the waterfall
            // endif
            if (fft_n == FFT_N) {
                int k1, k2, n, i;

                fft_fwd_c2c(b->fft_in, b->fft_out, FFT_N);
                fft_n = 0;

                k1 = (b->f_play_fft_min - b->f_min) / FFT_ELEMENT_HZ;
                ASSERT_MSG(k1 >= 0, "k1=%d", k1);
                n = (b->f_play_fft_max - b->f_play_fft_min) / FFT_ELEMENT_HZ;
                k2 = FFT_N - n / 2;

                for (i = 0; i < n; i++) {
                    b->cabs_fft[k1++] = cabs(b->fft_out[k2++]);
                    if (k2 == FFT_N) k2 = 0;
                }

                add_to_waterfall(b);
            }
        }

        // done with this rtlsdr data item
        rb->head++;
        t += (1. / SDR_SAMPLE_RATE);
    }

terminate:
    display_clear_debug_line();
    b->f_play_fft_min = 0;
    b->f_play_fft_max = 0;
    if (sdr_started) {
        sdr_cancel_async(b->sim);
    }
    free(rb);
}

static void demod_and_audio_out(complex data_freq_shift)
{
    #define VOLUME_SCALE 10

    complex data_lpf;
    double  data_demod, data_demod_volscale;

    data_lpf = lpf(data_freq_shift, 4000);
    data_demod = cabs(data_lpf);
    data_demod_volscale = data_demod * VOLUME_SCALE;
    downsample_and_audio_out(data_demod_volscale);
}

static complex lpf(complex x, double f_cut)
{
    static double curr_f_cut;
    static BWLowPass *bwi, *bwq;

    #define LPF_ORDER  10

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

// -----------------  FIND STATIONS BY SCANNING FFT  -------------------------

static void find_stations(band_t *b)
{
    b->max_scan_station = 0;
    find(b, 50000,  500);  // xxx use squelch
    find(b,  1000,  500);
    qsort(b->scan_station, b->max_scan_station, sizeof(struct scan_station_s), compare);

    if (0) {
        NOTICE("find_station results: cnt=%d\n", b->max_scan_station);
        for (j = 0; j < b->max_scan_station; j++) {
            struct scan_station_s *ss = &b->scan_station[j];
            NOTICE("  f=%ld  bw=%ld\n", ss->f, ss->bw);
        }
    }
}

static void find(band_t *b, int avg_freq_span, double threshold)
{
    void   *ma_cx = NULL;
    bool   found  = false;
    double v, ma, max=0;
    int    i, idx_of_max=0, idx_start=0, idx_end, n;
    freq_t f, bw;

    n = avg_freq_span / FFT_ELEMENT_HZ;
    n |= 1;

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
                bw = (idx_end - idx_start) *  b->f_span / (b->max_cabs_fft - 1);
                if (bw == 0) {
                    ERROR("bw %d %ld %d\n", (idx_end - idx_start), b->f_span, b->max_cabs_fft);
                }

                idx_of_max -= n/2;
                f = b->f_min + idx_of_max * b->f_span / (b->max_cabs_fft - 1);
                // xxx snap the freq

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
        ERROR("%s band, scan_station tbl is full\n", b->name);
        return -1;
    }

    for (j = 0; j < b->max_scan_station; j++) {
        struct scan_station_s *ss = &b->scan_station[j];
        if ((f >= ss->f - ss->bw/2) && (f <= ss->f + ss->bw/2)) {
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

