#include "common.h"

#include <util_sdl.h>


//
// defines 
//

#define KHZ 1000

#define SAMPLE_RATE 2400000
#define AUDIO_SAMPLE_RATE 22000
#define N (SAMPLE_RATE / 1)
//#define N 100

#define FC0      0
#define FC1  50000
#define FC2 150000

#define TIME(code) \
    ( { unsigned long start=microsec_timer(); code; (microsec_timer()-start)/1000000.; } )


//
// variables
//

char *progname = "sim";

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

complex *in, *out;

//
// prototypes
//

void audio_out(double yo);
static int pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event);
void *work_thread(void *cx);
static int plot(rect_t *pane, int idx, 
                double *data, int n, 
                double xv_min, double xv_max, 
                double yv_min, double yv_max);

// -----------------  MAIN  -------------------------------------------

static fftw_plan   plan;
static fftw_plan   plan2;

int main(int argc, char **argv)
{
    pthread_t tid;

    // allocate in and out arrays in create the plan
    in  = (complex*)fftw_malloc(sizeof(complex) * N);
    out = (complex*)fftw_malloc(sizeof(complex) * N);
    memset(in, 0, sizeof(complex)*N);
    memset(out, 0, sizeof(complex)*N);
    plan = fftw_plan_dft_1d(N, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
    plan2 = fftw_plan_dft_1d(N, out, in, FFTW_BACKWARD, FFTW_ESTIMATE);

    // xxx
    pthread_create(&tid, NULL, work_thread, NULL);

#if 1
    // init sdl
    static int         win_width = 1600;
    static int         win_height = 800;
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
#else
    while (1)
    pause();
#endif
}

double plot_test[N];

void *work_thread(void *cx)
{
    double dt = 1. / SAMPLE_RATE;
    double yo = 0;
    double t = 0;
    int    cnt = 0;
    double y;
    double t1, t2;
    int max_in = 0;

    NOTICE("work thread start\n");

    init_sine_wave();

    pthread_mutex_lock(&mutex);
    for (int i = 0; i < N; i++) {
        plot_test[i] = sine_wave(1,t);
        //NOTICE("t = %f  data = %f\n", t, plot_test[i]);
        t += (1. / N);
    }
    pthread_mutex_unlock(&mutex);

    NOTICE("work thread pauuse\n");
    pause();





    // init
    init_sine_wave();
    init_audio_src();

    // xxx comment
    while (true) {
        // modulate
        y = 0;
#if 1
        y += (1 + get_src(1,t));
        y += (1 + get_src(0,t)) * (0.5 * sine_wave(FC1,t));
        y += (1 + get_src(2,t)) * (0.5 * sine_wave(FC2,t));
#else
        y += sine_wave(1,t);
        //y += (1) * (0.5 * sine_wave(FC2,t));
#endif

        in[max_in++] = y;
        if (max_in < N) {
            t += dt;
            continue;
        }
        max_in = 0;

        pthread_mutex_lock(&mutex);
#if 1
        t1 = TIME(fftw_execute(plan));
        t2 = TIME(fftw_execute(plan2));
        for (int i = 0; i < N; i++) {
            in[i] /= (N);
        }
#endif
        pthread_mutex_unlock(&mutex);
        NOTICE("t1 = %f  t2 = %f\n", t1, t2);

#if 1
        double max = 0;
        for (int i = 0; i < N; i++) {
            y = cabs(in[i]);
            if (y > max) max = y;
        }
        NOTICE("max = %f\n", max);
#endif

        for (int i = 0; i < N; i++) {
            y = cabs(in[i]);
            y =  lpf(y,1,.999);

            // detector
            if (y > yo) {
                yo = y;
            }
            yo = .999 * yo;

            // audio out
            if (cnt++ == (SAMPLE_RATE / AUDIO_SAMPLE_RATE)) {
                audio_out(yo);
                cnt = 0;
            }
        }

        // advance time
        t += dt;
    }

    return NULL;
}

static int pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event)
{
    rect_t *pane = &pane_cx->pane;

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
        pthread_mutex_lock(&mutex);
        //plot(pane, 0, out, N);
        //plot(pane, 0, in, N);

        plot(pane, 0,   plot_test, N,   0, 1,   -1, 1);
        plot(pane, 1,   plot_test, N,   0, 1,   0, 2);
        plot(pane, 2,   plot_test, N,   0, 1,   0.5, 1);

        pthread_mutex_unlock(&mutex);
        return PANE_HANDLER_RET_NO_ACTION;
    }

    // -----------------------
    // -------- EVENT --------
    // -----------------------

    if (request == PANE_HANDLER_REQ_EVENT) {
#if 0
        switch (event->event_id) {
        case xxx:
        default:
        }
#endif
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
    #define FONTSZ 20

    struct {
        double min;
        double max;
    } yv[MAX_YV];
    double yv_span, y, y_min, y_max;
    int x_origin, x_end, x_span;
    int y_top, y_bottom, y_span, y_origin;
    int x, i;
    char s[100];

    // init
    yv_span  = yv_max - yv_min;

    x_origin = 20;
    x_end    = pane->w - 20;
    x_span   = x_end - x_origin;

    y_top    = idx * pane->h / 4;
    y_bottom = (idx + 1) * pane->h / 4 - 20;
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

    // x axis
    if (y_origin <= y_bottom && y_origin >= y_top) {
        sprintf(s, "%0.2f", xv_max);
        sdl_render_printf(pane, 
                        x_end-COL2X(strlen(s),FONTSZ), y_origin-ROW2Y(1,FONTSZ)+0,
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

    return 0; //xxx
}


#if 0
    // x axis
    int color = (idx == audio_out_filter ? SDL_BLUE : SDL_GREEN);
    sdl_render_line(pane, 0, y_origin, x_max, y_origin, color);
    for (freq = 200; freq <= MAX_PLOT_FREQ-200; freq+= 200) {
        x = freq / MAX_PLOT_FREQ * x_max;
        sprintf(str, "%d", (int)(nearbyint(freq)/100));
        sdl_render_text(pane,
            x-COL2X(strlen(str),20)/2, y_origin+1, 20, str, color, SDL_BLACK);
    }
#endif

// -----------------  xxx  --------------------------------------------

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

// -----------------  xxx  --------------------------------------------
// -----------------  save --------------------------------------------
#if 0
double de_modulate(double y, double ftune, double t)
{
    double tmp;

    tmp = y * sine_wave(ftune,t);
    //return tmp;

    return lpf(tmp,1,.90);
}

    // xxx cleanup
    int cnt = 0;
    int num_usleep = 0;
    unsigned long start_us, t_us, real_us;
    start_us = microsec_timer();

        if (t > 5) {
            NOTICE("5 secs %d\n", num_usleep);
            exit(1);
        }

        if (cnt++ > 1000) {
            t_us = t * 2000;
            real_us = microsec_timer() - start_us;
            if (t_us > real_us + 1000) {
                usleep(t_us - real_us);
                num_usleep++;
            }
            cnt = 0;
        }
double modulate(double m, double fc, double t)
{
    double y;

    y = (1 + m) * (0.5 * sine_wave(fc,t));  // xxx optimize

    return y;
}

double average(float *array, int cnt)
{
    double sum = 0;
    for (int i = 0; i < cnt; i++) {
        sum += array[i];
    }
    return sum / cnt;
}

double moving_avg(float v)
{
    #define MAX_VALS 10000

    static float vals[MAX_VALS];
    static int idx;
    static double sum;

    sum += (v - vals[idx]);
    vals[idx] = v;
    if (++idx == MAX_VALS) idx = 0;
    return sum / MAX_VALS;
}
#endif
