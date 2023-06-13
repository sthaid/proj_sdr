#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <misc.h>
#include <sdl.h>

#define FTPSZ    20
#define FTPSZ_LG 40

#define SDL_EVENT_END_PROGRAM    (SDL_EVENT_USER_DEFINED+0)
#define SDL_EVENT_FULLSCR        (SDL_EVENT_USER_DEFINED+2)

int main(int argc, char **argv)
{
    char *s;
    int   max_texture_dim;
    int   i, x, y, w, h, w_last=0, h_last=0;
    rect_t loc;
    texture_t circle = NULL;

    NOTICE("program starting\n");

    sdl_init(1600, 800, false, true, false); // resizeable=true

    sdl_get_max_texture_dim(&max_texture_dim);
    NOTICE("max_texture_dim = %d\n", max_texture_dim);

    while (true) {
        // get window size
        sdl_get_window_size(&w, &h);
        if (w != w_last || h != h_last) {
            NOTICE("%d %d\n", w, h);
            w_last = w;
            h_last = h;
        }

        // display init
        sdl_display_init();

        // render text
        s = "Top Left, White";
        sdl_render_text(0, 0, FTPSZ, s, SDL_WHITE, SDL_BLACK);

        s = "Bottom Right, Orange";
        x = w - strlen(s) * sdl_font_char_width(FTPSZ);
        y = h - sdl_font_char_height(FTPSZ);
        sdl_render_text(x, y, FTPSZ, s, SDL_ORANGE, SDL_BLACK);

        // rectangles
        loc = (rect_t){-20, -20, 200, 200};
        sdl_render_rect(&loc, 1, SDL_PURPLE);

        loc = (rect_t){w-180, h-180, 200, 200};
        sdl_render_rect(&loc, 10, SDL_PURPLE);

        loc = (rect_t){10, h/2, 200, 200};
        sdl_render_fill_rect(&loc, SDL_PURPLE);

        // lines
        point_t p[4];
        p[0] = (point_t){w/2-100, 100};
        p[1] = (point_t){w/2, -100};
        p[2] = (point_t){w/2+100, 100};
        p[3] = (point_t){w/2-100, 100};
        sdl_render_lines(p, 4, SDL_YELLOW);

        // circle
        sdl_render_circle(w*3/4, h/2, 200, 5, SDL_PINK);

        // points
        for (i = 0; i <= 9; i++) {
            sdl_render_point(10+i*30, h/3, SDL_GREEN, i);
        }

        // textures
        if (circle == NULL) {
            int wt, ht;
            circle = sdl_create_filled_circle_texture(100, SDL_BLUE);
            sdl_query_texture(circle, &wt, &ht);
            NOTICE("circle texture w,h = %d %d\n", wt, ht);
        }
        sdl_render_texture(w/2-100, h*4/5, circle);

        loc = (rect_t){w/2-100, h/2, 200, 100};
        sdl_render_scaled_texture(&loc, circle);

        // register events
        sdl_render_text_and_register_event(
            0, h - sdl_font_char_height(FTPSZ_LG), FTPSZ_LG, 
            "QUIT", SDL_LIGHT_BLUE, SDL_BLACK, 
            SDL_EVENT_END_PROGRAM, SDL_EVENT_TYPE_MOUSE_CLICK);
        sdl_render_text_and_register_event(
            w/2-3*sdl_font_char_width(FTPSZ_LG), 0, FTPSZ_LG, 
            "FULLSCR", SDL_LIGHT_BLUE, SDL_BLACK, 
            SDL_EVENT_FULLSCR, SDL_EVENT_TYPE_MOUSE_CLICK);

        // present the display
        sdl_display_present();

        // handle events
        sdl_event_t *ev;
        int poll_count = 0;
        bool redraw_now;

        while (true) {
            ev = sdl_poll_event();
            poll_count++;
            redraw_now = true;

            switch (ev->event_id) {
            case SDL_EVENT_QUIT: 
            case SDL_EVENT_END_PROGRAM:
            case SDL_EVENT_KEY_ALT + 'q':
                goto end_program;
                break;
            case SDL_EVENT_FULLSCR: {
                static bool fullscr;
                fullscr = !fullscr;
                sdl_full_screen(fullscr);
                break; }
            case SDL_EVENT_WIN_SIZE_CHANGE:
            case SDL_EVENT_WIN_MINIMIZED:
            case SDL_EVENT_WIN_RESTORED:
                break;
            case SDL_EVENT_KEY_ALT + 'p':
                sdl_print_screen(true, NULL);
                break;
            default:
                redraw_now = false;
                break;
            }

            if (redraw_now || poll_count == 10) {
                break;
            }
        }

        // xxx print display update rate
    }

end_program:
    return 0;
}

