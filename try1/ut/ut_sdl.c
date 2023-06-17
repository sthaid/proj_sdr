#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <locale.h>
#include <wchar.h>

#include <misc.h>
#include <sdl.h>

// unicode chars, in UTF8 encoding
#define UNICODE_LEFT_ARROW  "\u2190"
#define UNICODE_RIGHT_ARROW "\u2192"
#define UNICODE_UP_ARROW    "\u2191"
#define UNICODE_DOWN_ARROW  "\u2193"
#define UNICODE_PI          "\u03c0"

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
#define SDL_EVENT_MOUSE_DRAG     (SDL_EVENT_USER_DEFINED+2)
#define SDL_EVENT_MOUSE_POSITION (SDL_EVENT_USER_DEFINED+3)
#define SDL_EVENT_MOUSE_WHEEL    (SDL_EVENT_USER_DEFINED+4)

#define W (wi.w)
#define H (wi.h)

win_info_t    wi;
texture_t     circle, points;
int           dragx=800, dragy=400;
int           mousex, mousey;
int           wheel_count;
bool          fullscr;
char          evid_key_str[20];
unsigned long evid_key_time;

size_t mb_strlen(char *s);

// -----------------  MAIN  ------------------------------------------

int main(int argc, char **argv)
{
    char  *s;
    int    i, x, y;
    rect_t loc;

    NOTICE("program starting\n");

    if (setlocale(LC_ALL, "") == NULL) {
        printf("ERROR: setlocale failed.\n");
        return 1;
    }
    printf("MB_CUR_MAX = %ld\n", MB_CUR_MAX);

    sdl_init(1600, 800, false, true, false, &wi);

    int max_texture_dim;
    sdl_get_max_texture_dim(&max_texture_dim);
    NOTICE("max_texture_dim = %d\n", max_texture_dim);

    while (true) {
        // display init
        sdl_display_init();

        // render text
        s = "Top Left, White";
        sdl_render_text(0, 0, FTSZ1, s, SDL_WHITE, SDL_BLACK);

        s = "Bottom Right, Orange";
        x = W - strlen(s) * FTCW1;
        y = H - FTCH1;
        sdl_render_text(x, y, FTSZ1, s, SDL_ORANGE, SDL_BLACK);

        char *s = UNICODE_PI " = 3.14";
        x = W/4;
        y = FTCH2;
        sdl_render_text(x, y, FTSZ2, s, SDL_WHITE, SDL_BLACK);

        // rectangles
        loc = (rect_t){-20, -20, 200, 200};
        sdl_render_rect(&loc, 1, SDL_PURPLE);

        loc = (rect_t){W-180, H-180, 200, 200};
        sdl_render_rect(&loc, 10, SDL_PURPLE);

        loc = (rect_t){10, H/2, 200, 200};
        sdl_render_fill_rect(&loc, SDL_PURPLE);

        // lines
        point_t p[4];
        p[0] = (point_t){W/2-100, 100};
        p[1] = (point_t){W/2, -100};
        p[2] = (point_t){W/2+100, 100};
        p[3] = (point_t){W/2-100, 100};
        sdl_render_lines(p, 4, SDL_YELLOW);

        // circle
        sdl_render_circle(W*3/4, H/2, 200, 5, SDL_PINK);

        // if an ascii char evid occurred then display the char
        // in the center of the circle
        if (evid_key_time) {
            int len = mb_strlen(evid_key_str);
            sdl_render_printf(W*3/4-(len*FTCW3)/2, H/2-FTCH3/2, FTSZ3, SDL_WHITE, SDL_BLACK, "%s", evid_key_str);
            if (microsec_timer() > evid_key_time + 5000000) {
                evid_key_time = 0;
            }
        }

        // points
        for (i = 0; i <= 9; i++) {
            sdl_render_point(10+i*30, H/3, SDL_GREEN, i);
        }

        // textures
        if (circle == NULL) {
            int wt, ht;
            circle = sdl_create_filled_circle_texture(100, SDL_BLUE);
            sdl_query_texture(circle, &wt, &ht);
            NOTICE("circle texture w,h = %d %d\n", wt, ht);
        }
        sdl_render_texture(W/2-100, H*4/5, circle);

        // scaled texture
        loc = (rect_t){W/2-100, H/2, 200, 100};
        sdl_render_scaled_texture(&loc, circle);

        // create texture from window pixels;
        // make copy of points that were rendered in above code, and render 
        // just a bit below
        loc.x = 0;
        loc.y = H/3-10;
        loc.w = 300;
        loc.h = 20;
        points = sdl_create_texture_from_win_pixels(&loc);
        sdl_render_texture(0, H/3+50, points);

        // register events
        sdl_render_text_and_register_event(
            0, H - FTCH2, FTSZ2, 
            "QUIT", SDL_LIGHT_BLUE, SDL_BLACK, 
            SDL_EVENT_END_PROGRAM, SDL_EVENT_TYPE_MOUSE_LEFT_CLICK);

        sdl_render_text_and_register_event(
            W/2-3*FTCW1, 0, FTSZ2, 
            "FULLSCR", SDL_LIGHT_BLUE, SDL_BLACK, 
            SDL_EVENT_FULLSCR, SDL_EVENT_TYPE_MOUSE_LEFT_CLICK);

        sdl_render_text_and_register_event(
            dragx, dragy, FTSZ2,
            "DRAG", SDL_LIGHT_BLUE, SDL_BLACK, 
            SDL_EVENT_MOUSE_DRAG, SDL_EVENT_TYPE_MOUSE_DRAG);

        char wheel_count_str[100];
        sprintf(wheel_count_str, "%d", wheel_count);
        sdl_render_text_and_register_event(
            20, 100, FTSZ2,
            wheel_count_str, SDL_ORANGE, SDL_BLACK, 
            SDL_EVENT_MOUSE_WHEEL, SDL_EVENT_TYPE_MOUSE_WHEEL);

        sdl_register_event(&(rect_t){0,0,W,H},
            SDL_EVENT_MOUSE_POSITION, SDL_EVENT_TYPE_MOUSE_POSITION);

        // draw point at the mouse position
        if (wi.mouse_in_window) {
            sdl_render_point(mousex, mousey, SDL_RED, 3);
        }

        // present the display
        sdl_display_present();

        // handle events
        sdl_event_t *ev;
        int evid, poll_count = 0;
        bool redraw_now;

        while (true) {
            ev = sdl_poll_event();
            poll_count++;
            redraw_now = true;

            switch (ev->event_id) {
            case SDL_EVENT_QUIT: 
            case SDL_EVENT_END_PROGRAM:
                goto end_program;
                break;
            case SDL_EVENT_FULLSCR: {
                fullscr = !fullscr;
                sdl_full_screen(fullscr);
                break; }
            case SDL_EVENT_MOUSE_DRAG:
                dragx += ev->mouse_drag.delta_x;
                dragy += ev->mouse_drag.delta_y;
                break;
            case SDL_EVENT_MOUSE_POSITION:
                mousex = ev->mouse_position.x;
                mousey = ev->mouse_position.y;
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                wheel_count += ev->mouse_wheel.delta_y;
                break;
            case SDL_EVENT_WINDOW:
                wi = ev->win_info;
                NOTICE("win_info: w=%d h=%d minimized=%d mouse_in_window=%d\n",
                       wi.w, wi.h, wi.minimized, wi.mouse_in_window);
                break;
            case SDL_EVENT_KEY_FIRST ... SDL_EVENT_KEY_LAST:
                evid = ev->event_id;
                if (evid == SDL_EVENT_KEYMOD_ALT + 'q') {
                    NOTICE("got alt-q\n");
                    goto end_program;
                } else if (evid == SDL_EVENT_KEYMOD_CTRL + SDL_EVENT_KEY_PRINTSCREEN) {
                    NOTICE("got ctrl-PrtScn\n");
                    sdl_print_screen(true, NULL);
                } else if (IS_EVENT_ID_TEXT(evid)) {
                    NOTICE("got ASCII char '%c'\n", evid);
                    evid_key_str[0] = evid;
                    evid_key_str[1] = 0;
                    evid_key_time = microsec_timer();
                } else {
                    char tmp[10] = {0};
                    NOTICE("got special key %04x\n", evid);
                    switch (evid & ~SDL_EVENT_KEYMOD_MASK) {
                    case SDL_EVENT_KEY_LEFT_ARROW: strcat(tmp, UNICODE_LEFT_ARROW); break;
                    case SDL_EVENT_KEY_RIGHT_ARROW: strcat(tmp, UNICODE_RIGHT_ARROW); break;
                    case SDL_EVENT_KEY_UP_ARROW: strcat(tmp, UNICODE_UP_ARROW); break;
                    case SDL_EVENT_KEY_DOWN_ARROW: strcat(tmp, UNICODE_DOWN_ARROW); break;
                    case 'a' ... 'z': tmp[0] = evid; tmp[1] = 0; break;
                    }
                    if (tmp[0]) {
                        evid_key_str[0] = 0;
                        if (evid & SDL_EVENT_KEYMOD_SHIFT) strcat(evid_key_str, "S");
                        if (evid & SDL_EVENT_KEYMOD_CTRL) strcat(evid_key_str, "C");
                        if (evid & SDL_EVENT_KEYMOD_ALT) strcat(evid_key_str, "A");
                        if (evid_key_str[0]) strcat(evid_key_str , ":");
                        strcat(evid_key_str, tmp);
                        evid_key_time = microsec_timer();
                    }
                }
                break;
            default:
                redraw_now = false;
                break;
            }

            if (redraw_now || poll_count == 10) {
                break;
            }

            usleep(10000);
        }

        // xxx print display update rate
    }

end_program:
    return 0;
}

size_t mb_strlen(char *s)
{
    size_t cnt=0, charlen;
    mbstate_t mbs;

    memset(&mbs, 0, sizeof(mbs));
    while (1) {
        charlen = mbrlen(s, MB_CUR_MAX, &mbs);
        if (charlen < 0) {
            printf("ERROR invlid multibyte string\n");
        }
        if (charlen <= 0) {
            break;
        }
        cnt++;
        s += charlen;
    }

    return cnt;
}

