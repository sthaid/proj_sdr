#include "common.h"

//
// defines
//

#define FTSZ1  20
#define FTSZ2  40
#define FTSZ3  60
#define FTCW1 (sdl_font_char_width(FTSZ1))
#define FTCH1 (sdl_font_char_height(FTSZ1))
#define FTCW2 (sdl_font_char_width(FTSZ2))
#define FTCH2 (sdl_font_char_height(FTSZ2))
#define FTCW3 (sdl_font_char_width(FTSZ3))
#define FTCH3 (sdl_font_char_height(FTSZ3))

#define SDL_EVENT_END_PROGRAM    (SDL_EVENT_USER_DEFINED+0)
#define SDL_EVENT_FULLSCR        (SDL_EVENT_USER_DEFINED+1)

#define W (wi.w)
#define H (wi.h)

//
// variables
//

static win_info_t wi;
static bool       fullscr;

//
// prototypes
//

static bool handle_event(sdl_event_t *ev);
static void do_plot(band_t *b, rect_t *loc);  // xxx name

// -----------------  DISPLAY INIT  -------------------------------

void display_init(void)
{
    sdl_init(1600, 800, false, true, false, &wi);
}

// -----------------  DISPLAY HANDLER  ----------------------------

void display_handler(void)
{
    while (true) {
        // display init
        sdl_display_init();

        // Hello
        sdl_render_text(0, 0, FTSZ1, "Hello!", SDL_WHITE, SDL_BLACK);

        // xxx loop
        int x = 0, i;
        for (i = 0; i < max_band; i++) {
            band_t *b = &band[i];
            if (b->cabs_fft == NULL || b->max_cabs_fft == 0) {
                continue;
            }

            rect_t loc = {x, 50, 700, 400};
            do_plot(b, &loc);

            x += 800;
        }

        // register events
        sdl_render_text_and_register_event(
            0, H - FTCH2, FTSZ2,
            "QUIT", SDL_LIGHT_BLUE, SDL_BLACK,
            SDL_EVENT_END_PROGRAM, SDL_EVENT_TYPE_MOUSE_LEFT_CLICK);

        sdl_render_text_and_register_event(
            W/2-3.5*FTCW2, 0, FTSZ2,
            "FULLSCR", SDL_LIGHT_BLUE, SDL_BLACK,
            SDL_EVENT_FULLSCR, SDL_EVENT_TYPE_MOUSE_LEFT_CLICK);

        // present the display
        sdl_display_present();

        // handle events
        sdl_event_t *ev;
        int poll_count = 0;
        bool redraw;

        while (true) {
            ev = sdl_poll_event();
            poll_count++;

            redraw = handle_event(ev);

            if (redraw || poll_count == 10) {
                break;
            }

            usleep(10000);
        }

        // if program is terminating then return
        if (program_terminating) {
            break;
        }
    }
}

static bool handle_event(sdl_event_t *ev)
{
    bool redraw = true;

    switch (ev->event_id) {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_END_PROGRAM:
    case SDL_EVENT_KEYMOD_CTRL + 'q':
        program_terminating = true;
        break;
    case SDL_EVENT_FULLSCR:
        fullscr = !fullscr;
        sdl_full_screen(fullscr);
        break;
    default:
        redraw = false;
        break;
    }

    return redraw;
}

// -----------------  SUPPORT ROUTINES  ---------------------------

static void do_plot(band_t *b, rect_t *loc)  // xxx name
{
    unsigned long i;
    double max = 0, scaling;

    #define MAX_POINTS 1000000
    static point_t points[MAX_POINTS];

    if (b->max_cabs_fft > MAX_POINTS) {
        FATAL("max_cabs_fft %d, too large\n", b->max_cabs_fft);
    }

    //NOTICE("--------- PLOT --------\n");
    sdl_render_rect(loc, 2, SDL_GREEN);

    sdl_render_printf(loc->x, loc->y, FTSZ2, SDL_WHITE, SDL_BLACK, "%s", b->name);

    // xxx make this more efficient ...

    for (i = 0; i < b->max_cabs_fft; i++) {
        if (b->cabs_fft[i] > max) max = b->cabs_fft[i];
    }
    scaling = loc->h / max;
    DEBUG("display: max = %f  scaling = %f\n", max, scaling);

    for (i = 0; i < b->max_cabs_fft; i++) {
        points[i].x = loc->x + i * loc->w / b->max_cabs_fft;
        points[i].y = (loc->y+loc->h) - b->cabs_fft[i] * scaling;
    }
    
    sdl_render_lines(points, b->max_cabs_fft, SDL_WHITE);

    freq_t f = b->f_min + b->fft_freq_span/2;
    for (i = 0; i < b->num_fft; i++) {
        int x, y;

        y = loc->y + loc->h;
        x = loc->x + (double)(f - b->f_min) / (b->f_max - b->f_min) * loc->w;
        sdl_render_point(x, y, SDL_RED, 5);

        f += b->fft_freq_span;
    }
}
