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

void zero_real(double *data, int n)
{
    memset(data, 0, n*sizeof(double));
}

void zero_complex(complex *data, int n)
{
    memset(data, 0, n*sizeof(complex));
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
    double *in_real;
    complex *in, *in_fft, *in_filtered, *in_filtered_fft;
    int current_mode;

    int sample_rate = 20000;
    int max = 120 * sample_rate;  // 120 secs of data
    int total = 0;
    int n = .1 * sample_rate;

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
    in_real         = fftw_alloc_real(max);
    in              = fftw_alloc_complex(n);
    in_fft          = fftw_alloc_complex(n);
    in_filtered     = fftw_alloc_complex(n);
    in_filtered_fft = fftw_alloc_complex(n);

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
            if (tc_reset) {
                tc_reset = 0;
                tc_ctr = DEFAULT_CTR;
                tc_width = DEFAULT_WIDTH;
            }

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
        }
    }        
}

// -----------------  ANTENNA TEST  -------------------------

#define ANTENNA_FILENAME "antenna.dat"
#define SAMPLE_RATE  2400000              // 2.4 MS/sec
#define MAX_N        (SAMPLE_RATE / 10)   // 0.1 sec range
#define DELTA_T      (1. / SAMPLE_RATE)

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
#define DEFAULT_K2   10.0
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
    complex  *data, *data_filtered, *data_fft, *data_filtered_fft;
    double    y, yo;
    int       cnt, n;
    pthread_t tid;

    static char tc_info[100];

    n                 = MAX_DATA;;
    data_filtered     = fftw_alloc_complex(n);
    data_filtered_fft = fftw_alloc_complex(n);
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
        fft_bpf_complex(data, data_filtered, data_fft, n, SAMPLE_RATE, 
                        FREQ_OFFSET-BPF_WIDTH/2, FREQ_OFFSET+BPF_WIDTH/2);
        plot_fft(0, data_fft, n, n/DATA_BLOCK_DURATION, "DATA_FFT", "HZ");  // xxx define for .1

#if 1
        // debug
        fft_fwd_c2c(data_filtered, data_filtered_fft, n);
        plot_fft(1, data_filtered_fft, n, n/DATA_BLOCK_DURATION, "DATA_FILTERED_FFT", "HZ");  // xxx define for .1
#endif

        // AM detector, and audio output
        for (int i = 0; i < n; i++) {
            y = cabs(data_filtered[i]);  // xxx scaling

            // xxx a product detector
            //   https://en.wikipedia.org/wiki/Product_detector
            // xxx this discusses quadrature detectors
            //   https://en.wikipedia.org/wiki/Direct-conversion_receiver
            // xxx wikipedia sdr
            //   https://en.wikipedia.org/wiki/Software-defined_radio
            // xxx qudrature mixers
            //   https://www.youtube.com/watch?v=JuuKF1RFvBM
    
            if (y > 0) {
                yo = yo + (y - yo) * tc_k1;
            }

            if (cnt++ == (SAMPLE_RATE / 22000)) {  // xxx 22000 is the aplay rate
                audio_out(yo*tc_k2);  // xxx how to auto scale
                cnt = 0;
            }
        }

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
    complex *data, synth0, synth90;

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
#if 0
            synth0  = sin(w * t);                                    // cos is the I
            synth90 = sin(w * t + M_PI_2);  // xxx or use cos        // sin i s the Q   imaginary
            data[i] = antenna[i] * (synth0 + I * synth90);
#endif
#if 0
            data[i] = antenna[i] * sin(w * t)  +     // I
                      antenna[i] * cos(w * t) * I;   // Q
#endif
#if 1
            data[i] = antenna[i] * cexp(-I * w * t);
#endif

            t += delta_t;
        }

        // increment Tail
        __sync_synchronize();
        Tail++;
        __sync_synchronize();
    }

    return NULL;
}

#if 0
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


unsigned long head;
unsigned long tail;
unsigned char data[MAX_DATA];  // xxx rename to Data ?

void *get_data_thread(void *cx);

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

// xxx handle the differnet modes

void init_get_data(void)
{
    pthread_t tid;

    // will support getting data from:
    // - file
    // - tcp connection to rtl_sdr_server
    // - directly from rtl_sdr device

    // for now, get data from gen.dat
    pthread_create(&tid, NULL, get_data_thread, NULL);
}

// xxx this won't be from file, it will just construct the data on the fly

void *get_data_thread(void *cx)
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

// xxx dont use all these defines
#define SAMPLE_RATE  2000000              // 2.0 MS/sec
#define W_SYNTH      (TWO_PI * 500000)    // 500 KHz
#define MAX_IQ       (SAMPLE_RATE / 10)   // 0.1 sec range
#define F_BPF        100000               // 100 KHz
#define DELTA_T      (1. / SAMPLE_RATE)

void *gen_test(void *cx)
{
    double   *antenna, t, synth0, synth90;
    complex  *antenna_fft, *iq, *iq_fft, *iq_filtered, *iq_filtered_fft;
    int      n=0, tmax;
    FILE    *fp;

    static char tc_info[100];

    init_antenna();

    antenna         = fftw_alloc_real(MAX_IQ);    // xxx not needed as an array
    antenna_fft     = fftw_alloc_complex(MAX_IQ); // xxx not needed as an array
    iq              = fftw_alloc_complex(MAX_IQ);
    iq_fft          = fftw_alloc_complex(MAX_IQ);
    iq_filtered     = fftw_alloc_complex(MAX_IQ);
    iq_filtered_fft = fftw_alloc_complex(MAX_IQ);

    // xxx
    tmax = (arg1 == -1 ? 10 : arg1);

    // init test controls
    tc.name = "GEN";
    tc.info = tc_info;

    // xxx
    fp = fopen("gen.dat", "w");
    if (fp == NULL) {
        FATAL("failed to create gen.dat\n");
    }

    // xxx
    for (t = 0; t < tmax; t += DELTA_T) {
        // xxx
        antenna[n] = get_antenna(t);
        synth0     = sin((W_SYNTH) * t);
        synth90    = sin((W_SYNTH) * t + M_PI_2);
        iq[n]      = antenna[n] * (synth0 + I * synth90);
        n++;

        // xxx
        if (n == MAX_IQ) {
            sprintf(tc_info, "%0.1f secs", t);

            fft_fwd_r2c(antenna, antenna_fft, n);
            fft_bpf_complex(iq, iq_filtered, iq_fft, n, SAMPLE_RATE, -F_BPF, +F_BPF);
            fft_fwd_c2c(iq_filtered, iq_filtered_fft, n);

            plot_fft(0, antenna_fft, n, n/.1, "ANTENNA_FFT", "HZ");  // xxx define for .1
            plot_fft(1, iq_fft, n, n/.1, "IQ_FFT", "HZ");  // xxx define for .1
            plot_fft(2, iq_filtered_fft, n, n/.1, "IQ_FILTERED_FFT", "HZ");  // xxx define for .1

            fwrite(iq_filtered, sizeof(complex), n, fp);

            n = 0;
        }
    }

    fclose(fp);

#if 0
    // normalize the file
    fd = open(FILENAME, O_RDWR);
    if (fd < 0) {
        FATAL("failed to open %s, ^s\n", FILENAME, strerror(errno));
    }
#endif

    return NULL;
}

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
void *get_data_thread(void *cx);

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

// xxx handle the differnet modes

void init_get_data(void)
{
    pthread_t tid;

    // will support getting data from:
    // - file
    // - tcp connection to rtl_sdr_server
    // - directly from rtl_sdr device

    // for now, get data from gen.dat
    pthread_create(&tid, NULL, get_data_thread, NULL);
}

// xxx this won't be from file, it will just construct the data on the fly

void *get_data_thread(void *cx)
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

int play_cb_test(void *data, void *cx_arg)
{
    static int idx;

    if (idx && (idx%22000) == 0) sleep(1);

    *(float*)data = ((float*)cx_arg)[idx++];
    return 0;
}
    //pa_print_device_info_all();
    //int idx = pa_find_device(DEFAULT_OUTPUT_DEVICE);
    //NOTICE("XXX idx %d\n", idx);
    float *data;
    int ret, num_chan, num_items, sample_rate;
    ret = read_wav_file_float("super_critical.wav", &data, &num_chan, &num_items, &sample_rate);
    if (ret != 0) {
        FATAL("read_wav_file_float\n");
    }
    ret = pa_play(DEFAULT_OUTPUT_DEVICE, num_chan, num_items, sample_rate, PA_FLOAT32, data);
    if (ret != 0) {
        FATAL("pa_play\n");
    }
#endif
