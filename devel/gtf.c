// gtf = gen test file
// xxx rename to sim

#include "common.h"

//
// defines
//

#define FONTSZ 20

#if 0
#define SECS         1
#define SAMPLE_RATE  28800000  // 28.8 MS/s
#define DELTA_T      (1. / SAMPLE_RATE)
#define N            (SAMPLE_RATE * SECS)

#define SIG_HZ1  4500000  // 4.5 MHz
#define SIG_HZ2  5500000  // 5.5 MHz
#define SYNTH_HZ 5000000
#else
#define SECS         1
#define SAMPLE_RATE  2000000
#define DELTA_T      (1. / SAMPLE_RATE)
#define N            (SAMPLE_RATE * SECS)

#define SIG_HZ1  450000
#define SIG_HZ2  475000
#define SIG_HZ3  500000
#define SIG_HZ4  525000
#define SIG_HZ5  550000

#define SYNTH_HZ 500000
#endif

//
// variables
//

char *progname = "gtf";
bool  debug;

complex *sig;
complex *sig_freq;
double  *sig_freq_pd;

complex *iq;
complex *iq_freq;
double  *iq_freq_pd;

complex *iq_lpf;
complex *iq_lpf_freq;
double  *iq_lpf_freq_pd;

fftw_plan plan0;
fftw_plan plan1;
fftw_plan plan2;
fftw_plan plan3;

//
// prototypes
//

static int pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event);

static int plot(rect_t *pane, int idx,
                double *data, int n,
                double xv_min, double xv_max,
                double yv_min, double yv_max);


// -----------------  MAIN  --------------------------------------------

int main(int argc, char **argv)
{
    double t = 0;
    long i;

    sig        = (complex*)fftw_malloc(sizeof(complex) * N);
    sig_freq    = (complex*)fftw_malloc(sizeof(complex) * N);
    sig_freq_pd = (double*)calloc(N, sizeof(double));

    iq         = (complex*)fftw_malloc(sizeof(complex) * N);
    iq_freq     = (complex*)fftw_malloc(sizeof(complex) * N);
    iq_freq_pd  = (double*)calloc(N, sizeof(double));

    iq_lpf          = (complex*)fftw_malloc(sizeof(complex) * N);
    iq_lpf_freq     = (complex*)fftw_malloc(sizeof(complex) * N);
    iq_lpf_freq_pd  = (double*)calloc(N, sizeof(double));

    for (i = 0; i < N; i++) {
        sig[i] = 
           0.20 * sin((TWO_PI * SIG_HZ1) * ((double)i / SAMPLE_RATE)) + 
           0.40 * sin((TWO_PI * SIG_HZ2) * ((double)i / SAMPLE_RATE)) + 
           0.60 * sin((TWO_PI * SIG_HZ3) * ((double)i / SAMPLE_RATE)) + 
           0.80 * sin((TWO_PI * SIG_HZ4) * ((double)i / SAMPLE_RATE)) + 
           1.00 * sin((TWO_PI * SIG_HZ5) * ((double)i / SAMPLE_RATE));

        double synth0 = sin((TWO_PI * SYNTH_HZ) * ((double)i / SAMPLE_RATE));
        double synth90 = sin((TWO_PI * SYNTH_HZ) * ((double)i / SAMPLE_RATE) + M_PI_2);

        iq[i] = sig[i] * synth0 + I * sig[i] * synth90;

        t += DELTA_T;
    }

    // create sig_freq
    plan0 = fftw_plan_dft_1d(N, sig, sig_freq, FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_execute(plan0);
    for (i = 0; i < N; i++) {
        sig_freq_pd[i] = cabs(sig_freq[i]);
    }
    normalize(sig_freq_pd, N, 0, 1);

    // create iq_freq
    plan1 = fftw_plan_dft_1d(N, iq, iq_freq, FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_execute(plan1);
    for (i = 0; i < N; i++) {
        iq_freq_pd[i] = cabs(iq_freq[i]);
    }
    normalize(iq_freq_pd, N, 0, 1);

    // create iq_lpf
    for (i = 60000; i <= N-60000; i++) {
        iq_freq[i] = 0;
    }
    plan2 = fftw_plan_dft_1d(N, iq_freq, iq_lpf, FFTW_BACKWARD, FFTW_ESTIMATE);
    fftw_execute(plan2);

    // create iq_lpf_freq
    plan3 = fftw_plan_dft_1d(N, iq_lpf, iq_lpf_freq, FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_execute(plan3);
    for (i = 0; i < N; i++) {
        iq_lpf_freq_pd[i] = cabs(iq_lpf_freq[i]);
    }
    normalize(iq_lpf_freq_pd, N, 0, 1);
    
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
        1000000,          // 0=continuous, -1=never, else us
        1,              // number of pane handler varargs that follow
        pane_hndlr, NULL, 0, 0, win_width, win_height, PANE_BORDER_STYLE_NONE);

    return 0;
}

#if 0
// -----------------  WORK THREAD  -------------------------------------
//https://dsp.stackexchange.com/questions/51889/what-exactly-is-a-90-degree-phase-shift-of-a-digital-signal-in-fm-demodulation-a
// https://rahsoft.com/2022/10/17/understanding-90%cb%9a-phase-shift-and-hilbert-transform/

void *work_thread(void * cx)
{
}
#endif

// -----------------  PANE HANDLER  ------------------------------------

static int pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event)
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
#if 0
        sdl_render_printf(pane,
                        100, 100,
                        FONTSZ, SDL_GREEN, SDL_BLACK, "%s", "Hello");
#endif
        plot(pane, 0, sig_freq_pd, N, 
             0, 1,
             0, 1);

        plot(pane, 1, iq_freq_pd, N, 
             0, 1,
             0, 1);

        plot(pane, 2, iq_lpf_freq_pd, N, 
             0, 1,
             0, 1);

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


static int plot(rect_t *pane, int idx, 
                double *data, int n, 
                double xv_min, double xv_max, 
                double yv_min, double yv_max)
{
    #define MAX_YV 4000

    struct {
        double min;
        double max;
    } yv[MAX_YV];
    double yv_span, y, y_min, y_max;
    int x_origin, x_end, x_span;
    int y_top, y_bottom, y_span, y_origin;
    int x, i;
    char s[100];

    #define MAX_PLOT 6

    // init
    yv_span  = yv_max - yv_min;

    x_origin = 20;
    x_end    = pane->w - 20;
    x_span   = x_end - x_origin;

    y_top    = idx * pane->h / MAX_PLOT;
    y_bottom = (idx + 1) * pane->h / MAX_PLOT - 40;
    y_span   = y_bottom - y_top;
    y_origin = y_top + (yv_max / yv_span) * y_span;

    for (i = 0; i < MAX_YV; i++) {
        yv[i].min = 1e99;
        yv[i].max = -1e99;
    }

    // determine yv min/max
    for (i = 0; i < n; i++) {
        x = (long)i * x_span / n;
        y = data[i];
        if (y > yv[x].max) yv[x].max = y;
        if (y < yv[x].min) yv[x].min = y;
    }

    // limit yv to caller supplied min/max
    for (x = 0; x < x_span; x++) {
        if (yv[x].max == -1e99) {
            continue;
        }
        if (yv[x].max > yv_max) yv[x].max = yv_max;
        if (yv[x].max < yv_min) yv[x].max = yv_min;

        if (yv[x].min > yv_max) yv[x].min = yv_max;
        if (yv[x].min < yv_min) yv[x].min = yv_min;
    }

    // plot
    for (x = 0; x < x_span; x++) {
        if (yv[x].max == -1e99) {
            continue;
        }
        y_min = yv[x].min * (y_span / yv_span);
        y_max = yv[x].max * (y_span / yv_span);
        sdl_render_line(pane, 
                        x_origin+x, y_origin-y_min,
                        x_origin+x, y_origin-y_max,
                        SDL_WHITE);
    }

#if 0
    // x axis
    if (y_origin <= y_bottom && y_origin >= y_top) {
        sprintf(s, "%0.2f", xv_min);
        sdl_render_printf(pane, 
                        x_origin, y_origin,
                        FONTSZ, SDL_GREEN, SDL_BLACK, "%s", s);

        sprintf(s, "%0.2f", xv_max);
        sdl_render_printf(pane, 
                        x_end-COL2X(strlen(s),FONTSZ), y_origin,
                        FONTSZ, SDL_GREEN, SDL_BLACK, "%s", s);

        sdl_render_line(pane, 
                        x_origin, y_origin, 
                        x_origin + x_span, y_origin,
                        SDL_GREEN);
    }

    // y axis
    sdl_render_printf(pane, 
                      x_origin+3, y_top, 
                      FONTSZ, SDL_GREEN, SDL_BLACK, "%0.2f", yv_max);
    sdl_render_printf(pane, 
                      x_origin+3, y_bottom-ROW2Y(1,FONTSZ)+0, 
                      FONTSZ, SDL_GREEN, SDL_BLACK, "%0.2f", yv_min);

    sdl_render_line(pane, 
                    x_origin, y_top,
                    x_origin, y_bottom,
                    SDL_GREEN);
#endif

    return 0; //xxx
}
