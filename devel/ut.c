#include "common.h"

//
// defines
//

#define MAX_TESTS (sizeof(tests)/sizeof(tests[0]))

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

//
// prototypes
//

void usage(void);
int pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event);

void *plot_test(void *cx);
void *lpf_test(void *cx);

//
// test table
//

static struct test_s {
    char *name;
    void *(*proc)(void *cx);
} tests[] = {
        { "plot", plot_test },
        { "lpf",  lpf_test  },
                };

// -----------------  MAIN  --------------------------------

// xxx comments
int main(int argc, char **argv)
{
    int i;
    char *name;
    struct test_s *t;
    pthread_t tid;

    init_fft();

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
        10000,          // 0=continuous, -1=never, else us
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

    #define SDL_EVENT_xxx  (SDL_EVENT_USER_DEFINED + 0)

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

        return PANE_HANDLER_RET_NO_ACTION;
    }

    // -----------------------
    // -------- EVENT --------
    // -----------------------

    if (request == PANE_HANDLER_REQ_EVENT) {
        switch (event->event_id) {
        case SDL_EVENT_xxx:
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

// xxx macros for plot
void *plot_test(void *cx)
{
    int sample_rate, f, n;
    double *data;
    plots_t *p;

    #define SECS 1

    sample_rate = 24000;
    f = 10;
    n = SECS * sample_rate;
    data = fftw_alloc_real(n);
    zero_real(data, n);

    add_sine_wave(sample_rate, f, n, data);

    p = &plots[0];
    p->data    = data;
    p->n       = n;
    p->xv_min  = 0;
    p->xv_max  = 1;
    p->yv_min  = -1;
    p->yv_max  = +1;
    p->title   = "SINE WAVE";

    p = &plots[1];
    p->data    = data;
    p->n       = n;
    p->xv_min  = 0;
    p->xv_max  = 1;
    p->yv_min  = -0.5;
    p->yv_max  = +0.5;
    p->title   = "SINE WAVE";

    p = &plots[2];
    p->data    = data;
    p->n       = n;
    p->xv_min  = 0;
    p->xv_max  = 1;
    p->yv_min  = 0.5;
    p->yv_max  = 1.5;
    p->title   = "SINE WAVE";

    return NULL;
}


// -----------------  LPF TEST  ---------------------------

void *lpf_test(void *cx)
{
    int sample_rate, f, n;
    double *in, *lpf, *out, *out2;
    plots_t *p;

    #define SECS 1

    sample_rate = 24000;
    n = SECS * sample_rate;
    in = fftw_alloc_real(n);
    lpf = fftw_alloc_real(n);
    out = fftw_alloc_real(n);
    out2 = fftw_alloc_real(n);
    zero_real(in, n);

    // - - - - - -

    // create sum of sine waves, to 'in' buff
    for (f = 1000; f <= 12000; f += 1000) {
        add_sine_wave(sample_rate, f, n, in);
    }
    normalize(in, n, -1, 1);

    // plot 'in' buff
    p = &plots[0];
    p->data    = in;
    p->n       = n / 100;
    p->xv_min  = 0;
    p->xv_max  = 1 / 100.;
    p->yv_min  = -1;
    p->yv_max  = +1;
    p->title   = "SUM OF SINE WAVES";

    // - - - - - -

    // perfrom fwd fft on 'in' to 'out';
    fft_fwd_r2r(in, out, n);
    normalize(out, n, 0, 1);

    // plot 'out'
    p = &plots[1];
    p->data    = out;
    p->n       = n;
    p->xv_min  = 0;
    p->xv_max  = n / 2;
    p->yv_min  = 0;
    p->yv_max  = +1;
    p->title   = "LPF 1500";

    // - - - - - -

    // perform lpf on 'in' to 'lpf'
    lpf_real(in, lpf, n, sample_rate, 1500, "LPF");

    // - - - - - -

    // perfrom fwd fft on 'lpf' to 'out2';
    fft_fwd_r2r(lpf, out2, n);
    normalize(out2, n, 0, 1);
    // xxx streamline the plot with a macro

    // plot 'out'
    p = &plots[2];
    p->data    = out2;
    p->n       = n;
    p->xv_min  = 0;
    p->xv_max  = n / 2;
    p->yv_min  = 0;
    p->yv_max  = +1;
    p->title   = "LPF 1500";

    return NULL;
}
