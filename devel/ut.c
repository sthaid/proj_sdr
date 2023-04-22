// xxx later
// - comments
// - exit pgm
// - better way to handle global args

// xxx improve filter quality
// xxx init_fft?

#include "common.h"

//
// defines
//

#define min(a,b) ((a) < (b) ? (a) : (b))

#define MAX_TESTS (sizeof(tests)/sizeof(tests[0]))

// xxx yv min/max for complex
//    maybe new PLOT_COMPLEX

// xxx new plot for FFT
// xxx use routine not macro
#define PLOT(_idx,_is_complex,_data,_n,_xvmin,_xvmax,_yvmin,_yvmax,_title) \
    do { \
        plots_t *p = &plots[_idx]; \
        pthread_mutex_lock(&mutex); \
        if (!(_is_complex)) { \
            memcpy(p->data, _data, (_n)*sizeof(double)); \
        } else { \
            complex *data_complex = (complex*)(_data); \
            for (int i = 0; i < (_n); i++) { \
                p->data[i] = cabs(data_complex[i]); \
            } \
            normalize(p->data, _n, 0, 1); \
        } \
        p->n       = _n; \
        p->xv_min  = _xvmin; \
        p->xv_max  = _xvmax; \
        p->yv_min  = _yvmin; \
        p->yv_max  = _yvmax; \
        p->title   = _title; \
        pthread_mutex_unlock(&mutex); \
    } while (0)

//
// typedefs
//

#define MAX_PLOT_DATA  250000

typedef struct {
    double  data[MAX_PLOT_DATA];
    int     n;
    double  xv_min;
    double  xv_max;
    double  yv_min;
    double  yv_max;
    unsigned int flags;
    char   *title;
    char   *x_units;
} plots_t;

typedef struct {
    char *name;
    char *info;
    struct test_ctrl_s {
        char *name;
        double *val, min, max, step;
        char *val_enum_names[10];
        char *units;
        int decr_event;
        int incr_event;
    } ctrl[6];
} test_ctrl_t;

//
// variables
//

char *progname = "ut";

char *arg1_str, *arg2_str, *arg3_str;
int   arg1=-1, arg2=-1, arg3=-1;

test_ctrl_t tc;

plots_t plots[MAX_PLOT];

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

//
// prototypes
//

void usage(void);
int pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event);

void *plot_test(void *cx);
void *bpf_test(void *cx);
void *lpf_test(void *cx);
void *audio_test(void *cx);
void *gen_test(void *cx);
void *rx_test(void *cx);

//
// test table
//

static struct test_s {
    char *test_name;
    void *(*proc)(void *cx);
} tests[] = {
        { "plot",   plot_test  },
        { "bpf",    bpf_test   },
        { "lpf",    lpf_test   },
        { "audio",  audio_test },
        //{ "gen",    gen_test   },
        //{ "rx",     rx_test    },
                };
// xxx
// - rx sim
// - rx rtlsdr file
// - rx rtlsdr direct
// - rx rtlsdr server

// -----------------  MAIN  --------------------------------

int main(int argc, char **argv)
{
    int i;
    struct test_s *t;
    pthread_t tid;
    char *test_name;

    init_fft();  // xxx is this needed, elim later

    if (argc == 1) {
        usage();
        return 1;
    }
    test_name = argv[1];
    if (argc > 2) { arg1_str = argv[2]; sscanf(arg1_str, "%d", &arg1); }
    if (argc > 3) { arg2_str = argv[3]; sscanf(arg2_str, "%d", &arg2); }
    if (argc > 4) { arg3_str = argv[4]; sscanf(arg3_str, "%d", &arg3); }
    
    for (i = 0; i < MAX_TESTS; i++) {
        t = &tests[i];
        if (strcmp(test_name, t->test_name) == 0) {
            break;
        }
    }
    if (i == MAX_TESTS) {
        FATAL("test '%s' not found\n", test_name);
    }

    NOTICE("Running '%s' test\n", t->test_name);
    pthread_create(&tid, NULL, t->proc, NULL);

    // init sdl
    static int win_width = 1600;
    static int win_height = 800;
    if (sdl_init(&win_width, &win_height, false, false, false) < 0) {
        FATAL("sdl_init %dx%d failed\n", win_width, win_height);
        return 1;
    }
    sdl_pane_manager(
        NULL,           // context
        NULL,           // called prior to pane handlers
        NULL,           // called after pane handlers
        100000,          // 0=continuous, -1=never, else us
        1,              // number of pane handler varargs that follow
        pane_hndlr, NULL, 0, 0, win_width, win_height, PANE_BORDER_STYLE_NONE);
}

void usage(void)
{
    int i;
    struct test_s *t;

    NOTICE("tests:\n");
    for (i = 0; i < MAX_TESTS; i++) {
        t = &tests[i];
        NOTICE("  %s\n", t->test_name);
    }
}

int pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event)
{
    rect_t *pane = &pane_cx->pane;

    #define FONTSZ 20

    // ----------------------------
    // -------- INITIALIZE --------
    // ----------------------------

    if (request == PANE_HANDLER_REQ_INITIALIZE) {
        NOTICE("char width %d\n", COL2X(1,FONTSZ));
        return PANE_HANDLER_RET_NO_ACTION;
    }

    // ------------------------
    // -------- RENDER --------
    // ------------------------

    if (request == PANE_HANDLER_REQ_RENDER) {
        char str[100], *p;

        pthread_mutex_lock(&mutex);
        for (int i = 0; i < MAX_PLOT; i++) {
            plots_t *p = &plots[i];
            if (p->title) {
                sdl_plot(pane, i,
                        p->data, p->n,
                        p->xv_min, p->xv_max,
                        p->yv_min, p->yv_max,
                        p->flags, p->title, p->x_units);
            }
        }
        pthread_mutex_unlock(&mutex);  // sep mutex for each xxx

        p = str;
        p += sprintf(p, "%s", tc.name);
        if (tc.info[0] != '\0') {
            p += sprintf(p, " - %s\n", tc.info);
        }
        sdl_render_printf(pane, 
                          0, pane->h-ROW2Y(4,FONTSZ), FONTSZ,
                          SDL_WHITE, SDL_BLACK, "%s" , str);

        for (int i = 0; i < 6; i++) {
            struct test_ctrl_s *x = &tc.ctrl[i];
            if (x->val == NULL) continue;

            p = str;
            if (x->name) {
                p += sprintf(p, "%s ", x->name);
            }
            if (x->val_enum_names[0] != NULL) {
                int tmp = nearbyint(*x->val);
                if (tmp < 0 || tmp >= 10 || x->val_enum_names[tmp][0] == '\0') {  // xxx define for 10
                    p += sprintf(p, "%d ", tmp);
                } else {
                    p += sprintf(p, "%s ", x->val_enum_names[tmp]);
                }
            } else {
                p += sprintf(p, "%g ", *x->val);
            }
            if (x->units) {
                p += sprintf(p, "%s", x->units);
            }

            sdl_render_printf(pane, 
                              COL2X(20*i,FONTSZ), pane->h-ROW2Y(2,FONTSZ), FONTSZ,
                              SDL_WHITE, SDL_BLACK, "%s" , str);
        }

        for (int i = 0; i < 6; i++) {
            struct test_ctrl_s *x = &tc.ctrl[i];
            if (x->val == NULL) continue;

            str[0] = '\0';
            if (x->decr_event == SDL_EVENT_KEY_UP_ARROW && x->incr_event == SDL_EVENT_KEY_DOWN_ARROW) {
                sprintf(str, "^ v");
            } else if (x->decr_event == SDL_EVENT_KEY_LEFT_ARROW && x->incr_event == SDL_EVENT_KEY_RIGHT_ARROW) {
                sprintf(str, "<- ->");
            } else if (x->decr_event == SDL_EVENT_KEY_SHIFT_LEFT_ARROW && x->incr_event == SDL_EVENT_KEY_SHIFT_RIGHT_ARROW) {
                sprintf(str, "SHIFT <- ->");
            } //xxx add SHIFT and CTRL and ALT

            sdl_render_printf(pane, 
                              COL2X(20*i,FONTSZ), pane->h-ROW2Y(1,FONTSZ), FONTSZ,
                              SDL_WHITE, SDL_BLACK, "%s" , str);
        }
    
        return PANE_HANDLER_RET_NO_ACTION;
    }

    // -----------------------
    // -------- EVENT --------
    // -----------------------

    if (request == PANE_HANDLER_REQ_EVENT) {
        // loop over test controls to find matching event
        for (int i = 0; i < 6; i++) {
            struct test_ctrl_s *x = &tc.ctrl[i];
            double tmp;

            if (x->val == NULL) continue;

            if (x->incr_event == event->event_id) {
                tmp = *x->val + x->step;
                if (tmp > x->max) tmp = x->min;
                *x->val = tmp;
                break;
            }

            if (x->decr_event == event->event_id) {
                tmp = *x->val - x->step;
                if (tmp < x->min) tmp = x->max;
                *x->val = tmp;
                break;
            }
        }

        return PANE_HANDLER_RET_NO_ACTION;
    }

    // ---------------------------
    // -------- TERMINATE --------
    // ---------------------------

    if (request == PANE_HANDLER_REQ_TERMINATE) {
        return PANE_HANDLER_RET_NO_ACTION;
    }

    // not reached
    printf("FATAL: not reached\n");
    exit(1);
    return PANE_HANDLER_RET_NO_ACTION;
}

// -----------------  UTILS  -------------------------------

void zero_real(double *data, int n)
{
    memset(data, 0, n*sizeof(double));
}

void zero_complex(complex *data, int n)
{
    memset(data, 0, n*sizeof(complex));
}

// xxx allow 0 hz
void init_using_sine_waves(double *sw, int n, int freq_first, int freq_last, int freq_step, int sample_rate)
{
    int f;

    zero_real(sw,n);
    for (f = freq_first; f <= freq_last; f+= freq_step) {
        double  w = TWO_PI * f;
        double  t = 0;
        double  dt = (1. / sample_rate);

        for (int i = 0; i < n; i++) {
            sw[i] += (w ? sin(w * t) : 0.5);
            t += dt;
        }

        if (freq_step == 0) break;
    }
}

void init_using_white_noise(double *wn, int n)
{
    for (int i = 0; i < n; i++) {
        wn[i] = ((double)random() / RAND_MAX) - 0.5;
    }
}

void init_using_wav_file(double *wav, int n, char *filename)
{
    int ret, num_chan, num_items, sample_rate;
    double *data;

    ret = read_wav_file(filename, &data, &num_chan, &num_items, &sample_rate);
    if (ret != 0 || num_chan != 1) {
        FATAL("read_wav_file ret=%d num_chan=%d\n", ret, num_chan);
    }
    // xxx also fatal on sample rate

    zero_real(wav,n);
    for (int i = 0; i < min(n,num_items); i++) {
        wav[i] = data[i];
    }

    free(data);
}

#if 0
// xxx used?
void add_sine_wave(int sample_rate, int f, int n, double *data)
{
}

#endif

// xxx support multiple chan
void read_and_filter_wav_file(char *filename, double **data_arg, int *num_items_arg, int *sample_rate_arg, int lpf_freq)
{
    int ret;
    double *data;
    int num_items, sample_rate, num_chan;

    ret = read_wav_file(filename, &data, &num_chan, &num_items, &sample_rate);
    if (ret != 0 || num_chan != 1) {
        FATAL("read_wav_file ret=%d num_chan=%d\n", ret, num_chan);
    }
    NOTICE("read_wav_file num_chan=%d num_items=%d sample_rate=%d\n", num_chan, num_items, sample_rate);

    fft_lpf_real(data, data, num_items, sample_rate, lpf_freq);
    normalize(data, num_items, -1, 1);

    *data_arg        = data;
    *num_items_arg   = num_items;
    *sample_rate_arg = sample_rate;
}

void plot_real(int idx, double *data, int n, double xvmin, double xvmax, double yvmin, double yvmax, char *title)
{
    plots_t *p = &plots[idx];

    if (n > MAX_PLOT_DATA) {
        FATAL("plot n=%d\n", n);
    }

    pthread_mutex_lock(&mutex);
    for (int i = 0; i < n; i++) {
        p->data[i] = data[i];
    }
    p->n       = n;
    p->xv_min  = xvmin;
    p->xv_max  = xvmax;
    p->yv_min  = yvmin;
    p->yv_max  = yvmax;
    p->title   = title;
    pthread_mutex_unlock(&mutex);
}

void plot_complex(int idx, complex *data, int n, double xvmin, double xvmax, double yvmin, double yvmax, 
                  char *title, char *x_units)
{
    plots_t *p = &plots[idx];

    if (n > MAX_PLOT_DATA) {
        FATAL("plot n=%d\n", n);
    }

    pthread_mutex_lock(&mutex);
    for (int i = 0; i < n; i++) {
        p->data[i] = creal(data[i]);
    }
    p->n       = n;
    p->xv_min  = xvmin;
    p->xv_max  = xvmax;
    p->yv_min  = yvmin;
    p->yv_max  = yvmax;
    p->flags   = 0;
    p->x_units = x_units;
    p->title   = title;
    pthread_mutex_unlock(&mutex);
}

// xxx ctr on the frequency
void plot_fft(int idx, complex *fft, int n, double max_freq, char *title, char *x_units)
{
    plots_t *p = &plots[idx];
    int j=0;

    if (n > MAX_PLOT_DATA) {
        FATAL("plot n=%d\n", n);
    }

    pthread_mutex_lock(&mutex);
    for (int i = n/2; i < n; i++) {
        p->data[j++] = cabs(fft[i]);
    }
    for (int i = 0; i < n/2; i++) {
        p->data[j++] = cabs(fft[i]);
    }
    if (j != n) {
        FATAL("n=%d j=%d\n", n, j);
    }
    normalize(p->data, n, 0, 1);
    p->n       = n;
    p->xv_min  = -max_freq/2;
    p->xv_max  = max_freq/2;
    p->yv_min  = 0;
    p->yv_max  = 1;
    p->flags   = SDL_PLOT_FLAG_BARS;
    p->x_units = x_units;
    p->title   = title;
    pthread_mutex_unlock(&mutex);
}

// -----------------  PLOT TEST  ---------------------------

void *plot_test(void *cx)
{
    int sample_rate, f, n;
    double *sw;

    #define SECS 1

    sample_rate = 24000;  // xxx use 22000
    f = 10;
    n = SECS * sample_rate;
    sw = fftw_alloc_real(n);

    init_using_sine_waves(sw, n, f, f, 0, sample_rate);

    plot_real(0, sw, n,  0, 1,  -1, +1,  "SINE WAVE");
    plot_real(1, sw, n,  0, 1,  -0.5, +0.5,  "SINE WAVE");
    plot_real(2, sw, n,  0, 1,  0.5, +1.5,  "SINE WAVE");

    return NULL;
}

// -----------------  LPF TEST  ---------------------------

void audio_out(double yo); // xxx move this to utils section above

void *bpf_test(void *cx)
{
    double *in_real;
    complex *in, *fft;
    int current_mode;

    int sample_rate = 20000;
    int max = 60 * sample_rate;
    int total = 0;

    #define SINE_WAVE   0
    #define SINE_WAVES  1
    #define WHITE_NOISE 2
    #define WAV_FILE    3

    static char tc_info[100];
    static double tc_mode;
    static double tc_ctr;
    static double tc_width = 3000;

    // init test controls
    tc.name = "BPF";
    tc.info = tc_info;
    tc.ctrl[0] = (struct test_ctrl_s)
                 {"MODE", &tc_mode, SINE_WAVE, WAV_FILE, 1, 
                  {"SINE_WAVE", "SINE_WAVES", "WHITE_NOISE", "WAV_FILE"}, "", 
                  SDL_EVENT_KEY_UP_ARROW, SDL_EVENT_KEY_DOWN_ARROW};
    tc.ctrl[1] = (struct test_ctrl_s)
                 {"CENTER", &tc_ctr, -10000, 10000, 100,
                  {}, "HZ", 
                  SDL_EVENT_KEY_LEFT_ARROW, SDL_EVENT_KEY_RIGHT_ARROW};
    tc.ctrl[2] = (struct test_ctrl_s)
                 {"WIDTH", &tc_width, 1000, 10000, 100,
                  {}, "HZ", 
                  SDL_EVENT_KEY_SHIFT_LEFT_ARROW, SDL_EVENT_KEY_SHIFT_RIGHT_ARROW};

    // alloc buffers
    in_real = fftw_alloc_real(max);
    in = fftw_alloc_complex(max);
    fft = fftw_alloc_complex(max);

    while (true) {
        // construct in buff from either:
        // - sine waves
        // - white noise
        // - wav file
        current_mode = tc_mode;
        switch (current_mode) {
        case SINE_WAVE:
            init_using_sine_waves(in_real, max, 1000, 1000, 0, sample_rate);
            break;
        case SINE_WAVES:
            init_using_sine_waves(in_real, max, 0, 10000, 1000, sample_rate);
            break;
        case WHITE_NOISE:
            init_using_white_noise(in_real, max);
            break;
        case WAV_FILE:
            init_using_wav_file(in_real, max, "super_critical.wav");
            break;
        default:
            break;
        }

        // xx
        //normalize(in_real, max, -1, 1);

        while (current_mode == tc_mode) {
            // copy .01 secs of data from in_real to in buff
            int n = .01 * sample_rate;
            for (int i = 0; i < n; i++) {
                in[i] = in_real[(total+i)%max];
            }
            total += n;

            // plot the in buff, and fft of in buff
            plot_complex(0, in, n, 0, .01, -2, 2, "SINE_WAVES", "SECS");  // xxx title and units?
            fft_fwd_c2c(in, fft, n);
            plot_fft(1, fft, n, n/.01, "FFT", "HZ");

            // apply filter to in buff
            fft_bpf_complex(in, in, n, sample_rate, tc_ctr-tc_width/2, tc_ctr+tc_width/2);

            // plot the filtered in buff, and fft of the filtered in buff
            plot_complex(2, in, n, 0, .01, -2, 2, "SINE_WAVES", "SECS");  // xxx title and units?
            fft_fwd_c2c(in, fft, n);
            plot_fft(3, fft, n, n/.01, "FFT", "HZ");

            // play the filtered audio
            for (int i = 0; i < n; i++) {
                audio_out(in[i]);
            }
        }
    }        
}

// xxx bandpass filter test too
// xxx filter choices
// - sine waves
// - white noise
// - audio file
void *lpf_test(void *cx)
{
    int sample_rate, n;
    double *in, *in_fft;
    double *lpf, *lpf_fft;
    complex *in_complex, *in_fft_complex;
    complex *lpf_complex, *lpf_fft_complex;

    #define SECS 1

    sample_rate = 24000;
    n = SECS * sample_rate;

    // allocate buffers
    in      = fftw_alloc_real(n);
    in_fft  = fftw_alloc_real(n);
    lpf     = fftw_alloc_real(n);
    lpf_fft = fftw_alloc_real(n);

    in_complex = fftw_alloc_complex(n);
    in_fft_complex = fftw_alloc_complex(n);
    lpf_complex = fftw_alloc_complex(n);
    lpf_fft_complex = fftw_alloc_complex(n);

    // create sum of sine waves, to 'in' and 'in_complex' buffs
    init_using_sine_waves(in, n, 990, 12000, 1000, sample_rate);
    normalize(in, n, -1, 1);
    for (int i = 0; i < n; i++) {
        in_complex[i] = in[i];
    }

    // perfrom fwd fft on 'in' to 'in_fft', and plot
    fft_fwd_r2r(in, in_fft, n);
    normalize(in_fft, n, 0, 1);
    PLOT(0, false, in_fft, n,  0, n,  0, +1,  "IN_FFT");

    // perform lpf on 'in' to 'lpf', and plot the fft
    fft_lpf_real(in, lpf, n, sample_rate, 1500);
    fft_fwd_r2r(lpf, lpf_fft, n);
    normalize(lpf_fft, n, 0, 1);
    PLOT(1, false, lpf_fft, n,  0, n,  0, +1,  "LPF_FFT");

    // perfrom fwd fft on 'in_complex' to 'in_fft_complex', and plot
    fft_fwd_c2c(in_complex, in_fft_complex, n);
    PLOT(2, true, in_fft_complex, n,  0, n,  0, +1,  "IN_FFT_COMPLEX");

    // perform lpf on 'in_complex' to 'lpf_complex' and plot the fft
    fft_lpf_complex(in_complex, lpf_complex, n, sample_rate, 1500);
    fft_fwd_c2c(lpf_complex, lpf_fft_complex, n);
    PLOT(3, true, lpf_fft_complex, n,  0, n,  0, +1,  "LPF_FFT_COMPLEX");

    return NULL;
}

// -----------------  AUDIO TEST  ---------------------------

// xxx combine with prior
void *audio_test(void *cx)
{
    double *data;
    int num_items, sample_rate, n, idx;
    double *in, *out, *out_fft;

    // read wav file
    read_and_filter_wav_file("super_critical.wav", &data, &num_items, &sample_rate, 4000);

#if 0
    // init lpf_freq control
    tc.int_name = "LPF_FREQ";
    tc.int_val  = 4000;
    tc.int_step = 100;
    tc.int_min  = 500;
    tc.int_max  = 10000;
#endif

    // xxx
    n = sample_rate / 10;
    float audio_out[n];
    idx = 0;

    // alloc buffers
    in = fftw_alloc_real(n);
    out = fftw_alloc_real(n);
    out_fft = fftw_alloc_real(n);

    // loop forever, writing wav file audio to stdout
    while (true) {
        memcpy(in, data+idx, n*sizeof(double));

        fft_lpf_real(in, out, n, sample_rate, 12345l);  //xxx
        for (int i = 0; i < n; i++) {
            out[i] /= n;
        }

        fft_fwd_r2r(out, out_fft, n);
        normalize(out_fft, n, 0, 1);
        PLOT(0, false, out_fft, n,  0, sample_rate,  0, 1, "XXX");

        // xxx call routine
        for (int i = 0; i < n; i++) {
            audio_out[i] = out[i];
        }
        fwrite(audio_out, sizeof(float), n, stdout);

        idx += n;
        if (idx + n >= num_items) {
            idx = 0;
        }
    }

    return NULL;
}

#if 0
// -----------------  GEN TEST  -----------------------------

// xxx won't be needed, I hope

// xxx dont use all these defines
#define SAMPLE_RATE  2400000   // 2.4 MS/sec
#define F_SYNTH       600000   // 600 KHz
#define F_LPF        (SAMPLE_RATE / 4)
#define MAX_IQ       (SAMPLE_RATE / 10)   // 0.1 sec range
#define DELTA_T      (1. / SAMPLE_RATE)
#define W_SYNTH      (TWO_PI * F_SYNTH)

void *gen_test(void *cx)
{
    complex *iq, *iq_fft;
    double   t, max_iq_scaling, max_iq_measured=0;
    double   *antenna, synth0, synth90;
    int      i=0, tmax;
    FILE    *fp;

    static unsigned char usb[2*MAX_IQ];

    tmax = (arg1 == -1 ? 30 : arg1);
    max_iq_scaling = (arg2 <= 0 ? 1500000 : arg2);

    init_antenna();

    iq     = fftw_alloc_complex(MAX_IQ);
    iq_fft = fftw_alloc_complex(MAX_IQ);
    antenna = fftw_alloc_real(MAX_IQ);

    fp = fopen("gen.dat", "w");
    if (fp == NULL) {
        FATAL("failed to create gen.dat\n");
    }

    NOTICE("generating %d secs of data, max_iq_scaling=%0.2f\n", tmax, max_iq_scaling);

    for (t = 0; t < tmax; t += DELTA_T) {
        antenna[i] = get_antenna(t);
        synth0     = sin((W_SYNTH) * t);
        synth90    = sin((W_SYNTH) * t + M_PI_2);
        iq[i]      = antenna[i] * (synth0 + I * synth90);
        i++;

        if (i == MAX_IQ) {
            sprintf(tc.title, "gen %0.1f secs", t);

            fft_lpf_complex(iq, iq, MAX_IQ, SAMPLE_RATE, F_LPF);

            // plots
            PLOT(0, false, antenna, MAX_IQ,  0, 999,  -2, 2,  "ANTENNA");  // x scale
            fft_fwd_c2c(iq, iq_fft, MAX_IQ);
            PLOT(1, true, iq_fft, MAX_IQ,  0, 999,  0, 1,  "IQ_FFT");

            int offset = MAX_IQ/12;
            int span  = (long)24000 * MAX_IQ / SAMPLE_RATE;
            PLOT(2, true, iq_fft+(offset-span/2), span,  0, 999,  0, 1,  "IQ_FFT");
            // xxx ctr plot3
            PLOT(3, true, iq_fft, span,  0, 999,  0, 1,  "IQ_FFT");

            double scaling = 128. / max_iq_scaling;
            for (int j = 0; j < MAX_IQ; j++) {
                if (cabs(iq[j]) > max_iq_measured) max_iq_measured = cabs(iq[j]);

                int inphase    = nearbyint(128 + scaling * creal(iq[j]));
                int quadrature = nearbyint(128 + scaling * cimag(iq[j]));
                if (inphase < 0 || inphase > 255 || quadrature < 0 || quadrature > 255) {
                    static int count=0;
                    if (count++ < 10) {
                        ERROR("iq val too large %d %d", inphase, quadrature);
                    }
                }

                usb[2*j+0] = inphase;
                usb[2*j+1] = quadrature;
                DEBUG("%u %u\n", usb[2*j+0], usb[2*j+1]);
            }

            fwrite(usb, sizeof(unsigned char), MAX_IQ*2, fp);

            i = 0;
        }
    }

    fclose(fp);

    NOTICE("max_iq_measured = %f\n", max_iq_measured);
    NOTICE("max_iq_scaling  = %f\n", max_iq_scaling);
    if (max_iq_measured > max_iq_scaling) {
        ERROR("max_iq_scaling is too small\n");
    }

    return NULL;
}

// -----------------  RX TEST  -----------------------------

// reference sim/sim.c

// xxx
// - start / stop ctrls

// xxx new data block format
// - hdr
//   - maic, freq, gain, ...
// - data
//   - complex array

#define BLOCK_SIZE 262144
#define MAX_DATA (4*BLOCK_SIZE)

unsigned int sample_rate = 2400000;  // xxx make this private for each section

unsigned long head;
unsigned long tail;
unsigned char data[MAX_DATA];  // xxx rename to Data ?

void init_get_data(void);
void *get_data_from_file_thread(void *cx);

void *rx_test(void *cx)
{
    struct {
        unsigned char i;
        unsigned char q;
    } *d;

    complex *dc;
    int n;
    double yo=0, y;
    int cnt=0;

    init_get_data();

    dc = fftw_alloc_complex(BLOCK_SIZE/2);

    tc.int_name = "FREQ";
    tc.int_val  = 0;
    tc.int_step = 1000;
    tc.int_min  = -220000;
    tc.int_max  = +220000;

    while (true) {
        // wait for data
        while (head == tail) {
            usleep(1000);
        }

        d = (void*)&data[head%MAX_DATA];
        n = BLOCK_SIZE/2;
        //NOTICE("GOT DATA\n");

// xxx not needed
        for (int i = 0; i < n; i++) {
            dc[i] = ((d[i].i - 128) / 128.) + 
                    ((d[i].q - 128) / 128.) * I;
        }

        // process the usb data ...

        int freq = tc.int_val;
        // low pass filter
        //fft_lpf_complex(dc, dc, n, sample_rate, 4000);
        //fft_bpf_complex(dc, dc, n, sample_rate, 200000-5000, 200000+5000);
        fft_bpf_complex(dc, dc, n, sample_rate, freq-4000, freq+4000);

        for (int i = 0; i < n; i++) {
            //y = creal(dc[i]) * 5e-5;
            y = creal(dc[i]) / 5000;

            if (y > yo) {
                yo = y;
            }
            yo = .9995 * yo;

            if (cnt++ == (sample_rate / 22000)) {
                audio_out(yo);
                cnt = 0;
            }
        }

        __sync_synchronize();
        head += BLOCK_SIZE;
        __sync_synchronize();
    }
}
#endif

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

// xxx handle the differnet modes

#if 0
void init_get_data(void)
{
    pthread_t tid;

    // will support getting data from:
    // - file
    // - tcp connection to rtl_sdr_server
    // - directly from rtl_sdr device

    // for now, get data from gen.dat
    pthread_create(&tid, NULL, get_data_from_file_thread, NULL);
}

// xxx this won't be from file, it will just construct the data on the fly

void *get_data_from_file_thread(void *cx)
{
    int fd;
    unsigned long t_read, t_next_read, t_delay;;
    struct stat statbuf;
    size_t file_size, file_offset, len;

    //unsigned long t_start, total=0;

    fd = open("gen.dat", O_RDONLY);
    if (fd < 0) {
        FATAL("failed open gen.dat, %s\n", strerror(errno));
    }

    fstat(fd, &statbuf);
    file_size = statbuf.st_size;
    file_offset = 0;

    //t_start = microsec_timer();

    t_read = microsec_timer();
    while (true) {
        // if there is room in data buffer then read from file
        if ((MAX_DATA - (tail - head)) >= BLOCK_SIZE) {
            len = pread(fd, data+(tail%MAX_DATA), BLOCK_SIZE, file_offset);
            if (len != BLOCK_SIZE) {
                FATAL("read gen.dat, len=%ld, %s\n", len, strerror(errno));
            }

            //NOTICE("put data\n");

            __sync_synchronize();
            tail += BLOCK_SIZE;
            __sync_synchronize();

            file_offset += BLOCK_SIZE;
            if (file_offset + BLOCK_SIZE > file_size) {
                file_offset = 0;
            }
        } else {
            ERROR("discarding\n");
        }

        // wait for time to do next read
        t_next_read = t_read + ((double)(BLOCK_SIZE/2) / sample_rate) * 1000000;
        t_delay = t_next_read - microsec_timer();
        if (t_delay > 0) {
            usleep(t_delay);
        }
        t_read = t_next_read;

        // debug print the read rate
        //total += BLOCK_SIZE;
        //NOTICE("rate = %e\n", (double)total/(microsec_timer() - t_start));
    }
}
#endif
