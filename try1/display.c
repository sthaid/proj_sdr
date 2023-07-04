#include "common.h"

//
// defines
//

#define FTSZ1  20
#define FTSZ2  30
#define FTSZ3  40
#define FTCW1 (sdl_font_char_width(FTSZ1))
#define FTCH1 (sdl_font_char_height(FTSZ1))
#define FTCW2 (sdl_font_char_width(FTSZ2))
#define FTCH2 (sdl_font_char_height(FTSZ2))
#define FTCW3 (sdl_font_char_width(FTSZ3))
#define FTCH3 (sdl_font_char_height(FTSZ3))

#define W (wi.w)
#define H (wi.h)

//
// variables
//

static win_info_t wi;

static char *debug_line;
const static bool debug_enable = false;

//
// prototypes
//

static int handle_events(void);
static void display_band(band_t *b, rect_t *loc);  // xxx name

// -----------------  DISPLAY INIT  -------------------------------

void display_init(void)
{
    sdl_init(1600, 800, 
             false,   // fullscreen
             true,    // resizeable
             false,   // swap_white_black
             &wi);
}

// -----------------  DISPLAY HANDLER  ----------------------------

void display_handler(void)
{
    char    title[100], *p;
    int     len, ret;;
    band_t *ab;

    while (true) {
        // display init
        sdl_display_init();

        // display title line
        p = title;
        p += sprintf(p, "%s", MODE_STR(mode));
        if (mode == MODE_SCAN) {
            if (scan_pause) {
                p += sprintf(p, ":PAUSED");
            } else {
                p += sprintf(p, ":INTVL=%d", scan_intvl);
            }
        }
        if (mode == MODE_PLAY || mode == MODE_SCAN) {
            if ((ab = active_band)) {
                p += sprintf(p, "  %0.6f  DEMOD:%s", (double)ab->f_play/MHZ, DEMOD_STR(demod));
            }
        }
        len = (p - title) * FTCW2;
        sdl_render_text((W-len)/2, 0, FTSZ2, title, SDL_WHITE, SDL_BLACK);

        // disaplay the bands that are selected
        int x = 50, i;
        for (i = 0; i < max_band; i++) {
            band_t *b = band[i];

            int xxx = W / 2;

            if (b->selected) {
                rect_t loc = {x, 1.5*FTCH2, xxx-100, 400};
                display_band(b, &loc);
                x += xxx;
            }
        }

        // display debug line
        if (false && (p = debug_line)) {
            sdl_render_text(0, H-FTCH1, FTSZ1, p, SDL_WHITE, SDL_BLACK);
        }

        // register events
        // - none yet

        // present the display
        sdl_display_present();

        // handle events, if ret equal -1 then program is terminating
        ret = handle_events();
        if (ret == -1) {
            break;
        }
    }
}

static int handle_events(void)
{
    bool         event_was_handled;
    int          poll_count = 0;
    sdl_event_t *ev;

    while (true) {
        ev = sdl_poll_event();

        poll_count++;
        event_was_handled = true;

        switch (ev->event_id) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_KEYMOD_CTRL + 'q':
            return -1;
        case SDL_EVENT_WINDOW: {
            win_info_t *new_wi = &ev->win_info;
            if (new_wi->minimized != wi.minimized)
                NOTICE("window minimized = %d\n", new_wi->minimized);
            if (new_wi->mouse_in_window != wi.mouse_in_window)
                NOTICE("window mouse_in_window = %d\n", new_wi->mouse_in_window);
            if (new_wi->w != wi.w || new_wi->h != wi.h)
                NOTICE("window WxH = %d x %d\n", new_wi->w, new_wi->h);
            wi = *new_wi;
            break; }
        default:
            event_was_handled = radio_event(ev);
            break;
        }

        if (event_was_handled || poll_count == 10) {
            break;
        }

        usleep(10000);
    }

    return 0;
}

// -----------------  DEBUG INTERFACE  ----------------------------

void print_debug_line(char *fmt, ...)
{
    #define MAX_LEN 100

    static char strs[2][MAX_LEN];
    static int  idx;
    char       *s;
    va_list     ap;

    if (!debug_enable) {
        return;
    }

    s = strs[idx];
    idx = (idx + 1) % 2;

    va_start(ap, fmt);
    vsnprintf(s, MAX_LEN, fmt, ap);
    va_end(ap);

    debug_line = s;
}

void clear_debug_line(void)
{
    debug_line = NULL;
}

// -----------------  DISPLAY BAND  -------------------------------

static void display_waterfall(band_t *b, rect_t *loc);
static void cvt_wf_to_pixels(band_t *b, int row, int width);

static void display_band(band_t *b, rect_t *loc)  // xxx name
{
    rect_t loc_title, loc_fft, loc_wf;
    int    i, n, x, y;
    int    points_size_needed;
    double scaling;
    bool   highlight_fft;
    char   str[100];

    static point_t *points;
    static int      len, points_size_alloced;

    #define F2X(_f) (loc_fft.x + ((_f) - b->f_min) * loc_fft.w/ (b->f_max - b->f_min))

    #define H_FFT 200
    #define H_WF  MAX_WATERFALL

    // allocate memory for points that are used to plot the fft
    points_size_needed = b->max_cabs_fft * sizeof(point_t);
    if (points_size_needed > points_size_alloced) {
        free(points);
        NOTICE("MALLOC %d\n", points_size_needed);
        points = malloc(points_size_needed);
        points_size_alloced = points_size_needed;
    }

    // determine the locations of the titls, fft, and waterfall
    loc_title = (rect_t){ loc->x, loc->y, loc->w, FTCH1 };
    loc_fft   = (rect_t){ loc->x, loc_title.y + loc_title.h, loc->w, H_FFT };
    loc_wf    = (rect_t){ loc->x, loc_fft.y + loc_fft.h + 5, loc->w, H_WF };

    // display the header
    sprintf(str, "%s", b->name);
    len = strlen(str) * FTCW1;
    sdl_render_text(loc_title.x + (loc_title.w - len) / 2, loc_title.y, 
                    FTSZ1, str, SDL_WHITE, SDL_BLACK);

    sprintf(str, "%0.3f", (double)b->f_min/MHZ);
    sdl_render_text(loc_title.x, loc_title.y, 
                    FTSZ1, str, SDL_WHITE, SDL_BLACK);

    sprintf(str, "%0.3f", (double)b->f_max/MHZ);
    len = strlen(str) * FTCW1;
    sdl_render_text(loc_title.x + loc_title.w - len, loc_title.y, 
                    FTSZ1, str, SDL_WHITE, SDL_BLACK);

    // display the fft box
    highlight_fft = (mode == MODE_FFT) ||
                    (mode == MODE_PLAY && b->active) ||
                    (mode == MODE_SCAN && b->active);
    sdl_render_rect(&loc_fft, highlight_fft ? 3 : 1, highlight_fft ? SDL_GREEN : SDL_LIGHT_GREEN);

    // display fft graph
    scaling = (double)loc_fft.h / 20000;  // xxx 20000 tbd
    for (n = 0, i = 0; i < b->max_cabs_fft; i++) {
        if (b->cabs_fft[i] == 0) continue;
        points[n].x = loc_fft.x + i * loc_fft.w / b->max_cabs_fft;
        points[n].y = (loc_fft.y+loc_fft.h-1) - b->cabs_fft[i] * scaling;
        n++;
    }
    sdl_render_lines(points, n, SDL_WHITE);

    // display waterfall
    display_waterfall(b, &loc_wf);

    // display location of found stations when in scan mode; using SDL_BLUE point
    if (mode == MODE_SCAN) {
        for (i = 0; i < b->max_scan_station; i++) {
            y = loc_fft.y + loc_fft.h - 1;
            x = F2X(b->scan_station[i].f);
            sdl_render_point(x, y, SDL_BLUE, 5);
        }
    }

    // display start and end of fft range when in play mode; using SDL_YELLOW point
    if (mode == MODE_PLAY && b->active && b->f_play_fft_min && b->f_play_fft_max) { 
        y = loc_fft.y + loc_fft.h - 1;
        x = F2X(b->f_play_fft_min);
        sdl_render_point(x, y, SDL_YELLOW, 5);
        x = F2X(b->f_play_fft_max);
        sdl_render_point(x, y, SDL_YELLOW, 5);
    }

    // display freq when in play or scan mode; using SDL_RED point
    if ((mode == MODE_PLAY || mode == MODE_SCAN) && b->active && b->f_play != 0) {
        y = loc_fft.y + loc_fft.h - 1;
        x = F2X(b->f_play);
        sdl_render_point(x, y, SDL_RED, 5);
    }
}

static void display_waterfall(band_t *b, rect_t *loc)
{
    int pitch, num_new_rows;

    pitch = loc->w * BYTES_PER_PIXEL;
    num_new_rows = b->wf.num - b->wf.last_displayed_num;

    if (loc->w != b->wf.last_displayed_width || b->wf.pixels8 == NULL) {
        // waterfall display width has changed ...
        NOTICE("WDITH CHANGED\n");

        // free and reallocate pixels and texture
        free(b->wf.pixels8);
        sdl_destroy_texture(b->wf.texture);
        b->wf.texture = sdl_create_texture(loc->w, MAX_WATERFALL);
        b->wf.pixels8 = calloc(MAX_WATERFALL*loc->w, BYTES_PER_PIXEL);

        // convert all waterfall rows to pixels
        for (int row = 0; row < MAX_WATERFALL; row++) {
            cvt_wf_to_pixels(b, row, loc->w);
        }

        // remember the last_displayed_width
        b->wf.last_displayed_width = loc->w;

        // copy the pixels in to the texture
        sdl_update_texture(b->wf.texture, b->wf.pixels8, pitch);
    } else if (num_new_rows > 0) {
        // one or more new waterfall rows have been added

        // move the pixels to make room for the new row(s)
        memmove(b->wf.pixels8 + num_new_rows * pitch, 
                b->wf.pixels8, 
                pitch * (MAX_WATERFALL - num_new_rows));

        // convert the new waterfall row(s) to pixels
        for (int row = 0; row < num_new_rows; row++) {
            cvt_wf_to_pixels(b, row, loc->w);
        }

        // remember the number of waterfall rows
        b->wf.last_displayed_num = b->wf.num; 

        // copy the pixels in to the texture
        sdl_update_texture(b->wf.texture, b->wf.pixels8, pitch);
    } else {
        // display width has not changed AND num wf has not changed,
        // so no need to udate pixels
    }

    // render the texture
    sdl_render_texture(loc->x, loc->y, b->wf.texture);
}

// xxx review this
static void cvt_wf_to_pixels(band_t *b, int row, int width)
{
    unsigned char *wf;
    unsigned int  *pixels32;
    int            pitch, i, j1, j2, k, n, wf_ent_max, wvlen;
    unsigned char red, green, blue;

    pitch = width * BYTES_PER_PIXEL;

    wf = get_waterfall(b, row);
    pixels32 = (unsigned int *)(b->wf.pixels8 + row * pitch);

    if (wf == NULL) {
        memset(pixels32, 0, pitch);
        return;
    }

    for (i = 0; i < width; i++) {
        j1 = (long)i * b->max_cabs_fft / width;
        j2 = (long)(i+1) * b->max_cabs_fft / width;
        n = j2 - j1;
        if (n == 0) n = 1;

        wf_ent_max = 0;
        for (k = 0; k < n; k++) {
            if (wf[j1+k] > wf_ent_max) {
                wf_ent_max = wf[j1+k];
            }
        }

        if (wf_ent_max > 0) {
            wvlen = 440 + wf_ent_max * 225 / 256;
            sdl_wavelen_to_rgb(wvlen, &red, &green, &blue);  // xxx optimize this routine with table lookup
            *pixels32++ = PIXEL(red, green, blue);
        } else {
            *pixels32++ = PIXEL(0, 0, 0);
        }
    }
}



#if 0
        sdl_render_text_and_register_event(  // xxx use a key?  OR the window ctrl
            W/2-3.5*FTCW2, 0, FTSZ2,
            "FULLSCR", SDL_LIGHT_BLUE, SDL_BLACK,
            SDL_EVENT_FULLSCR, SDL_EVENT_TYPE_MOUSE_LEFT_CLICK);

        char play_time_str[100];
        sprintf(play_time_str, "%d", play_time);
        sdl_render_text_and_register_event(
            W - FTCW2*10 , 0, FTSZ2,
            play_time_str, SDL_LIGHT_BLUE, SDL_BLACK,
            SDL_EVENT_PLAY_TIME, SDL_EVENT_TYPE_MOUSE_WHEEL);
#endif
#if 0
        case SDL_EVENT_PLAY_TIME: {
            // xxx redo this
            int tmp = play_time + ev->mouse_wheel.delta_y;
            if (tmp < 0) tmp = 0;
            if (tmp > 10) tmp = 10;
            play_time = tmp;
            break; }
#endif
