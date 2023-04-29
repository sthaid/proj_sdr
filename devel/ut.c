// xxx later
// - comments
// - exit pgm
// - better way to handle global args

// xxx improve filter quality
// xxx are the _sync_cynchronize needed

// xxx make plot screen locations flexible

// xxx also plot Bodie plots, in lpf_test

// xxx
// - rx sim
// - rx rtlsdr file
// - rx rtlsdr direct
// - rx rtlsdr server

// xxx check the fft code in ut_antenna.c

#include "common.h"

//
// defines
//

#define USE_PA

#define CTRL SDL_EVENT_KEY_CTRL
#define ALT  SDL_EVENT_KEY_ALT

#define MAX_PLOT       10
#define MAX_PLOT_DATA  250000

#define MAX_CTRL            6
#define MAX_CTRL_ENUM_NAMES 10

#define min(a,b) ((a) < (b) ? (a) : (b))

//
// typedefs
//

typedef struct {
    double  data[MAX_PLOT_DATA];
    int     n;
    double  xv_min;
    double  xv_max;
    double  xv_cursor;
    double  yv_min;
    double  yv_max;
    unsigned int flags;
    char   *title;
    char   *x_units;
    int    x_pos;
    int    y_pos;
    int    x_width;
    int    y_height;
} plots_t;

typedef struct {
    char name[100];
    char info[100];
    struct test_ctrl_s {
        char *name;
        double *val, min, max, step;
        char *val_enum_names[MAX_CTRL_ENUM_NAMES];
        char *units;
        int decr_event;
        int incr_event;
    } ctrl[MAX_CTRL];
} test_ctrl_t;

//
// variables
//

char *progname = "ut";

char *test_name;
char *arg1_str, *arg2_str, *arg3_str;
int   arg1=-1, arg2=-1, arg3=-1;

test_ctrl_t tc;

plots_t plots[MAX_PLOT];

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

bool program_terminating;

//
// prototypes
//

void usage(void);
int pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event);

void init_audio_out(void);

void *plot_test(void *cx);
void *filter_test(void *cx);
void *antenna_test(void *cx);
void *rx_test(void *cx);

//
// test table
//

static struct test_s {
    char *test_name;
    void *(*proc)(void *cx);
} tests[] = {
        { "plot",    plot_test  },
        { "filter",  filter_test   },
        { "antenna", antenna_test   },
        { "rx_sim",  rx_test    },
        { "rx_sdr",  rx_test    },
                };

// -----------------  MAIN  --------------------------------

int main(int argc, char **argv)
{
    int i;
    struct test_s *t;
    pthread_t tid;

    #define MAX_TESTS (sizeof(tests)/sizeof(tests[0]))

    // parse cmdline args
    // - argv[1] is the test_name
    // - argv[2..4] are additional args, values copied to argN_str and argN
    if (argc == 1) {
        usage();
        return 1;
    }
    test_name = argv[1];
    if (argc > 2) { arg1_str = argv[2]; sscanf(arg1_str, "%d", &arg1); }
    if (argc > 3) { arg2_str = argv[3]; sscanf(arg2_str, "%d", &arg2); }
    if (argc > 4) { arg3_str = argv[4]; sscanf(arg3_str, "%d", &arg3); }
    
    // find the test_name in the tests array
    for (i = 0; i < MAX_TESTS; i++) {
        t = &tests[i];
        if (strcmp(test_name, t->test_name) == 0) {
            break;
        }
    }
    if (i == MAX_TESTS) {
        FATAL("test '%s' not found\n", test_name);
    }

    // call initialization routines
    init_audio_out();

    // start test thread
    NOTICE("Running '%s' test\n", test_name);
    sprintf(tc.name, "%s", test_name);
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

    // xxx
    program_terminating = true;
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
        char str[200], *p;

        pthread_mutex_lock(&mutex);
        for (int i = 0; i < MAX_PLOT; i++) {
            plots_t *p = &plots[i];
            if (p->title) {
                sdl_plot(pane,
                         p->x_pos, p->y_pos, p->x_width, p->y_height,
                         p->data, p->n,
                         p->xv_min, p->xv_max, p->xv_cursor,
                         p->yv_min, p->yv_max,
                         p->flags, p->title, p->x_units);
            }
        }
        pthread_mutex_unlock(&mutex);  // sep mutex for each xxx

        str[0] = '\0';
        p = str;
        p += sprintf(p, "%s", tc.name);
        if (tc.info[0] != '\0') {
            p += sprintf(p, " - %s", tc.info);
        }
        sdl_render_printf(pane, 
                          0, pane->h-ROW2Y(4,FONTSZ), FONTSZ,
                          SDL_WHITE, SDL_BLACK, "%s" , str);

        for (int i = 0; i < MAX_CTRL; i++) {
            struct test_ctrl_s *x = &tc.ctrl[i];
            if (x->val == NULL) continue;

            str[0] = '\0';
            p = str;
            if (x->name) {
                p += sprintf(p, "%s ", x->name);
            }
            if (x->val_enum_names[0] != NULL) {
                int tmp = nearbyint(*x->val);
                if (tmp < 0 || tmp >= MAX_CTRL_ENUM_NAMES || x->val_enum_names[tmp] == NULL) {
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

        for (int i = 0; i < MAX_CTRL; i++) {
            struct test_ctrl_s *x = &tc.ctrl[i];
            if (x->val == NULL) continue;

            str[0] = '\0';
            if (x->decr_event == SDL_EVENT_KEY_UP_ARROW && x->incr_event == SDL_EVENT_KEY_DOWN_ARROW) {
                sprintf(str, "^ v");
            } else if (x->decr_event == SDL_EVENT_KEY_SHIFT_UP_ARROW && x->incr_event == SDL_EVENT_KEY_SHIFT_DOWN_ARROW) {
                sprintf(str, "SHIFT ^ v");
            } else if (x->decr_event == SDL_EVENT_KEY_UP_ARROW+CTRL && x->incr_event == SDL_EVENT_KEY_DOWN_ARROW+CTRL) {
                sprintf(str, "CTRL ^ v");
            } else if (x->decr_event == SDL_EVENT_KEY_UP_ARROW+ALT && x->incr_event == SDL_EVENT_KEY_DOWN_ARROW+ALT) {
                sprintf(str, "ALT ^ v");
            } else if (x->decr_event == SDL_EVENT_KEY_LEFT_ARROW && x->incr_event == SDL_EVENT_KEY_RIGHT_ARROW) {
                sprintf(str, "<- ->");
            } else if (x->decr_event == SDL_EVENT_KEY_SHIFT_LEFT_ARROW && x->incr_event == SDL_EVENT_KEY_SHIFT_RIGHT_ARROW) {
                sprintf(str, "SHIFT <- ->");
            } else if (x->decr_event == SDL_EVENT_KEY_LEFT_ARROW+CTRL && x->incr_event == SDL_EVENT_KEY_RIGHT_ARROW+CTRL) {
                sprintf(str, "CTRL <- ->");
            } else if (x->decr_event == SDL_EVENT_KEY_LEFT_ARROW+ALT && x->incr_event == SDL_EVENT_KEY_RIGHT_ARROW+ALT) {
                sprintf(str, "ALT <- ->");
            } else {
                p = str;
                if (isgraph(x->decr_event)) p += sprintf(p, "%c ", x->decr_event);
                if (isgraph(x->incr_event)) p += sprintf(p, "%c ", x->incr_event);
            }

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
        if (event->event_id == SDL_EVENT_NONE) {
            return PANE_HANDLER_RET_NO_ACTION;
        }

        // loop over test controls to find matching event
        for (int i = 0; i < MAX_CTRL; i++) {
            struct test_ctrl_s *x = &tc.ctrl[i];
            double tmp;

            if (x->val == NULL) continue;

            if (x->incr_event == event->event_id) {
                tmp = *x->val + x->step;
                if (tmp > x->max) tmp = x->max;
                *x->val = tmp;
                break;
            }

            if (x->decr_event == event->event_id) {
                tmp = *x->val - x->step;
                if (tmp < x->min) tmp = x->min;
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

// -----------------  PLOT HELPERS  ------------------------

void plot_real(int idx, 
               double *data, int n, double xvmin, double xvmax, double yvmin, double yvmax, 
               char *title, char *x_units,
               int x_pos, int y_pos, int x_width, int y_height)
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
    p->flags   = 0;
    p->title   = title;
    p->x_units = x_units;
    p->x_pos   = x_pos;
    p->y_pos   = y_pos;
    p->x_width = x_width;
    p->y_height= y_height;
    pthread_mutex_unlock(&mutex);
}

// to auto scale use max=0
void plot_fft(int idx, 
              complex *fft, int n, double sample_rate, bool half_flag, double yv_max, double xv_cursor,
              char *title,
              int x_pos, int y_pos, int x_width, int y_height)
{
    plots_t *p = &plots[idx];

    if (n > MAX_PLOT_DATA) {
        FATAL("plot n=%d\n", n);
    }

    pthread_mutex_lock(&mutex);
    if (!half_flag) {
        int j=0;
        for (int i = n/2; i < n; i++) {
            p->data[j++] = cabs(fft[i]);
        }
        for (int i = 0; i < n/2; i++) {
            p->data[j++] = cabs(fft[i]);
        }
        if (j != n) FATAL("n=%d j=%d\n", n, j);
    } else {
        n /= 2;
        for (int i = 0; i < n; i++) {
            p->data[i] = cabs(fft[i]);
        }
    }
    if (yv_max == 0) {
        normalize(p->data, n, 0, 1);
        yv_max = 1;
    }
    p->n       = n;
    p->xv_min  = (!half_flag ? -sample_rate/2 : 0);
    p->xv_max  = sample_rate/2;
    p->yv_min  = 0;
    p->yv_max  = yv_max;
    p->xv_cursor = xv_cursor;
    p->flags   = SDL_PLOT_FLAG_BARS;
    p->title   = title;
    p->x_units = "HZ";
    p->x_pos   = x_pos;
    p->y_pos   = y_pos;
    p->x_width = x_width;
    p->y_height= y_height;
    pthread_mutex_unlock(&mutex);
}

// -----------------  AUDIO OUT  ---------------------------

#ifndef USE_PA

void init_audio_out(void)
{
    // nothing needed
}

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
        //double out_min, out_max, out_avg;
        //average_float(out, MAX_OUT, &out_min, &out_max, &out_avg);
        //fprintf(stderr, "min=%f max=%f avg=%f\n", out_min, out_max, out_avg);
        max = 0;
    }
}

#else

#define MAX_AO_BUFF 2000

double ao_buff[MAX_AO_BUFF];
unsigned long ao_head;
unsigned long ao_tail;

void *pa_play_thread(void*cx);

void init_audio_out(void)
{
    pthread_t tid;

    pa_init();
    pthread_create(&tid, NULL, pa_play_thread, NULL);
}

void audio_out(double yo)
{
    #define MAX_MA 1000
    static void *ma_cx;
    double ma;

    while (ao_tail - ao_head == MAX_AO_BUFF) {
        usleep(1000);
    }

    ma = moving_avg(yo, MAX_MA, &ma_cx);
    ao_buff[ao_tail%MAX_AO_BUFF] = yo - ma;
    ao_tail++;
}

int pa_play_cb(void *data, void *cx_arg)
{
    while (ao_head == ao_tail) {
        if (program_terminating) {
            return -1;
        }
        usleep(1000);
    }

    *(float*)data = ao_buff[ao_head%MAX_AO_BUFF];
    ao_head++;
    return 0;
}

void *pa_play_thread(void*cx)
{
    int ret;
    int num_chan = 1;
    int sample_rate = 22000;  //xxx define

    ret = pa_play2(DEFAULT_OUTPUT_DEVICE, num_chan, sample_rate, PA_FLOAT32, pa_play_cb, NULL);
    if (ret != 0) {
        FATAL("pa_play2\n");
    }

    return NULL;
}

#endif

// -----------------  PLOT TEST  ---------------------------

void *plot_test(void *cx)
{
    int sample_rate, f, n;
    double *sw, t, dt;

    #define SECS 1

    sprintf(tc.info, "Hello World");

    sample_rate = 20000;
    f = 10;
    n = SECS * sample_rate;
    sw = fftw_alloc_real(n);
    dt = (1. / sample_rate);
    t = 0;

    for (int i = 0; i < n; i++) {
        sw[i] = sin(TWO_PI * f * t);
        t += dt;
    }

    plot_real(0, sw, n,  0, 1,  -1, +1,     "SINE WAVE", NULL,  0,  0, 50, 25);
    plot_real(1, sw, n,  0, 1,  -0.5, +0.5, "SINE WAVE", NULL,  0, 25, 50, 25);;
    plot_real(2, sw, n,  0, 1,  0.5, +1.5,  "SINE WAVE", NULL,  0, 50, 50, 25);;

    return NULL;
}

// -----------------  BAND PASS FILTER TEST  --------------

void init_using_sine_waves(double *sw, int n, int freq_first, int freq_last, int freq_step, int sample_rate);
void init_using_white_noise(double *wn, int n);
void init_using_wav_file(double *wav, int n, char *filename);

// xxx plot_fft normalizes,  
// xxx put back the / n in fft.c  ??
// xxx glitch when changing filters
void *filter_test(void *cx)
{
    #define SINE_WAVE   0
    #define SINE_WAVES  1
    #define WHITE_NOISE 2
    #define WAV_FILE    3

    #define DEFAULT_CUTOFF 4000
    #define DEFAULT_ORDER  8

    int        sample_rate = 20000;               // 20 KS/s
    int        max         = 120 * sample_rate;   // 120 total secs of data
    int        n           = .1 * sample_rate;    // .1 secs of data processed at a time
    int        total       = 0;

    double     tc_mode = SINE_WAVES;
    double     tc_cutoff = DEFAULT_CUTOFF;
    double     tc_order  = DEFAULT_ORDER;
    double     tc_reset = 0;

    double    *in_real         = fftw_alloc_real(max);
    double    *in              = fftw_alloc_real(n);
    double    *in_filtered     = fftw_alloc_real(n);
    complex   *in_fft          = fftw_alloc_complex(n);
    complex   *in_filtered_fft = fftw_alloc_complex(n);

    BWLowPass *bwlpf = NULL;
    int        bwlpf_cutoff = 0;
    int        bwlpf_order  = 0;
    int        current_mode;

    double     yv_max;

    // init test controls
    tc.ctrl[0] = (struct test_ctrl_s)
                 {"MODE", &tc_mode, SINE_WAVE, WAV_FILE, 1, 
                  {"SINE_WAVE", "SINE_WAVES", "WHITE_NOISE", "WAV_FILE"}, "", 
                  SDL_EVENT_KEY_UP_ARROW, SDL_EVENT_KEY_DOWN_ARROW};
    tc.ctrl[1] = (struct test_ctrl_s)
                 {"CUTOFF", &tc_cutoff, 100, 10000, 100,
                  {}, "HZ", 
                  SDL_EVENT_KEY_LEFT_ARROW, SDL_EVENT_KEY_RIGHT_ARROW};
    tc.ctrl[2] = (struct test_ctrl_s)
                 {"ORDER", &tc_order, 1, 20, 1,
                  {}, NULL, 
                  SDL_EVENT_KEY_SHIFT_LEFT_ARROW, SDL_EVENT_KEY_SHIFT_RIGHT_ARROW};
    tc.ctrl[3] = (struct test_ctrl_s)
                 {"RESET", &tc_reset, 0, 1, 1,
                  {"", ""}, NULL, 
                  SDL_EVENT_NONE, 'r'};

    while (true) {
        // construct in buff from either:
        // - sine waves
        // - white noise
        // - wav file
        current_mode = tc_mode;
        switch (current_mode) {
        case SINE_WAVE:
            init_using_sine_waves(in_real, max, 1000, 1000, 0, sample_rate);
            yv_max = 1000;
            break;
        case SINE_WAVES:
            init_using_sine_waves(in_real, max, 0, 10000, 500, sample_rate);
            yv_max = 1000;
            break;
        case WHITE_NOISE:
            init_using_white_noise(in_real, max);
            yv_max = 30;
            break;
        case WAV_FILE:
            init_using_wav_file(in_real, max, "wav_files/super_critical.wav");
            yv_max = 15;
            break;
        default:
            FATAL("invalid current_mode %d\n", current_mode);
            break;
        }

        // loop until test-ctrl mode changes
        while (current_mode == tc_mode) {
            // reset ctrls when requested
            if (tc_reset) {
                tc_reset = 0;
                tc_cutoff = DEFAULT_CUTOFF;
                tc_order  = DEFAULT_ORDER;
            }

            // if test ctrl cutoff freq has changed then 
            // free and recreate the butterworth lpf
            if (tc_cutoff != bwlpf_cutoff || tc_order != bwlpf_order) {
                if (bwlpf != NULL) {
                    free_bw_low_pass(bwlpf);
                }
                bwlpf = create_bw_low_pass_filter(tc_order, sample_rate, tc_cutoff);
                bwlpf_cutoff = tc_cutoff;
                bwlpf_order  = tc_order;
            }

            // copy .1 secs of data from 'in_real' to 'in'
            for (int i = 0; i < n; i++) {
                in[i] = in_real[(total+i)%max];
            }
            total += n;

            // apply low pass filter of 'in', output to 'in_filtered'
            for (int i = 0; i < n; i++) {
                in_filtered[i] = bw_low_pass(bwlpf, in[i]);
            }

            // plot fft of 'in'
            fft_fwd_r2c(in, in_fft, n);
            plot_fft(0, in_fft, n, sample_rate, true, yv_max, tc_cutoff, "FFT", 0, 0, 50, 25);

            // plot fft of 'in_filtered'
            fft_fwd_r2c(in_filtered, in_filtered_fft, n);
            plot_fft(1, in_filtered_fft, n, sample_rate, true, yv_max, tc_cutoff, "FFT", 0, 25, 50, 25);

            // play the filtered audio
            for (int i = 0; i < n; i++) {
                audio_out(in_filtered[i]);
            }
        }
    }        
}

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
    if (ret != 0 || num_chan != 1 || sample_rate < 21000 || sample_rate > 23000) {
        FATAL("read_wav_file ret=%d num_chan=%d sample_rate=%d\n", ret, num_chan, sample_rate);
    }

    for (int i = 0; i < n; i++) {
        wav[i] = data[i%num_items];
    }

    free(data);
}

// -----------------  ANTENNA TEST  -------------------------

#define ANTENNA_FILENAME "antenna.dat"
#define SAMPLE_RATE      2400000              // 2.4 MS/sec
#define MAX_N            (SAMPLE_RATE / 10)   // 0.1 sec range
#define DELTA_T          (1. / SAMPLE_RATE)

void *antenna_test(void *cx)
{
    double  *antenna, t, tmax;
    complex *antenna_fft;
    FILE    *fp;
    int      n = 0;

    init_antenna();

    antenna     = fftw_alloc_real(MAX_N);
    antenna_fft = fftw_alloc_complex(MAX_N);

    // xxx
    tmax = (arg1 == -1 ? 30 : arg1);

    // xxx
    fp = fopen(ANTENNA_FILENAME, "w");
    if (fp == NULL) {
        FATAL("failed to create %s\n", ANTENNA_FILENAME);
    }

    // xxx
    for (t = 0; t < tmax; t += DELTA_T) {
        // xxx
        antenna[n] = get_antenna(t);
        n++;

        // xxx
        if (n == MAX_N) {
            sprintf(tc.info, "Generating simulated antenna data: %0.1f secs", t);
            fft_fwd_r2c(antenna, antenna_fft, n);
            plot_fft(0, antenna_fft, n, SAMPLE_RATE, true, 0, SAMPLE_RATE, "ANTENNA_FFT", 0, 0, 50, 50);
            fwrite(antenna, sizeof(double), n, fp);
            n = 0;
        }
    }

    sprintf(tc.info, "Done");

    // xxxx
    fclose(fp);

    // xxx 
    sdl_push_event(&(sdl_event_t){SDL_EVENT_QUIT});

    return NULL;
}

// -----------------  RX TEST  -----------------------------

// xxx comments and cleanup
// xxx still clicking when freq is changed

// defines

#define SAMPLE_RATE     2400000  // 2.4 MS/sec
#define MAX_DATA_CHUNK  131072
#define MAX_DATA        (4*MAX_DATA_CHUNK)

#define DEFAULT_FREQ    (strcmp(test_name,"rx_sim") == 0 ? 500000 : 1030000)  // 500 KHz or 1030 KHz
#define DEFAULT_K1      0.004
#define DEFAULT_K2      4.0

// typedefs

typedef struct {
    complex *data;
    complex *data_fft;
    complex *data_lpf;
    complex *data_lpf_fft;
    int      n;
} fft_t;

// variables

unsigned long Head;
unsigned long Tail;
complex       Data[MAX_DATA];

fft_t         fft;

double        tc_freq;
double        tc_freq_offset;
double        tc_k1;
double        tc_k2;
double        tc_reset;

// prototypes

void rx_tc_init(void);
void rx_tc_reset(void);

void rx_fft_init(void);
void rx_fft_add_data(complex data, complex data_lpf);
void *rx_fft_thread(void *cx);

void rx_demod_am(complex data_lpf);

void rx_sdr_init(void);
void rx_sim_init(void);

// AAA xxx
// - incorporate the sdr
// - mark the top fft at the frequency point
// - exapand the graph to just part of the fft
//
// AAA done
// - select sim or sdr
// - tc_reset
// - have a ctr and offset freq

// - - - - - - - - -  RX TEST - - - - - - - - - - - 

void *rx_test(void *cx)
{
    pthread_t  tid;
    BWLowPass *bwi, *bwq;
    complex    data_orig, data, data_lpf;
    double     t=0, w;
    int cnt=0;
    const double delta_t = 1. / SAMPLE_RATE;

    rx_tc_init();
    rx_fft_init();

    if (strcmp(test_name, "rx_sim") == 0) {
        rx_sim_init();
    } else {
        rx_sdr_init();
    }

    pthread_create(&tid, NULL, rx_fft_thread, NULL);

    bwi = create_bw_low_pass_filter(8, SAMPLE_RATE, 4000);  //xxx adjust the 4000?
    bwq = create_bw_low_pass_filter(8, SAMPLE_RATE, 4000);

    while (true) {
        if (tc_reset) {
            rx_tc_reset();
        }

        if (Head == Tail) {
            usleep(1000);
            continue;
        }

        // xxx needs cleanup and comments

        data_orig = Data[Head % MAX_DATA];

        w = TWO_PI * tc_freq_offset;
        data = data_orig * cexp(-I * w * t);

        data_lpf = bw_low_pass(bwi, creal(data)) +
                   bw_low_pass(bwq, cimag(data)) * I;

        rx_fft_add_data(data_orig, data_lpf);

        rx_demod_am(data_lpf);

        Head++;

        t += delta_t;

        if (cnt++ >= SAMPLE_RATE/10) {
            cnt = 0;
            sprintf(tc.info, "FREQ = %0.3f MHz", (tc_freq + tc_freq_offset) / 1000000);
        }
    }

    return NULL;
}

void rx_tc_init(void)
{
    tc.ctrl[0] = (struct test_ctrl_s)
                 {"F", &tc_freq, 0, 200000000, 1000,   // 0 to 200 MHx
                  {}, "HZ", 
                  SDL_EVENT_KEY_LEFT_ARROW, SDL_EVENT_KEY_RIGHT_ARROW};
    tc.ctrl[1] = (struct test_ctrl_s)
                 {"F_OFF", &tc_freq_offset, -600000, 600000, 1000,
                  {}, "HZ", 
                  SDL_EVENT_KEY_LEFT_ARROW+CTRL, SDL_EVENT_KEY_RIGHT_ARROW+CTRL};
    tc.ctrl[2] = (struct test_ctrl_s)
                 {"K1", &tc_k1, 0, 1.0, .0001, 
                  {}, NULL,
                  '1', '2'};
    tc.ctrl[3] = (struct test_ctrl_s)
                 {"K2", &tc_k2, 1, 100, 1, 
                  {}, NULL,
                  '3', '4'};
    tc.ctrl[4] = (struct test_ctrl_s)
                 {"RESET", &tc_reset, 0, 1, 1,
                  {"", ""}, NULL, 
                  SDL_EVENT_NONE, 'r'};

    rx_tc_reset();
}

void rx_tc_reset(void)
{
    tc_freq        = DEFAULT_FREQ;
    tc_freq_offset = 0;
    tc_k1          = DEFAULT_K1;
    tc_k2          = DEFAULT_K2;
    tc_reset       = 0;
}

// - - - - - - - - -  RX FFT   - - - - - - - - - - - 

#define FFT_N        240000
#define FFT_INTERVAL 100000   // .2 sec

void rx_fft_init(void)
{
    fft.data         = fftw_alloc_complex(FFT_N);
    fft.data_fft     = fftw_alloc_complex(FFT_N);
    fft.data_lpf     = fftw_alloc_complex(FFT_N);
    fft.data_lpf_fft = fftw_alloc_complex(FFT_N);
}

void rx_fft_add_data(complex data, complex data_lpf)
{
    if (fft.n < FFT_N) {
        fft.data[fft.n] = data;
        fft.data_lpf[fft.n] = data_lpf;
        fft.n++;
    }
}

void *rx_fft_thread(void *cx)
{
    #define DATA_BLOCK_DURATION  ((double)FFT_N / SAMPLE_RATE)

    double               yv_max;
    unsigned long        tnow;
    static unsigned long tlast;

    if (strcmp(test_name, "rx_sim") == 0) {
        yv_max = 150000;
    } else {
        yv_max = 4000;
    }

    while (true) {
        tnow = microsec_timer();
        if (fft.n != FFT_N || tnow-tlast < FFT_INTERVAL) {
            usleep(10000);
            continue;
        }

        fft_fwd_c2c(fft.data, fft.data_fft, fft.n);
        plot_fft(0, fft.data_fft, fft.n, SAMPLE_RATE, false, yv_max, tc_freq_offset, "DATA_FFT", 0, 0, 100, 30);

        // xxx expand the plot?
        fft_fwd_c2c(fft.data_lpf, fft.data_lpf_fft, fft.n);
//xxxx 
        plot_fft(1, fft.data_lpf_fft, fft.n, SAMPLE_RATE, false, yv_max, 0, "DATA_LPF_FFT", 0, 30, 100, 30);
// xxx                                                                   ^

        tlast = tnow;
        fft.n = 0;
    }
}

// - - - - - - - - -  RX DEMODULATORS  - - - - - - - 

void rx_demod_am(complex data_lpf)
{
    double        y;
    static double yo;
    static int    cnt;

#if 0
    // xxx why is this not needed?
    static const double delta_t = 1. / SAMPLE_RATE;
    static const double w = 300000 * TWO_PI;  // xxx try 0
    static double t;

    data_lpf = data_lpf * cexp(I * w * t);
    t += delta_t;
#endif

    y = cabs(data_lpf);
    if (y > 0) {
        yo = yo + (y - yo) * tc_k1;
    }

    // xxx why 0.9
    if (cnt++ == (int)(0.9 * SAMPLE_RATE / 22000)) {  // xxx 22000 is the aplay rate
        audio_out(yo*tc_k2);  // xxx auto scale
        cnt = 0;
    }
}

// - - - - - - - - -  RX SIMULATOR - - - - - - - - - 

void *rx_sim_thread(void *cx);

void rx_sim_init(void)
{
    pthread_t tid;

    pthread_create(&tid, NULL, rx_sim_thread, NULL);
}

void *rx_sim_thread(void *cx)
{
    int fd;
    struct stat statbuf;
    size_t file_offset, file_size, len;
    double t, delta_t, antenna[MAX_DATA_CHUNK], w;
    complex *data;

    // open ANTENNA_FILENAME
    fd = open(ANTENNA_FILENAME, O_RDONLY);
    if (fd < 0) {
        FATAL("failed open %s, %s\n", ANTENNA_FILENAME, strerror(errno));
    }

    // get size of ANTENNA_FILENAME
    fstat(fd, &statbuf);
    file_size = statbuf.st_size;
    file_offset = 0;

    // other inits
    t = 0;
    delta_t = (1. / SAMPLE_RATE);

    // loop forever, 
    // when end of file is reached start over from file begining
    while (true) {
        // wait for space to be available in Data array, 
        while (MAX_DATA - (Tail - Head) < MAX_DATA_CHUNK) {
            usleep(1000);
        }

        // read values from antenna file, these are real values
        len = pread(fd, antenna, MAX_DATA_CHUNK*sizeof(double), file_offset);
        if (len != MAX_DATA_CHUNK*sizeof(double)) {
            FATAL("read %s, len=%ld, %s\n", ANTENNA_FILENAME, len, strerror(errno));
        }
        file_offset += len;
        if (file_offset + len > file_size) {
            file_offset = 0;
        }

        // frequency shift the antenna data, and 
        // store result in Data[Tail], these are complex values
        data = &Data[Tail % MAX_DATA];
        w = TWO_PI * tc_freq;
        for (int i = 0; i < MAX_DATA_CHUNK; i++) {
            data[i] = antenna[i] * cexp(-I * w * t);
            t += delta_t;
        }

        // increment Tail
        Tail += MAX_DATA_CHUNK;
    }

    return NULL;
}

// - - - - - - - - -  RX SDR - - - - - - - - - - - - 

void *rx_sdr_ctrl_thread(void *cx);
void rx_sdr_cb(unsigned char *iq, size_t len);

// AAA
void rx_sdr_init(void)
{
    pthread_t tid;
    struct rtlsdr_dev *dev;

    dev = sdr_init(tc_freq, rx_sdr_cb);
    pthread_create(&tid, NULL, rx_sdr_ctrl_thread, dev);
}

void * rx_sdr_ctrl_thread(void *cx)
{
    struct rtlsdr_dev *dev = cx;
    double tc_freq_last_set = tc_freq;
    double f;

    while (true) {
        f = tc_freq;
        if (f != tc_freq_last_set) {
            NOTICE("SET FREQ %f\n", f);
            sdr_set_freq(dev, f);
            tc_freq_last_set = f;
        }

        usleep(10000);
    }

    return NULL;
}

// AAA todo
void rx_sdr_cb(unsigned char *iq, size_t len)
{
    int items=len/2, i, j;

    //NOTICE("RX_SDR_CB len=%d\n", len);

    if (MAX_DATA - (Tail - Head) < items) {
        NOTICE("discarding sdr data\n");
        return;
    }

    j = Tail % MAX_DATA;
    for (i = 0; i < items; i++) {
        Data[j] = ((iq[2*i+0] - 128.) + (iq[2*i+1] - 128.) * I) / 128.;  // xxx try without all the 128.
        if (++j == MAX_DATA) j = 0;
    }

    Tail += items;
}

#if 0
void *rx_sim_thread2(void *cx)
{
    int fd;
    struct stat statbuf;
    size_t file_offset, file_size, len;
    unsigned char iq[2*MAX_DATA_CHUNK];

    // open BUF0
    fd = open("buf0", O_RDONLY);
    if (fd < 0) {
        FATAL("failed open buf0, %s\n", strerror(errno));
    }

    // get size of BUF0
    fstat(fd, &statbuf);
    file_size = statbuf.st_size;
    file_offset = 0;

    // other inits
    double t = 0;
    double delta_t = (1. / SAMPLE_RATE);

    // loop forever, 
    // when end of file is reached start over from file begining
    while (true) {
        // wait for space to be available in Data array, 
        while (MAX_DATA - (Tail - Head) < MAX_DATA_CHUNK) {
            usleep(1000);
        }

        // read values from buf0 file, these are IQ bytes
        len = pread(fd, iq, MAX_DATA_CHUNK*2, file_offset);
        if (len != MAX_DATA_CHUNK*2) {
            FATAL("read %s, len=%ld, %s\n", "buf0", len, strerror(errno));
        }
        file_offset += len;
        if (file_offset + len > file_size) {
            file_offset = 0;
        }

        // convert the iq bytes to complex Data values
        complex *data = &Data[Tail % MAX_DATA];
        double w = TWO_PI * tc_freq;
        for (int i = 0; i < MAX_DATA_CHUNK; i++) {
            data[i] = (iq[2*i+0] - 128) +
                      (iq[2*i+1] - 128) * I;
            data[i] /= 128;

            data[i] *= cexp(I * w * t);

            t += delta_t;
        }

        // increment Tail
        Tail += MAX_DATA_CHUNK;
    }

    return NULL;
}
#endif
