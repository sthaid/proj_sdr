// xxx put tick marks on plot
// - adjust the cutoff frequency
// xxx comments

#include "common.h"

//
// defines
//

#define MAX_TESTS (sizeof(tests)/sizeof(tests[0]))

#define PLOT(_idx,_data,_n,_xvmin,_xvmax,_yvmin,_yvmax,_title) \
    do { \
        plots_t *p = &plots[_idx]; \
        p->data    = _data; \
        p->n       = _n; \
        p->xv_min  = _xvmin; \
        p->xv_max  = _xvmax; \
        p->yv_min  = _yvmin; \
        p->yv_max  = _yvmax; \
        p->title   = _title; \
    } while (0)

#define PLOT_COMPLEX(_idx,_data_complex,_n,_xvmin,_xvmax,_yvmin,_yvmax,_title) \
    do { \
        plots_t *p = &plots[_idx]; \
        double *data = malloc(n*sizeof(double)); \
        for (int i = 0; i < _n; i++) { \
            data[i] = cabs(_data_complex[i]); \
        } \
        normalize(data, n, _yvmin, _yvmax); \
        p->data    = data; \
        p->n       = _n; \
        p->xv_min  = _xvmin; \
        p->xv_max  = _xvmax; \
        p->yv_min  = _yvmin; \
        p->yv_max  = _yvmax; \
        p->title   = _title; \
    } while (0)

//
// typedefs
//

typedef struct {
    double *data;
    int     n;
    double  xv_min;
    double  xv_max;
    double  yv_min;
    double  yv_max;
    char   *title;
} plots_t;

//
// variables
//

char *progname = "ut";
bool  debug;

plots_t plots[MAX_PLOT];

int test_param;

//
// prototypes
//

void usage(void);
int pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event);

void *plot_test(void *cx);
void *lpf_test(void *cx);
void *audio_test(void *cx);

//
// test table
//

static struct test_s {
    char *name;
    void *(*proc)(void *cx);
} tests[] = {
        { "plot", plot_test },
        { "lpf",  lpf_test  },
        { "audio",  audio_test  },
                };

// -----------------  MAIN  --------------------------------

int main(int argc, char **argv)
{
    int i;
    char *name;
    struct test_s *t;
    pthread_t tid;

    init_fft();  // xxx is this needed

    if (argc == 1) {
        usage();
        return 1;
    }
    name = argv[1];
    
    for (i = 0; i < MAX_TESTS; i++) {
        t = &tests[i];
        if (strcmp(name, t->name) == 0) {
            break;
        }
    }
    if (i == MAX_TESTS) {
        FATAL("test '%s' not found\n", name);
    }

    NOTICE("Running '%s' test\n", t->name);
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
        NOTICE("  %s\n", t->name);
    }
}

int pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event)
{
    rect_t *pane = &pane_cx->pane;

    #define SDL_EVENT_tbd  (SDL_EVENT_USER_DEFINED + 0)

    // ----------------------------
    // -------- INITIALIZE --------
    // ----------------------------

    if (request == PANE_HANDLER_REQ_INITIALIZE) {
        return PANE_HANDLER_RET_NO_ACTION;
    }

    // ------------------------
    // -------- RENDER --------
    // ------------------------

    if (request == PANE_HANDLER_REQ_RENDER) {
        for (int i = 0; i < MAX_PLOT; i++) {
            plots_t *p = &plots[i];
            if (p->data) {
                sdl_plot(pane, i,
                        p->data, p->n,
                        p->xv_min, p->xv_max,
                        p->yv_min, p->yv_max,
                        p->title);
            }
        }

        sdl_render_printf(pane, 
                          0, pane->h-ROW2Y(1,20), 20,
                          SDL_WHITE, SDL_BLACK, "<-  %d  ->", test_param);

        return PANE_HANDLER_RET_NO_ACTION;
    }

    // -----------------------
    // -------- EVENT --------
    // -----------------------

    if (request == PANE_HANDLER_REQ_EVENT) {
        switch (event->event_id) {
        case SDL_EVENT_KEY_LEFT_ARROW:
            test_param -= 100;  // xxx step
            break;
        case SDL_EVENT_KEY_RIGHT_ARROW:
            test_param += 100;
            break;
        case SDL_EVENT_tbd:
            break;
        default:
            break;
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

void add_sine_wave(int sample_rate, int f, int n, double *data)
{
    double  w = TWO_PI * f;
    double  t = 0;
    double  dt = (1. / sample_rate);
    int     i;

    for (i = 0; i < n; i++) {
        data[i] += sin(w * t);
        t += dt;
    }
}

void zero_real(double *data, int n)
{
    memset(data, 0, n*sizeof(double));
}

// -----------------  PLOT TEST  ---------------------------

void *plot_test(void *cx)
{
    int sample_rate, f, n;
    double *data;

    #define SECS 1

    sample_rate = 24000;
    f = 10;
    n = SECS * sample_rate;
    data = fftw_alloc_real(n);
    zero_real(data, n);

    add_sine_wave(sample_rate, f, n, data);

    PLOT(0, data, n,  0, 1,  -1, +1,  "SINE WAVE");
    PLOT(1, data, n,  0, 1,  -0.5, +0.5,  "SINE WAVE");
    PLOT(2, data, n,  0, 1,  0.5, +1.5,  "SINE WAVE");

    return NULL;
}

// -----------------  LPF TEST  ---------------------------

void *lpf_test(void *cx)
{
    int sample_rate, f, n;
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
    zero_real(in, n);
    for (f = 990; f <= 12000; f += 1000) {
        add_sine_wave(sample_rate, f, n, in);
    }
    normalize(in, n, -1, 1);
    for (int i = 0; i < n; i++) {
        in_complex[i] = in[i];
    }

    // perfrom fwd fft on 'in' to 'in_fft', and plot
    fft_fwd_r2r(in, in_fft, n);
    normalize(in_fft, n, 0, 1);
    PLOT(0, in_fft, n,  0, n,  0, +1,  "IN_FFT");

    // perform lpf on 'in' to 'lpf', and plot the fft
    fft_lpf_real(in, lpf, n, sample_rate, 1500);
    fft_fwd_r2r(lpf, lpf_fft, n);
    normalize(lpf_fft, n, 0, 1);
    PLOT(1, lpf_fft, n,  0, n,  0, +1,  "LPF_FFT");

    // perfrom fwd fft on 'in_complex' to 'in_fft_complex', and plot
    fft_fwd_c2c(in_complex, in_fft_complex, n);
    PLOT_COMPLEX(2, in_fft_complex, n,  0, n,  0, +1,  "IN_FFT_COMPLEX");

    // perform lpf on 'in_complex' to 'lpf_complex' and plot the fft
    fft_lpf_complex(in_complex, lpf_complex, n, sample_rate, 1500);
    fft_fwd_c2c(lpf_complex, lpf_fft_complex, n);
    PLOT_COMPLEX(3, lpf_fft_complex, n,  0, n,  0, +1,  "LPF_FFT_COMPLEX");

    return NULL;
}

// -----------------  AUDIO TEST  ---------------------------

void *audio_test(void *cx)
{
    double *data;
    int ret, num_chan, num_items, sample_rate, n, idx;

    // read wav file
    ret = read_wav_file("super_critical.wav", &data, &num_chan, &num_items, &sample_rate);
    if (ret != 0 || num_chan != 1) {
        FATAL("audio_test ret=%d num_chan=%d\n", ret, num_chan);
    }
    NOTICE("num_chan=%d num_items=%d sample_rate=%d\n", num_chan, num_items, sample_rate);

    fft_lpf_real(data, data, num_items, sample_rate, 4000); // xxx ?
    normalize(data, num_items, -1, 1);

    test_param = 10000;

    n = sample_rate / 10;
    idx = 0;

    double *in = fftw_alloc_real(n);
    double *out = fftw_alloc_real(n);
    double *out_fft = fftw_alloc_real(n);

    // loop forever, writing wav file audio to stdout
    while (true) {
        float audio_out[n];

        memcpy(in, data+idx, n*sizeof(double));

        int lpf_freq = test_param;
        if (lpf_freq < 1000) lpf_freq = 1000;
        if (lpf_freq > 10000) lpf_freq = 10000;
        test_param = lpf_freq;

        if (lpf_freq < 10000) {
            fft_lpf_real(in, out, n, sample_rate, lpf_freq);
            for (int i = 0; i < n; i++) {
                out[i] /= n;
            }
        } else {
            NOTICE("NO FILTERING\n");
            memcpy(out, in, n*sizeof(double));
        }

        fft_fwd_r2r(out, out_fft, n);
        normalize(out_fft, n, 0, 1);
        PLOT(0, out_fft, n,  0, sample_rate,  0, 1, "XXX");

        for (int i = 0; i < n; i++) {
            audio_out[i] = out[i];
        }
        fwrite(audio_out, sizeof(float), n, stdout);

        idx += n;
        if (idx + n >= num_items) {
            idx = 0;
        }
    }
}

