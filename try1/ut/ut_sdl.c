#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <misc.h>
#include <sdl.h>

#define FTPSZ    20
#define FTPSZ_LG 40

#define SDL_EVENT_END_PROGRAM    (SDL_EVENT_USER_DEFINED+0)
#define SDL_EVENT_END_PROGRAM2   (SDL_EVENT_USER_DEFINED+1)

int main(int argc, char **argv)
{
    char *s;
    int   max_texture_dim;
    int   i, x, y, w, h;
    rect_t loc;
    texture_t circle = NULL;

    NOTICE("program starting\n");

    sdl_init(1600, 800, false, true, false); // resizeable=true

    sdl_get_max_texture_dim(&max_texture_dim);
    NOTICE("max_texture_dim = %d\n", max_texture_dim);

    while (true) {
        // display init
        NOTICE("display_init\n");
        sdl_display_init();

        // get window size
        sdl_get_window_size(&w, &h);

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
            10 * sdl_font_char_width(FTPSZ_LG), h - sdl_font_char_height(FTPSZ_LG), FTPSZ_LG, 
            "QUIT2", SDL_LIGHT_BLUE, SDL_BLACK, 
            SDL_EVENT_END_PROGRAM2, SDL_EVENT_TYPE_MOUSE_CLICK);

        // present the display
        sdl_display_present();

        // handle events
        sdl_event_t *ev;
        bool event_handled;

        NOTICE("processing events\n");
        do {
            event_handled = true;
            ev = sdl_poll_event();
            switch (ev->event_id) {
            case SDL_EVENT_QUIT: 
                NOTICE("got event SDL_EVENT_QUIT\n");
                goto end_program;
                break;
            case SDL_EVENT_END_PROGRAM:
                NOTICE("got event SDL_EVENT_END_PROGRAM\n");
                goto end_program;
                break;
            case SDL_EVENT_KEY_ALT + 'q':
                NOTICE("got event ALT-q\n");
                goto end_program;
                break;
            case SDL_EVENT_END_PROGRAM2: {
                static sdl_event_t evq = {SDL_EVENT_QUIT};
                NOTICE("got event SDL_EVENT_END_PROGRAM2\n");
                sdl_push_event(&evq);
                break; }
            default:
                event_handled = false;
                usleep(10000);
                break;
            }
        } while (event_handled == false);
    }

end_program:
    return 0;
}

