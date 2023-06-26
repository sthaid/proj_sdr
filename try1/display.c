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
#define SDL_EVENT_PLAY_TIME      (SDL_EVENT_USER_DEFINED+2)

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
            band_t *b = band[i];
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

        char play_time_str[100];
        sprintf(play_time_str, "%d", play_time);
        sdl_render_text_and_register_event(
            W - FTCW2*10 , 0, FTSZ2,
            play_time_str, SDL_LIGHT_BLUE, SDL_BLACK,
            SDL_EVENT_PLAY_TIME, SDL_EVENT_TYPE_MOUSE_WHEEL);


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
    case SDL_EVENT_PLAY_TIME: {
        int tmp = play_time + ev->mouse_wheel.delta_y;
        if (tmp < 0) tmp = 0;
        if (tmp > 10) tmp = 10;
        play_time = tmp;
        break; }
    default:
        redraw = false;
        break;
    }

    return redraw;
}

// -----------------  SUPPORT ROUTINES  ---------------------------

static void cvt_wf_to_pixels(band_t *b, int row, int width);

static void do_plot(band_t *b, rect_t *loc)  // xxx name
{
    unsigned long i;
    double max = 0, scaling;

    #define MAX_POINTS 1000000  //xxx why so many
    static point_t points[MAX_POINTS];

    #define F2X(_f) (loc->x + ((_f) - b->f_min) * loc->w / (b->f_max - b->f_min))

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
    //NOTICE("display: max = %f  scaling = %f\n", max, scaling);

    for (i = 0; i < b->max_cabs_fft; i++) {
        points[i].x = loc->x + i * loc->w / b->max_cabs_fft;
        points[i].y = (loc->y+loc->h) - b->cabs_fft[i] * scaling;
    }
    
    sdl_render_lines(points, b->max_cabs_fft, SDL_WHITE);


    // FFT LOCS

    freq_t f = b->f_min + b->fft_freq_span/2;
    for (i = 0; i < b->num_fft; i++) {
        int x, y;

        y = loc->y + loc->h;
        //x = loc->x + (double)(f - b->f_min) / (b->f_max - b->f_min) * loc->w;
        x = F2X(f);
        sdl_render_point(x, y, SDL_YELLOW, 3);

        f += b->fft_freq_span;
    }

    // WATERFALL

    int pitch, num_new_lines;

    pitch = loc->w * BYTES_PER_PIXEL;
    num_new_lines = b->wf.num - b->wf.last_displayed_num;

    if (loc->w != b->wf.last_displayed_width) {
        free(b->wf.pixels8);
        sdl_destroy_texture(b->wf.texture);

        b->wf.texture = sdl_create_texture(loc->w, MAX_WATERFALL);
        b->wf.pixels8 = calloc(MAX_WATERFALL*loc->w, BYTES_PER_PIXEL);

        //NOTICE("ALLOCING   max_cabs_fft=%d\n", b->max_cabs_fft);
        for (int row = 0; row < MAX_WATERFALL; row++) {
            cvt_wf_to_pixels(b, row, loc->w);
        }

        b->wf.last_displayed_width = loc->w;
    } else if (num_new_lines > 0) {
        //NOTICE("new lines %d\n", num_new_lines);
        memmove(b->wf.pixels8 + num_new_lines * pitch, 
                b->wf.pixels8, 
                pitch * (MAX_WATERFALL - num_new_lines));

        for (int row = 0; row < num_new_lines; row++) {
            cvt_wf_to_pixels(b, row, loc->w);
        }

        b->wf.last_displayed_num = b->wf.num; 
    } else {
        // display width has not changed AND num wf has not changed,
        // so no need to udate pixels
    }

    sdl_update_texture(b->wf.texture, b->wf.pixels8, pitch);

    sdl_render_texture(loc->x, loc->y+loc->h+5, b->wf.texture);


    // LOCATION OF FOUND STATIONS
    for (i = 0; i < b->max_scan_station; i++) {
        struct scan_station_s *ss = &b->scan_station[i];
        int x1, x2, y;

        x1 = F2X(ss->f - ss->bw/2);
        x2 = F2X(ss->f + ss->bw/2);
        y = loc->y + loc->h;

        if (x2-x1 > 2) {
            sdl_render_line(x1, y-2, x2, y-2, SDL_ORANGE);
            sdl_render_line(x1, y-1, x2, y-1, SDL_ORANGE);
            sdl_render_line(x1, y, x2, y, SDL_ORANGE);
            sdl_render_line(x1, y+1, x2, y+1, SDL_ORANGE);
            sdl_render_line(x1, y+2, x2, y+2, SDL_ORANGE);
        } else {
            sdl_render_point((x1+x2)/2, y, SDL_ORANGE, 3);
        }
    }

    // WHICH STATION IS PLAYING
    if (b->f_play != 0) {
        int x, y;

        y = loc->y + loc->h;
        //x = loc->x + (double)(b->f_play - b->f_min) / (b->f_max - b->f_min) * loc->w;
        //x = loc->x + (b->f_play - b->f_min) * loc->w / (b->f_max - b->f_min);
        x = F2X(b->f_play);
        sdl_render_point(x, y, SDL_RED, 7);
    }
}

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

        wvlen = 440 + wf_ent_max * 225 / 256;

        sdl_wavelen_to_rgb(wvlen, &red, &green, &blue);  // xxx optimize this routine with table lookup

        *pixels32++ = PIXEL(red, green, blue);
    }
}
