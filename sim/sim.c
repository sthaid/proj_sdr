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

// xxx print time in here
#define TIME(code) \
    ( { unsigned long start=microsec_timer(); code; (microsec_timer()-start)/1000000.; } )

#define FREQ_FIRST   50000
#define FREQ_LAST    200000
#define FREQ_STEP    10000
#define FREQ(n)  (FREQ_FIRST + (n) * FREQ_STEP)

//
// variables
//

char *progname = "sim";

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

complex *in, *out1, *out2, *out3;

int freq = FREQ_FIRST;

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

static fftw_plan   plan1;
static fftw_plan   plan2;
static fftw_plan   plan3;

int main(int argc, char **argv)
{
    pthread_t tid;

    // allocate in and out arrays in create the plan
    in  = (complex*)fftw_malloc(sizeof(complex) * N);
    out1 = (complex*)fftw_malloc(sizeof(complex) * N);
    out2 = (complex*)fftw_malloc(sizeof(complex) * N);
    out3 = (complex*)fftw_malloc(sizeof(complex) * N);
    memset(in, 0, sizeof(complex)*N);
    memset(out1, 0, sizeof(complex)*N);
    memset(out2, 0, sizeof(complex)*N);
    memset(out3, 0, sizeof(complex)*N);
    plan1 = fftw_plan_dft_1d(N, in, out1, FFTW_FORWARD, FFTW_ESTIMATE);
    plan2 = fftw_plan_dft_1d(N, out1, out2, FFTW_BACKWARD, FFTW_ESTIMATE);
    plan3 = fftw_plan_dft_1d(N, out2, out3, FFTW_FORWARD, FFTW_ESTIMATE);

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

struct pd_s {
    double data[N];
    //int max;
} pd_array[6];

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

#if 0
    init_sine_wave();
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < N; i++) {
        pd[i] = sine_wave(1,t);
        //NOTICE("t = %f  data = %f\n", t, pd[i]);
        t += (1. / N);
    }
    pthread_mutex_unlock(&mutex);
    NOTICE("work thread pauuse\n");
    pause();
#endif

    // init
    init_sine_wave();
    init_audio_src();

    // xxx comment
    while (true) {
        // modulate  3 stations
        y = 0;
        y += (1 + get_src(2,t)) * (0.5 * sine_wave(FREQ(0),t));  // music ...
        y += (1 + get_src(3,t)) * (0.5 * sine_wave(FREQ(1),t));
        y += (1 + get_src(4,t)) * (0.5 * sine_wave(FREQ(2),t));
        y += (1 + get_src(5,t)) * (0.5 * sine_wave(FREQ(3),t));
        y += (1 + get_src(0,t)) * (0.5 * sine_wave(FREQ(4),t));  // sine wave
        // xxx maybe white noise needs to be bw limitted
        //y += (1 + get_src(1,t)) * (0.5 * sine_wave(FREQ(5),t));  // white noise

        in[max_in++] = y;
        if (max_in == N) {
            struct pd_s *pd;
            max_in = 0;

            pthread_mutex_lock(&mutex);

            // air wave plot
            pd = &pd_array[0];
            for (int i = 0; i < N; i++) {
                pd->data[i] = in[i];
            }

            // fft,  creates out
            TIME(fftw_execute(plan1));
            pd = &pd_array[1];
            for (int i = 0; i < N; i++) {
                pd->data[i] = cabs(out1[i]);
            }

            // filter fft out2
            int freq_start = freq - 3000;
            int freq_end = freq + 3000;
            for (int i = 0; i < N/2; i++) {
                if (i < freq_start || i > freq_end) {
                    out1[i] = 0;
                    out1[N-1-i] = 0;
                }
            }

            // backward fft, creates out2
            TIME(fftw_execute(plan2));
            pd = &pd_array[2];
            for (int i = 0; i < N; i++) {
                pd->data[i] = creal(out2[i]) / N;
            }

            // look at the fft of the filtered data
            // forward fft, creates out3
#if 0
            for (int i = 0; i < N; i++) {
                out2[i] = creal(out2[i]);
            }
#endif
            TIME(fftw_execute(plan3));
            pd = &pd_array[3];
            for (int i = 0; i < N; i++) {
                pd->data[i] = cabs(out3[i]);
            }

            pd = &pd_array[4];
            for (int i = 0; i < N; i++) {
                y = creal(out2[i]) / 2138063;

                // detector
                if (y > yo) {
                    yo = y;
                }
                yo = .9995 * yo;
                pd->data[i] = yo;

                // audio out
                if (cnt++ == (SAMPLE_RATE / AUDIO_SAMPLE_RATE)) {
                    audio_out(yo);
                    cnt = 0;
                }
            }

            pthread_mutex_unlock(&mutex);
        }



        // advance time
        t += dt;
#if 0
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
#endif
    }

    return NULL;
}

    #define FONTSZ 20
    #define FONTSZ_LG 40

static int pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event)
{
    rect_t *pane = &pane_cx->pane;

#define SDL_EVENT_FREQ_DOWN    (SDL_EVENT_USER_DEFINED + 0)
#define SDL_EVENT_FREQ_UP      (SDL_EVENT_USER_DEFINED + 1)

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
        #define T_MAX .01
        struct pd_s *pd;
        pthread_mutex_lock(&mutex);

        // air wave
        NOTICE("air wave\n");
        pd = &pd_array[0];
        normalize(pd->data, (int)(T_MAX*N), -1, 1);
        plot(pane, 0,
             pd->data, (int)(T_MAX*N),   
             0, T_MAX,   
             -1, 1);

        // air wave fft
        NOTICE("air wave fft\n");
        pd = &pd_array[1];
        normalize(pd->data, 200000, 0, 1);
        plot(pane, 1,
             pd->data, 200000,   
             0, 200000,   
             0, 1);

        // filtered air wave
        NOTICE("filtered air wave\n");
        pd = &pd_array[2];
        normalize(pd->data, (int)(T_MAX*N), -1, 1);
        plot(pane, 2,
             pd->data, (int)(T_MAX*N),   
             0, T_MAX,   
             -1, 1);

        // filtered air wave fft
        NOTICE("filtered air wave fft\n");
        pd = &pd_array[3];
        normalize(pd->data, 200000, 0, 1);
        plot(pane, 3,
             pd->data, 200000,   
             0, 200000,   
             0, 1);

        // detected
        NOTICE("detected\n");
        pd = &pd_array[4];
        normalize(pd->data, (int)(T_MAX*N), -1, 1);
        plot(pane, 4,
             pd->data, (int)(T_MAX*N),   
             0, T_MAX,   
             -1, 1);

        pthread_mutex_unlock(&mutex);


        char str[100];
        sprintf(str, "%d", freq);
        sdl_render_text_and_register_event(
            pane, 
            0, pane->h-ROW2Y(1,FONTSZ_LG),
            FONTSZ_LG, str, SDL_LIGHT_BLUE, SDL_BLACK,
            SDL_EVENT_FREQ_DOWN, SDL_EVENT_TYPE_MOUSE_CLICK, pane_cx);

        sdl_render_text_and_register_event(
            pane, 
            COL2X(10,FONTSZ_LG), pane->h-ROW2Y(1,FONTSZ_LG),
            FONTSZ_LG, str, SDL_LIGHT_BLUE, SDL_BLACK,
            SDL_EVENT_FREQ_UP, SDL_EVENT_TYPE_MOUSE_CLICK, pane_cx);

        return PANE_HANDLER_RET_NO_ACTION;
    }

    // -----------------------
    // -------- EVENT --------
    // -----------------------

    if (request == PANE_HANDLER_REQ_EVENT) {
        switch (event->event_id) {
        case SDL_EVENT_FREQ_DOWN:
            freq = freq - FREQ_STEP;
            if (freq < FREQ_FIRST) freq = FREQ_LAST;
            break;
        case SDL_EVENT_FREQ_UP:
            freq = freq + FREQ_STEP;
            if (freq > FREQ_LAST) freq = FREQ_FIRST;
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
#if 1
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
