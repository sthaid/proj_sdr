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

//
// test table
//

static struct test_s {
    char *name;
    void *(*proc)(void *cx);
} tests[] = {
        { "plot", plot_test },
                };

// -----------------  MAIN  --------------------------------

int main(int argc, char **argv)
{
    int i;
    char *name;
    struct test_s *t;
    pthread_t tid;

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
#if 0
        sdl_render_printf(pane,
                        100, 100,
                        FONTSZ, SDL_GREEN, SDL_BLACK, "%s", "Hello");
#endif
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

// -----------------  PLOT TEST  ---------------------------

void init_sine_wave(int sample_rate, int f, int secs, double **data_arg, int *n_arg);

void *plot_test(void *cx)
{
    int n;
    double *data;
    plots_t *p;

    init_sine_wave(10000, 10, 1, &data, &n);

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

void init_sine_wave(int sample_rate, int f, int secs, double **data_arg, int *n_arg)
{
    double  w = TWO_PI * f;
    double  t = 0;
    double  dt = (1. / sample_rate);
    double *data;
    int     i, n;

    n = sample_rate * secs;
    data = (double*)calloc(n, sizeof(double));

    for (i = 0; i < n; i++) {
        data[i] = sin(w * t);
        t += dt;
    }

    *data_arg = data;
    *n_arg = n;
}
