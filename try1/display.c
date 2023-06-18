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

static win_info_t    wi;
static bool          fullscr;
static bool          end_program;

//
// prototypes
//

static bool handle_event(sdl_event_t *ev);

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

        // register events
        sdl_render_text_and_register_event(
            0, H - FTCH2, FTSZ2,
            "QUIT", SDL_LIGHT_BLUE, SDL_BLACK,
            SDL_EVENT_END_PROGRAM, SDL_EVENT_TYPE_MOUSE_LEFT_CLICK);

        sdl_render_text_and_register_event(
            W/2-3*FTCW1, 0, FTSZ2,
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

        // if end_program requested then return
        if (end_program) {
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
        end_program = true;
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

