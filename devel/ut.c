// xxx later
// - comments
// - exit pgm
// - better way to handle global args

// xxx improve filter quality
// xxx init_fft?

// xxx
// - rx sim
// - rx rtlsdr file
// - rx rtlsdr direct
// - rx rtlsdr server

#include "common.h"

//
// defines
//

#define min(a,b) ((a) < (b) ? (a) : (b))

#define MAX_TESTS (sizeof(tests)/sizeof(tests[0]))

#define CTRL SDL_EVENT_KEY_CTRL
#define ALT  SDL_EVENT_KEY_ALT

#define USE_PA

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

#define MAX_CTRL_ENUM_NAMES 10
#define MAX_CTRL 6
typedef struct {
    char *name;
    char *info;
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

#ifdef USE_PA
void *pa_play_thread(void*cx);
#endif

void *plot_test(void *cx);
void *bpf_test(void *cx);
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
        { "bpf",     bpf_test   },
        { "antenna", antenna_test   },
        { "rx",      rx_test    },
                };

// -----------------  MAIN  --------------------------------

int main(int argc, char **argv)
{
    int i;
    struct test_s *t;
    pthread_t tid;
    char *test_name;

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

    init_fft();  // xxx is this needed, elim later
#ifdef USE_PA
    pa_init();
    pthread_create(&tid, NULL, pa_play_thread, NULL);
#endif

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

        str[0] = '\0';
        p = str;
        if (tc.name != NULL) {
            p += sprintf(p, "%s", tc.name);
        }
        if (tc.info != NULL) {
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

// -----------------  UTILS  -------------------------------

// xxx move to fft or misc
void zero_real(double *data, int n)
{
    memset(data, 0, n*sizeof(double));
}

void zero_complex(complex *data, int n)
{
    memset(data, 0, n*sizeof(complex));
}

// xxx move to section below
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
        p->data[i] = cabsl(data[i]);  // xxx
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

// xxx move to audio section
#ifndef USE_PA
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
    int sample_rate = 22000;

    ret = pa_play2(DEFAULT_OUTPUT_DEVICE, num_chan, sample_rate, PA_FLOAT32, pa_play_cb, NULL);
    if (ret != 0) {
        FATAL("pa_play2\n");
    }

    return NULL;
}
#endif

// -----------------  PLOT TEST  ---------------------------

// xxx make plot screen locations flexible
void *plot_test(void *cx)
{
    int sample_rate, f, n;
    double *sw;

    #define SECS 1

    tc.name = "PLOT";
    tc.info = "Hello World";

    sample_rate = 20000;
    f = 10;
    n = SECS * sample_rate;
    sw = fftw_alloc_real(n);

    init_using_sine_waves(sw, n, f, f, 0, sample_rate);

    plot_real(0, sw, n,  0, 1,  -1, +1,  "SINE WAVE");
    plot_real(1, sw, n,  0, 1,  -0.5, +0.5,  "SINE WAVE");
    plot_real(2, sw, n,  0, 1,  0.5, +1.5,  "SINE WAVE");

    return NULL;
}

// -----------------  BAND PASS FILTER TEST  --------------

void *bpf_test(void *cx)
{
    //double *in_real;
    //complex *in, *in_fft, *in_filtered, *in_filtered_fft;
    int current_mode;

    int sample_rate = 20000;
    int max = 120 * sample_rate;  // 120 secs of data
    int total = 0;
    int n = .1 * sample_rate;     // .1 secs of data

    #define SINE_WAVE   0
    #define SINE_WAVES  1
    #define WHITE_NOISE 2
    #define WAV_FILE    3

    #define DEFAULT_CTR 2010
    #define DEFAULT_WIDTH 4020

    static char tc_info[100];
    static double tc_mode = SINE_WAVES;
    static double tc_reset = 0;
    static double tc_ctr = DEFAULT_CTR;
    static double tc_width = DEFAULT_WIDTH;

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
    tc.ctrl[3] = (struct test_ctrl_s)
                 {"RESET", &tc_reset, 0, 1, 1,
                  {"", ""}, NULL, 
                  SDL_EVENT_NONE, 'r'};

    // alloc buffers
// xxx do these zero?
    double  *in_real         = fftw_alloc_real(max);
    double  *in              = fftw_alloc_real(n);
    double  *in_filtered     = fftw_alloc_real(n);
    complex *in_fft          = fftw_alloc_complex(n);
    complex *in_filtered_fft = fftw_alloc_complex(n);

    BWLowPass* bw;
    bw = create_bw_low_pass_filter(8, sample_rate, 4000);  // xxx make this adjustable

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

        // xxx also plot Bodie plots

        while (current_mode == tc_mode) {
            if (tc_reset) {
                tc_reset = 0;
                tc_ctr = DEFAULT_CTR;
                tc_width = DEFAULT_WIDTH;
            }

            // copy .1 secs of data from 'in_real' to 'in'
            for (int i = 0; i < n; i++) {
                in[i] = in_real[(total+i)%max];
            }
            total += n;

            // filter 'in' to 'in_filtered'
            for (int i = 0; i < n; i++) {
                in_filtered[i] = bw_low_pass(bw, in[i]);
            }

            // plot fft of in and in_filtered
            fft_fwd_r2c(in, in_fft, n);
            fft_fwd_r2c(in_filtered, in_filtered_fft, n);

            plot_fft(1, in_fft, n, n/.01, "FFT", "HZ");  // xxx define for .01
            plot_fft(3, in_filtered_fft, n, n/.01, "FFT", "HZ");

            // play the filtered audio
            for (int i = 0; i < n; i++) {
                audio_out(in_filtered[i]);
            }
        }
    }        
}

#if 0
// xxx use filter
            // copy .1 secs of data from in_real to in buff
            for (int i = 0; i < n; i++) {
                in[i] = in_real[(total+i)%max];
            }
            total += n;

            // apply bpf filter to in buff, creating in_filtered and in_fft output
            // perform fwd fft on in_filtered, creating in_filtered_fft
            fft_bpf_complex(in, in_filtered, in_fft, n, sample_rate, tc_ctr-tc_width/2, tc_ctr+tc_width/2);
            fft_fwd_c2c(in_filtered, in_filtered_fft, n);

            // plot the in buff, and fft of in buff
            plot_complex(0, in, n/10, 0, .01, -2, 2, "SINE_WAVES", "SECS");  // xxx title and units?
            plot_fft(1, in_fft, n, n/.01, "FFT", "HZ");  // xxx define for .01

            // plot the filtered in buff, and fft of the filtered in buff
            plot_complex(2, in_filtered, n/10, 0, .01, -2, 2, "SINE_WAVES", "SECS");  // xxx title and units?
            plot_fft(3, in_filtered_fft, n, n/.01, "FFT", "HZ");

            // play the filtered audio
            for (int i = 0; i < n; i++) {
                audio_out(in_filtered[i]);
            }
#endif

// -----------------  ANTENNA TEST  -------------------------

#define ANTENNA_FILENAME "antenna.dat"
#define SAMPLE_RATE  2400000              // 2.4 MS/sec
#define MAX_N        (SAMPLE_RATE / 10)   // 0.1 sec range
#define DELTA_T      (1. / SAMPLE_RATE)

// xxx check the fft code in ut_antenna.c
void *antenna_test(void *cx)
{
    double  *antenna, t, tmax;
    complex *antenna_fft;
    FILE    *fp;
    int      n = 0;

    static char tc_info[100];

    init_antenna();

    antenna     = fftw_alloc_real(MAX_N);    // xxx not needed as an array
    antenna_fft = fftw_alloc_complex(MAX_N); // xxx not needed as an array

    // xxx
    tmax = (arg1 == -1 ? 30 : arg1);

    // init test controls
    tc.name = "GEN";
    tc.info = tc_info;

    // xxx
    fp = fopen(ANTENNA_FILENAME, "w");
    if (fp == NULL) {
        FATAL("failed to create gen.dat\n");
    }

    // xxx
    for (t = 0; t < tmax; t += DELTA_T) {
        // xxx
        antenna[n] = get_antenna(t);
        n++;

        // xxx
        if (n == MAX_N) {
            sprintf(tc_info, "%0.1f secs", t);
            fft_fwd_r2c(antenna, antenna_fft, n);
            plot_fft(0, antenna_fft, n, n/.1, "ANTENNA_FFT", "HZ");  // xxx define for .1
            fwrite(antenna, sizeof(double), n, fp);
            n = 0;
        }
    }

    // xxxx
    fclose(fp);

    return NULL;
}

// -----------------  RX TEST  -----------------------------

#define SAMPLE_RATE  2400000              // 2.4 MS/sec
#define MAX_DATA_BLOCK 3
#define MAX_DATA       131072
#define DATA_BLOCK_DURATION  ((double)MAX_DATA / SAMPLE_RATE)

#define DEFAULT_FREQ 500000   // 500 KHz
#define DEFAULT_K1   0.004
#define DEFAULT_K2   4.0
//#define FREQ_OFFSET  300000   // 300 KHz
#define FREQ_OFFSET  0
#define BPF_WIDTH    8000     // 8 KHz

unsigned long Head;
unsigned long Tail;
complex      *Data[MAX_DATA_BLOCK];

double        tc_freq  = DEFAULT_FREQ;
double        tc_k1    = DEFAULT_K1;
double        tc_k2    = DEFAULT_K2;
double        tc_reset = 0;

void *get_data_thread(void *cx);

void *rx_test(void *cx)
{
    complex  *data, *data_lpf, *data_fft, *data_lpf_fft;
    double    y, yo;
    int       cnt, n;
    pthread_t tid;

    static char tc_info[100];

    n                 = MAX_DATA;;
    data_lpf     = fftw_alloc_complex(n);
    data_lpf_fft = fftw_alloc_complex(n);
    data_fft          = fftw_alloc_complex(n);
    for (int i = 0; i < MAX_DATA_BLOCK; i++) {
        Data[i] = fftw_alloc_complex(n);
    }
    yo  = 0;
    cnt = 0;

    tc.name = "RX";
    tc.info = tc_info;
    tc.ctrl[0] = (struct test_ctrl_s)
                 {"F", &tc_freq, 0, 1000000, 1000, 
                  {}, "HZ", 
                  SDL_EVENT_KEY_LEFT_ARROW, SDL_EVENT_KEY_RIGHT_ARROW};
    tc.ctrl[1] = (struct test_ctrl_s)
                 {"K1", &tc_k1, 0, 1.0, .0001, 
                  {}, NULL,
                  SDL_EVENT_KEY_LEFT_ARROW+CTRL, SDL_EVENT_KEY_RIGHT_ARROW+CTRL};
    tc.ctrl[2] = (struct test_ctrl_s)
                 {"K2", &tc_k2, 1, 100, 1, 
                  {}, NULL,
                  SDL_EVENT_KEY_LEFT_ARROW+ALT, SDL_EVENT_KEY_RIGHT_ARROW+ALT};
    tc.ctrl[3] = (struct test_ctrl_s)
                 {"RESET", &tc_reset, 0, 1, 1,
                  {"", ""}, NULL, 
                  SDL_EVENT_NONE, 'r'};

    pthread_create(&tid, NULL, get_data_thread, NULL);

    BWLowPass *bwi, *bwq;
    bwi = create_bw_low_pass_filter(8, SAMPLE_RATE, 4000);
    bwq = create_bw_low_pass_filter(8, SAMPLE_RATE, 4000);

    while (true) {
        // wait for data to be available
        while (Head == Tail) {
            usleep(1000);
        }

        // xxx
        if (tc_reset) {
            tc_freq     = DEFAULT_FREQ;
            tc_k1       = DEFAULT_K1;
            tc_k2       = DEFAULT_K2;
            tc_reset = 0;
        }

        // low pass filter
        data = Data[Head % MAX_DATA_BLOCK];

        // make data_fft, and plot
        fft_fwd_c2c(data, data_fft, n);
        plot_fft(0, data_fft, n, n/DATA_BLOCK_DURATION, "DATA_FFT", "HZ");

        // lpf
        for (int i = 0; i < n; i++) {
            data_lpf[i] = bw_low_pass(bwi, creal(data[i])) +
                          bw_low_pass(bwq, cimag(data[i])) * I;
        }

        fft_fwd_c2c(data_lpf, data_lpf_fft, n);
        plot_fft(1, data_lpf_fft, n, n/DATA_BLOCK_DURATION, "DATA_FILTERED_FFT", "HZ");  // xxx define for .1

#if 0
        fft_bpf_complex(data, data_lpf, data_fft, n, SAMPLE_RATE, 
                        FREQ_OFFSET-BPF_WIDTH/2, FREQ_OFFSET+BPF_WIDTH/2);
        plot_fft(0, data_fft, n, n/DATA_BLOCK_DURATION, "DATA_FFT", "HZ");  // xxx define for .1

        // debug
        fft_fwd_c2c(data_lpf, data_lpf_fft, n);
        plot_fft(1, data_lpf_fft, n, n/DATA_BLOCK_DURATION, "DATA_FILTERED_FFT", "HZ");  // xxx define for .1
#endif

        // shift data_lpf to 300 khz, and plot
        // xxx make this a routine for AM detector
        static double t;
        static double delta_t = 1. / SAMPLE_RATE;
        double w = 300000 * TWO_PI;
        for (int i = 0; i < n; i++) {
            data_lpf[i] = data_lpf[i] * cexp(I * w * t);  // xxx simd ?
            t += delta_t;
        }

        fft_fwd_c2c(data_lpf, data_lpf_fft, n);
        plot_fft(2, data_lpf_fft, n, n/DATA_BLOCK_DURATION, "DATA_SHIFTED_FFT", "HZ");  // xxx define for .1

        for (int i = 0; i < n; i++) {
            y = cabs(data_lpf[i]);  // xxx scaling
            if (y > 0) {
                yo = yo + (y - yo) * tc_k1;
            }
            if (cnt++ == (SAMPLE_RATE / 22000)) {  // xxx 22000 is the aplay rate
                audio_out(yo*tc_k2);  // xxx how to auto scale:
                cnt = 0;
            }
        }


// move the links to the notes file
#if 0
        // AM detector, and audio output
        for (int i = 0; i < n; i++) {
            y = cabs(data_lpf[i]);  // xxx scaling

            // xxx a product detector
            //   https://en.wikipedia.org/wiki/Product_detector
            // xxx this discusses quadrature detectors
            //   https://en.wikipedia.org/wiki/Direct-conversion_receiver
            // xxx wikipedia sdr
            //   https://en.wikipedia.org/wiki/Software-defined_radio
            // xxx qudrature mixers
            //   https://www.youtube.com/watch?v=JuuKF1RFvBM

            // lpf filter
            // https://www.youtube.com/watch?v=HJ-C4Incgpw
            // butterworth filter  4th order  or 50
            //   https://github.com/adis300/filter-c/blob/master/filter.c
            //   https://github.com/adis300/filter-c
            // https://exstrom.com/journal/sigproc/dsigproc.html

    
            if (y > 0) {
                yo = yo + (y - yo) * tc_k1;
            }

            if (cnt++ == (SAMPLE_RATE / 22000)) {  // xxx 22000 is the aplay rate
                audio_out(yo*tc_k2);  // xxx how to auto scale
                cnt = 0;
            }
        }
#endif

        // done with this data block, so increment head
        __sync_synchronize();
        Head++;
        __sync_synchronize();
    }

    return NULL;
}

void *get_data_thread(void *cx)
{
    int fd;
    struct stat statbuf;
    size_t file_offset, file_size, len;
    double t, delta_t, antenna[MAX_DATA], w;
    complex *data;

    fd = open(ANTENNA_FILENAME, O_RDONLY);
    if (fd < 0) {
        FATAL("failed open gen.dat, %s\n", strerror(errno));
    }

    fstat(fd, &statbuf);
    file_size = statbuf.st_size;
    file_offset = 0;
    t = 0;
    delta_t = (1. / SAMPLE_RATE);

    while (true) {
        // wait for a data block to be available
        while (Tail - Head == MAX_DATA_BLOCK) {
            usleep(1000);
        }

        // read MAX_DATA values from antenna file, these are real values
        len = pread(fd, antenna, MAX_DATA*sizeof(double), file_offset);
        if (len != MAX_DATA*sizeof(double)) {
            FATAL("read gen.dat, len=%ld, %s\n", len, strerror(errno));
        }
        file_offset += len;
        if (file_offset + len > file_size) {
            file_offset = 0;
        }

        // frequency shift the antenna data, and 
        // store result in Data[Tail], which are complex values
        data = Data[Tail%MAX_DATA_BLOCK];
        w = TWO_PI * (tc_freq - FREQ_OFFSET);
        for (int i = 0; i < MAX_DATA; i++) {
            data[i] = antenna[i] * cexp(-I * w * t);
            t += delta_t;
        }

        // increment Tail
        // xxx is sync_syncrhonize  needed
        __sync_synchronize();
        Tail++;
        __sync_synchronize();
    }

    return NULL;
}

