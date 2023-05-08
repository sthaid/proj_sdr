// xxx later
// - comments and cleanup
// - are _sync_cynchronize needed

// xxx first
// - loc of red dot in plot_test

#include "common.h"

//
// defines
//

#define USE_PA

#define CTRL SDL_EVENT_KEY_CTRL
#define ALT  SDL_EVENT_KEY_ALT

#define MAX_PLOT       10

#define min(a,b) ((a) < (b) ? (a) : (b))

//
// typedefs
//

typedef struct {
    double  *data;
    int     n;
    double  xv_min;
    double  xv_max;
    double  xv_blue_cursor;
    double  xv_red_cursor;
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

static void usage(void);
static int pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event);
static void init_audio_out(void);

//
// test table
//

static struct test_s {
    char *test_name;
    void *(*proc)(void *cx);
} tests[] = {
        { "plot",    plot_test  },
        { "filter",  filter_test   },
        { "ssb",     ssb_test   },
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
    static int win_width = 1680;
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

    // set program_terminating flag
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

// -----------------  PANE HANDLER  ------------------------

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
                         p->xv_min, p->xv_max, p->xv_blue_cursor, p->xv_red_cursor,
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
                          0, pane->h-ROW2Y(7,FONTSZ), FONTSZ,
                          SDL_WHITE, SDL_BLACK, "%s" , str);

        for (int i = 0; i < MAX_CTRL; i++) {
            struct test_ctrl_s *x = &tc.ctrl[i];
            int xpos, ypos;

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
                double v = *x->val;
                if (fabs(v) < 1e3) {
                    p += sprintf(p, "%0.3f ", v);
                } else if (fabs(v) < 10e6) {
                    p += sprintf(p, "%0.1f K", v/1000);
                } else if (fabs(v) < 10e9) {
                    p += sprintf(p, "%0.1f M", v/1000000);
                } else {
                    p += sprintf(p, "%g ", *x->val);
                }
            }
            if (x->units) {
                p += sprintf(p, "%s", x->units);
            }

            if (i < MAX_CTRL/2) {
                xpos = COL2X(20*i,FONTSZ);
                ypos = pane->h-ROW2Y(5,FONTSZ);
            } else {
                xpos = COL2X(20*(i-MAX_CTRL/2),FONTSZ);
                ypos = pane->h-ROW2Y(2,FONTSZ);
            }
            sdl_render_printf(pane, xpos, ypos, FONTSZ, SDL_WHITE, SDL_BLACK, "%s" , str);
        }

        for (int i = 0; i < MAX_CTRL; i++) {
            struct test_ctrl_s *x = &tc.ctrl[i];
            int xpos, ypos;

            if (x->val == NULL) continue;

            str[0] = '\0';
            if (x->decr_event == SDL_EVENT_KEY_DOWN_ARROW && x->incr_event == SDL_EVENT_KEY_UP_ARROW) {
                sprintf(str, "v ^");
            } else if (x->decr_event == SDL_EVENT_KEY_SHIFT_DOWN_ARROW && x->incr_event == SDL_EVENT_KEY_SHIFT_UP_ARROW) {
                sprintf(str, "SHIFT v ^");
            } else if (x->decr_event == SDL_EVENT_KEY_DOWN_ARROW+CTRL && x->incr_event == SDL_EVENT_KEY_UP_ARROW+CTRL) {
                sprintf(str, "CTRL v ^");
            } else if (x->decr_event == SDL_EVENT_KEY_DOWN_ARROW+ALT && x->incr_event == SDL_EVENT_KEY_UP_ARROW+ALT) {
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

            if (i < MAX_CTRL/2) {
                xpos = COL2X(20*i,FONTSZ);
                ypos = pane->h-ROW2Y(4,FONTSZ);
            } else {
                xpos = COL2X(20*(i-MAX_CTRL/2),FONTSZ);
                ypos = pane->h-ROW2Y(1,FONTSZ);
            }
            sdl_render_printf(pane, xpos, ypos, FONTSZ, SDL_WHITE, SDL_BLACK, "%s" , str);
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

void plot_clear(int idx)
{
    plots_t *p = &plots[idx];
    p->title = NULL;
}

void plot_real(int idx, 
               double *data, int n, 
               double xvmin, double xvmax, double yvmin, double yvmax, 
               double xv_blue_cursor, double xv_red_cursor, char *title, char *x_units,
               int x_pos, int y_pos, int x_width, int y_height)
{
    plots_t *p = &plots[idx];

    pthread_mutex_lock(&mutex);
    free(p->data);
    p->data = malloc(n*sizeof(double));
    for (int i = 0; i < n; i++) {
        p->data[i] = data[i];
    }
    p->n         = n;
    p->xv_min    = xvmin;
    p->xv_max    = xvmax;
    p->yv_min    = yvmin;
    p->yv_max    = yvmax;
    p->xv_blue_cursor = xv_blue_cursor;
    p->xv_red_cursor = xv_red_cursor;
    p->flags     = 0;
    p->title     = title;
    p->x_units   = x_units;
    p->x_pos     = x_pos;
    p->y_pos     = y_pos;
    p->x_width   = x_width;
    p->y_height  = y_height;
    pthread_mutex_unlock(&mutex);
}

// to auto scale use max=0
// xv_min to xv_max must be in range -sample_rate/2 to sample_rate/2
void plot_fft(int idx, 
              complex *fft, int n, double sample_rate, 
              double xv_min, double xv_max, double yv_max, double xv_blue_cursor, double xv_red_cursor, char *title,
              int x_pos, int y_pos, int x_width, int y_height)
{
    plots_t *p = &plots[idx];
    int i, nn, jj;

    if (xv_min < -sample_rate/2 || xv_max > sample_rate/2) {
        FATAL("sample_rate=%f xvmin=%f xv_max=%f\n", sample_rate, xv_min, xv_max);
    }

    nn = nearbyint(n * ((xv_max - xv_min) / sample_rate));
    if (nn <= 0 || nn > n) {
        FATAL("nn=%d sample_rate=%f xvmin=%f xv_max=%f\n", nn, sample_rate, xv_min, xv_max);
    }

    if (xv_min >= 0) {
        jj = nearbyint(xv_min * (n / sample_rate));
    } else {
        jj = nearbyint(n + xv_min * (n / sample_rate));
        if (jj == n) jj = n-1;
        if (jj < n/2) jj = n/2;
    }
    if (jj < 0 || jj >= n) {
        FATAL("jj=%d sample_rate=%f xvmin=%f xv_max=%f\n", jj, sample_rate, xv_min, xv_max);
    }

    pthread_mutex_lock(&mutex);

    free(p->data);
    p->data = malloc(nn*sizeof(double));

    for (i = 0; i < nn; i++) {
        p->data[i] = cabs(fft[jj++]);
        if (jj == n) jj = 0;
    }

    if (yv_max == 0) {
        normalize(p->data, nn, 0, 1);
        yv_max = 1;
    }

    p->n         = nn;
    p->xv_min    = xv_min;
    p->xv_max    = xv_max;
    p->yv_min    = 0;
    p->yv_max    = yv_max;
    p->xv_blue_cursor = xv_blue_cursor;
    p->xv_red_cursor = xv_red_cursor;
    p->flags     = SDL_PLOT_FLAG_BARS;
    p->title     = title;
    p->x_units   = "HZ";
    p->x_pos     = x_pos;
    p->y_pos     = y_pos;
    p->x_width   = x_width;
    p->y_height  = y_height;
    pthread_mutex_unlock(&mutex);
}

// -----------------  AUDIO OUTPUT--------------------------

#ifndef USE_PA

xxx not used

static void init_audio_out(void)
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

    // xxx make this same as the other, or delete this

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

static double ao_buff[MAX_AO_BUFF];
static unsigned long ao_head;
static unsigned long ao_tail;

static void *pa_play_thread(void*cx);
static int pa_play_cb(void *data, void *cx_arg);

static void init_audio_out(void)
{
    pthread_t tid;

    pa_init();
    pthread_create(&tid, NULL, pa_play_thread, NULL);
}

void audio_out(double yo)
{
    #define MAX_MA 10000  // xxx define needed

    static void *ma_cx;
    static int   cnt;

    // wait for room in circular audio output data buffer
    while (ao_tail - ao_head == MAX_AO_BUFF) {
        usleep(1000);
    }

    // center yo at zero
    yo -= moving_avg(yo, MAX_MA, &ma_cx);

    // print audio out value once per sec
    if (cnt++ == AUDIO_SAMPLE_RATE) {
        NOTICE("AUDIO %f\n", yo);
        cnt = 0;
    }

    // add audio out data vale to the tail of the audio out buffer    
    ao_buff[ao_tail%MAX_AO_BUFF] = yo;
    ao_tail++;
}

static void *pa_play_thread(void*cx)
{
    int ret;
    int num_chan = 1;
    int sample_rate = AUDIO_SAMPLE_RATE;

    ret = pa_play2(DEFAULT_OUTPUT_DEVICE, num_chan, sample_rate, PA_FLOAT32, pa_play_cb, NULL);
    if (ret != 0) {
        FATAL("pa_play2\n");
    }

    return NULL;
}

static int pa_play_cb(void *data, void *cx_arg)
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

#endif
