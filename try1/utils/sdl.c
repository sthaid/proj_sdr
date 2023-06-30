#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <unistd.h>
#include <limits.h>
#include <time.h>
#include <assert.h>
#include <sys/stat.h>
#include <SDL.h>
#include <SDL_ttf.h>

#include <sdl.h>
#include <misc.h>
#include <png_rw.h>

#define UNUSED __attribute__ ((unused))

// SDL2 Doc
// - https://wiki.libsdl.org/SDL2/FrontPage
//     https://wiki.libsdl.org/SDL2/APIByCategory
//     https://wiki.libsdl.org/SDL2/CategoryAPI

// SDL2 TTF Doc
// - https://wiki.libsdl.org/SDL2_ttf/FrontPage
//     https://wiki.libsdl.org/SDL2_ttf/CategoryAPI
// - rendering functions
//     solid:   just fg color, quick but dirty rendering
//     blended: just fg color, good quality
//     shaded:  fg and bg colors, good quality

// UTF8
//  https://en.wikipedia.org/wiki/UTF-8

// UNICODE
//  https://en.wikipedia.org/wiki/Unicode_block
//  https://en.wikipedia.org/wiki/Block_Elements
//  https://en.wikipedia.org/wiki/Arrows_(Unicode_block)
//
//  https://www.compart.com/en/unicode/
//  https://www.compart.com/en/unicode/block          list of blocks
//  https://www.compart.com/en/unicode/block/U+0370   Greek and Coptic
//  https://www.compart.com/en/unicode/U+03C0         pi

// Inserting unicode (this inserts UTF8 encoded unicode char)
//   C language string example: "Hello \u03c0 = 3.14"
//     note: gcc will not accept "\u0041" 
//   vim, insert π:  ^vu03c0

// xxx 
// - add comments for each routine

//
// defines
//

#define MAX_FONT_PTSIZE       200
#define MAX_EVENT_REG_TBL     1000
#define MAX_SDL_COLOR_TO_RGBA 1000

#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

//
// typedefs
//

typedef struct {
    TTF_Font * font;
    int32_t    char_width;
    int32_t    char_height;
} sdl_font_t;

typedef struct {
    int32_t event_id;
    int32_t event_type;
    rect_t disp_loc;
} sdl_event_reg_t;

//
// variables
//

static SDL_Window     * sdl_window;
static SDL_Renderer   * sdl_renderer;
static SDL_RendererInfo sdl_renderer_info;
static win_info_t       sdl_win_info;

static sdl_font_t       sdl_font[MAX_FONT_PTSIZE];
static char           * sdl_font_path;

static sdl_event_reg_t  sdl_event_reg_tbl[MAX_EVENT_REG_TBL];
static int32_t          sdl_event_max;
static sdl_event_t      sdl_push_ev;

static uint32_t         sdl_color_to_rgba[MAX_SDL_COLOR_TO_RGBA] = {
                            PIXEL_PURPLE, 
                            PIXEL_BLUE, 
                            PIXEL_LIGHT_BLUE, 
                            PIXEL_GREEN, 
                            PIXEL_YELLOW, 
                            PIXEL_ORANGE, 
                            PIXEL_PINK, 
                            PIXEL_RED, 
                            PIXEL_GRAY, 
                            PIXEL_WHITE, 
                            PIXEL_BLACK, 
                                        };

//
// prototypes
//

static void exit_handler(void);
static void font_init(int32_t ptsize);
static void set_color(int32_t color); 

// -----------------  SDL INIT & MISC ROUTINES  ------------------------- 

int32_t sdl_init(int32_t w, int32_t h, bool fullscreen, bool resizeable, bool swap_white_black, win_info_t *wi)
{
    #define MAX_FONT_SEARCH_PATH 2

    static char font_search_path[MAX_FONT_SEARCH_PATH][PATH_MAX];

    // display available and current video drivers
    int num, i;
    num = SDL_GetNumVideoDrivers();
    DEBUG("Available Video Drivers: ");
    for (i = 0; i < num; i++) {
        DEBUG("   %s\n",  SDL_GetVideoDriver(i));
    }

    // initialize Simple DirectMedia Layer  (SDL)
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO) < 0) {
        ERROR("SDL_Init failed\n");
        return -1;
    }

    // create SDL Window and Renderer
    #define SDL_FLAGS ((resizeable ? SDL_WINDOW_RESIZABLE : 0) | \
                       (fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0))
    if (SDL_CreateWindowAndRenderer(w, h, SDL_FLAGS, &sdl_window, &sdl_renderer) != 0) {
        ERROR("SDL_CreateWindowAndRenderer failed\n");
        return -1;
    }
    sdl_poll_event();

    // init win_info
    SDL_GetWindowSize(sdl_window, &sdl_win_info.w, &sdl_win_info.h);
    sdl_win_info.minimized = false;
    sdl_win_info.mouse_in_window = true;
    NOTICE("win_width=%d win_height=%d\n", sdl_win_info.w, sdl_win_info.h);

    // initialize True Type Font
    if (TTF_Init() < 0) {
        ERROR("TTF_Init failed\n");
        return -1;
    }

    // determine sdl_font_path by searching for FreeMonoBold.ttf font file in possible locations
    // note - fonts can be installed using:
    //   sudo yum install gnu-free-mono-fonts       # rhel,centos,fedora
    //   sudo apt-get install fonts-freefont-ttf    # raspberrypi, ubuntu
    sprintf(font_search_path[0], "%s", "/usr/share/fonts/gnu-free/FreeMonoBold.ttf");
    sprintf(font_search_path[1], "%s", "/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf");
    for (i = 0; i < MAX_FONT_SEARCH_PATH; i++) {
        struct stat buf;
        sdl_font_path = font_search_path[i];
        if (stat(sdl_font_path, &buf) == 0) {
            break;
        }
    }
    if (i == MAX_FONT_SEARCH_PATH) {
        ERROR("failed to locate font file\n");
        return -1;
    }
    DEBUG("using font %s\n", sdl_font_path);

    // if caller requests swap_white_black then swap the white and black
    // entries of the sdl_color_to_rgba table
    if (swap_white_black) {
        uint32_t tmp = sdl_color_to_rgba[SDL_WHITE];
        sdl_color_to_rgba[SDL_WHITE] = sdl_color_to_rgba[SDL_BLACK];
        sdl_color_to_rgba[SDL_BLACK] = tmp;
    }

    // register exit handler
    atexit(exit_handler);

    // return sdl_win_info to caller
    *wi = sdl_win_info;

    // return success
    DEBUG("success\n");
    return 0;
}

static void font_init(int32_t font_ptsize)
{
    // if this font has already been initialized then return
    if (sdl_font[font_ptsize].font != NULL) {
        return;
    }

    // open font for this font_ptsize,
    assert(sdl_font_path);
    sdl_font[font_ptsize].font = TTF_OpenFont(sdl_font_path, font_ptsize);
    if (sdl_font[font_ptsize].font == NULL) {
        FATAL("failed TTF_OpenFont(%s,%d)\n", sdl_font_path, font_ptsize);
    }

    // and init the char_width / char_height
    TTF_SizeText(sdl_font[font_ptsize].font, "X", &sdl_font[font_ptsize].char_width, &sdl_font[font_ptsize].char_height);
    DEBUG("font_ptsize=%d width=%d height=%d\n",
         font_ptsize, sdl_font[font_ptsize].char_width, sdl_font[font_ptsize].char_height);
}

void sdl_get_win_info(win_info_t *wi)
{
    *wi = sdl_win_info;
}

void sdl_full_screen(bool enable)
{
    SDL_SetWindowFullscreen(sdl_window, enable ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

void sdl_get_max_texture_dim(int32_t * max_texture_dim)
{
    // return the max texture dimension to caller;
    // - SDL provides us with max_texture_width and max_texture_height, usually the same
    // - the min of max_texture_width/height is returned to caller
    // - this returned max_texture_dim is to be used by the caller to limit the maximum
    //   width and height args passed to sdl_create_texture()
    if (SDL_GetRendererInfo(sdl_renderer, &sdl_renderer_info) != 0) {
        ERROR("SDL_SDL_GetRendererInfo failed\n");
        *max_texture_dim = 0;
    }
    NOTICE("max_texture_width=%d  max_texture_height=%d\n",
         sdl_renderer_info.max_texture_width, sdl_renderer_info.max_texture_height);
    *max_texture_dim = min(sdl_renderer_info.max_texture_width, 
                           sdl_renderer_info.max_texture_height);
}

static void exit_handler(void)
{
    int32_t i;
    
    for (i = 0; i < MAX_FONT_PTSIZE; i++) {
        if (sdl_font[i].font != NULL) {
            TTF_CloseFont(sdl_font[i].font);
        }
    }
    TTF_Quit();

    SDL_DestroyRenderer(sdl_renderer);
    SDL_DestroyWindow(sdl_window);
    SDL_Quit();
}

// -----------------  PRINT SCREEN -------------------------------------- 

void sdl_print_screen(bool flash_display, rect_t * rect_arg) 
{
    uint8_t * pixels = NULL;
    SDL_Rect  rect;
    int32_t   ret, len;
    struct tm tm;
    time_t    t;
    char      file_name[PATH_MAX];

    // create file_name
    t = time(NULL);
    localtime_r(&t, &tm);
    sprintf(file_name, "screenshot_%2.2d%2.2d%2.2d_%2.2d%2.2d%2.2d.png",
            tm.tm_year-100, tm.tm_mon+1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec);

    // if caller has supplied region to print then 
    //   init rect to print with caller supplied position
    // else
    //   init rect to print with entire window
    // endif
    if (rect_arg) {
        rect.x = rect_arg->x;
        rect.y = rect_arg->y;
        rect.w = rect_arg->w;
        rect.h = rect_arg->h;
    } else {
        rect.x = 0;
        rect.y = 0;
        rect.w = sdl_win_info.w;
        rect.h = sdl_win_info.h;;   
    }

    // allocate memory for pixels
    pixels = calloc(1, rect.w * rect.h * BYTES_PER_PIXEL);
    if (pixels == NULL) {
        ERROR("allocate pixels failed\n");
        return;
    }

    // copy display rect to pixels
    ret = SDL_RenderReadPixels(sdl_renderer, 
                               &rect, 
                               SDL_PIXELFORMAT_ABGR8888, 
                               pixels, 
                               rect.w * BYTES_PER_PIXEL);
    if (ret < 0) {
        ERROR("SDL_RenderReadPixels, %s\n", SDL_GetError());
        free(pixels);
        return;
    }

    // write pixels to file_name, 
    // filename must have .png extension
    len = strlen(file_name);
    if (len > 4 && strcmp(file_name+len-4, ".png") == 0) {
        ret = write_png_file(file_name, pixels, rect.w, rect.h);
        if (ret != 0) {
            ERROR("write_png_file %s failed\n", file_name);
        }
    } else {
        ret = -1;
        ERROR("filename %s must have .jpg or .png extension\n", file_name);
    }
    if (ret != 0) {
        free(pixels);
        return;
    }

    // it worked, flash display if enabled;
    // the caller must redraw the screen if flash_display is enabled
    if (flash_display) {
        set_color(SDL_WHITE);
        SDL_RenderClear(sdl_renderer);
        SDL_RenderPresent(sdl_renderer);
        usleep(250000);
    }

    // free pixels
    free(pixels);
}

// -----------------  DISPLAY INIT AND PRESENT  ------------------------- 

void sdl_display_init(void)
{
    sdl_event_max = 0;
    set_color(SDL_BLACK);
    SDL_RenderClear(sdl_renderer);
}

void sdl_display_present(void)
{
    SDL_RenderPresent(sdl_renderer);
}

// -----------------  COLORS  ------------------------------------------- 

static void set_color(int32_t color)
{
    uint8_t r, g, b, a;
    uint32_t rgba;

    if (color < 0 || color >= MAX_SDL_COLOR_TO_RGBA) {
        FATAL("color %d out of range\n", color);
    }

    rgba = sdl_color_to_rgba[color];
    
    r = (rgba >>  0) & 0xff;
    g = (rgba >>  8) & 0xff;
    b = (rgba >> 16) & 0xff;
    a = (rgba >> 24) & 0xff;

    SDL_SetRenderDrawColor(sdl_renderer, r, g, b, a);
}

void sdl_define_custom_color(int32_t color, uint8_t r, uint8_t g, uint8_t b)
{
    if (color < FIRST_SDL_CUSTOM_COLOR || color >= MAX_SDL_COLOR_TO_RGBA) {
        FATAL("color %d out of range\n", color);
    }

    sdl_color_to_rgba[color] = (r << 0) | ( g << 8) | (b << 16) | (0xff << 24);
}

// ported from http://www.noah.org/wiki/Wavelength_to_RGB_in_Python
void sdl_wavelen_to_rgb(double wavelength, uint8_t *r, uint8_t *g, uint8_t *b)
{
    double attenuation;
    double gamma = 0.8;
    double R,G,B;

// xxx use a lookup table

    if (wavelength >= 380 && wavelength <= 440) {
        double attenuation = 0.3 + 0.7 * (wavelength - 380) / (440 - 380);
        R = pow((-(wavelength - 440) / (440 - 380)) * attenuation, gamma);
        G = 0.0;
        B = pow(1.0 * attenuation, gamma);
    } else if (wavelength >= 440 && wavelength <= 490) {
        R = 0.0;
        G = pow((wavelength - 440) / (490 - 440), gamma);
        B = 1.0;
    } else if (wavelength >= 490 && wavelength <= 510) {
        R = 0.0;
        G = 1.0;
        B = pow(-(wavelength - 510) / (510 - 490), gamma);
    } else if (wavelength >= 510 && wavelength <= 580) {
        R = pow((wavelength - 510) / (580 - 510), gamma);
        G = 1.0;
        B = 0.0;
    } else if (wavelength >= 580 && wavelength <= 645) {
        R = 1.0;
        G = pow(-(wavelength - 645) / (645 - 580), gamma);
        B = 0.0;
    } else if (wavelength >= 645 && wavelength <= 750) {
        attenuation = 0.3 + 0.7 * (750 - wavelength) / (750 - 645);
        R = pow(1.0 * attenuation, gamma);
        G = 0.0;
        B = 0.0;
    } else {
        R = 0.0;
        G = 0.0;
        B = 0.0;
    }

    if (R < 0) R = 0; else if (R > 1) R = 1;
    if (G < 0) G = 0; else if (G > 1) G = 1;
    if (B < 0) B = 0; else if (B > 1) B = 1;

    *r = R * 255;
    *g = G * 255;
    *b = B * 255;
}

// -----------------  FONT SUPPORT ROUTINES  ---------------------------- 

int32_t sdl_font_char_width(int32_t font_ptsize)
{
    font_init(font_ptsize);
    return sdl_font[font_ptsize].char_width;
}

int32_t sdl_font_char_height(int32_t font_ptsize)
{
    font_init(font_ptsize);
    return sdl_font[font_ptsize].char_height;
}

// -----------------  EVENT HANDLING  ----------------------------------- 

void sdl_register_event(rect_t * loc, int32_t event_id, int32_t event_type)
{
    sdl_event_reg_t * e = &sdl_event_reg_tbl[sdl_event_max];

    if (sdl_event_max == MAX_EVENT_REG_TBL) {
        ERROR("sdl_event_reg_tbl is full\n");
        return;
    }

    if (loc->w == 0 || loc->h == 0) {
        return;
    }

    e->event_id = event_id;
    e->event_type = event_type;
    e->disp_loc.x = loc->x;
    e->disp_loc.y = loc->y;
    e->disp_loc.w = loc->w;
    e->disp_loc.h = loc->h;

    sdl_event_max++;
}

void sdl_render_text_and_register_event(int32_t x, int32_t y,
        int32_t font_ptsize, char * str, int32_t fg_color, int32_t bg_color, 
        int32_t event_id, int32_t event_type)
{
    rect_t loc;

    sdl_render_text_ex(x, y, font_ptsize, str, fg_color, bg_color, &loc);
    if (loc.w == 0) {
        return;
    }
    sdl_register_event(&loc, event_id, event_type);
}

void sdl_render_texture_and_register_event(int32_t x, int32_t y,
        texture_t texture, int32_t event_id, int32_t event_type)
{
    rect_t loc;
    int w, h;

    sdl_render_texture(x, y, texture);

    sdl_query_texture(texture, &w, &h);
    loc.x = x;
    loc.y = y;
    loc.w = w;
    loc.h = h;

    sdl_register_event(&loc, event_id, event_type);
}

sdl_event_t * sdl_poll_event(void)
{
    #define AT_POS(X,Y,pos) (((X) >= (pos).x) && \
                             ((X) < (pos).x + (pos).w) && \
                             ((Y) >= (pos).y) && \
                             ((Y) < (pos).y + (pos).h))

    #define SDL_WINDOWEVENT_STR(x) \
       ((x) == SDL_WINDOWEVENT_SHOWN           ? "SDL_WINDOWEVENT_SHOWN"        : \
        (x) == SDL_WINDOWEVENT_HIDDEN          ? "SDL_WINDOWEVENT_HIDDEN"       : \
        (x) == SDL_WINDOWEVENT_EXPOSED         ? "SDL_WINDOWEVENT_EXPOSED"      : \
        (x) == SDL_WINDOWEVENT_MOVED           ? "SDL_WINDOWEVENT_MOVED"        : \
        (x) == SDL_WINDOWEVENT_RESIZED         ? "SDL_WINDOWEVENT_RESIZED"      : \
        (x) == SDL_WINDOWEVENT_SIZE_CHANGED    ? "SDL_WINDOWEVENT_SIZE_CHANGED" : \
        (x) == SDL_WINDOWEVENT_MINIMIZED       ? "SDL_WINDOWEVENT_MINIMIZED"    : \
        (x) == SDL_WINDOWEVENT_MAXIMIZED       ? "SDL_WINDOWEVENT_MAXIMIZED"    : \
        (x) == SDL_WINDOWEVENT_RESTORED        ? "SDL_WINDOWEVENT_RESTORED"     : \
        (x) == SDL_WINDOWEVENT_ENTER           ? "SDL_WINDOWEVENT_ENTER"        : \
        (x) == SDL_WINDOWEVENT_LEAVE           ? "SDL_WINDOWEVENT_LEAVE"        : \
        (x) == SDL_WINDOWEVENT_FOCUS_GAINED    ? "SDL_WINDOWEVENT_FOCUS_GAINED" : \
        (x) == SDL_WINDOWEVENT_FOCUS_LOST      ? "SDL_WINDOWEVENT_FOCUS_LOST"   : \
        (x) == SDL_WINDOWEVENT_CLOSE           ? "SDL_WINDOWEVENT_CLOSE"        : \
        (x) == SDL_WINDOWEVENT_TAKE_FOCUS      ? "SDL_WINDOWEVENT_TAKE_FOCUS" : \
        (x) == SDL_WINDOWEVENT_HIT_TEST        ? "SDL_WINDOWEVENT_HIT_TEST" : \
        (x) == SDL_WINDOWEVENT_ICCPROF_CHANGED ? "SDL_WINDOWEVENT_ICCPROF_CHANGED" : \
        (x) == SDL_WINDOWEVENT_DISPLAY_CHANGED ? "SDL_WINDOWEVENT_DISPLAY_CHANGED" : \
                                                 int_to_str(x))

    SDL_Event ev;
    int32_t i;

    static sdl_event_t event;
    static struct {
        bool active;
        int32_t event_id;
    } mouse_drag;

    memset(&event, 0,sizeof(event));

    // if a push event is pending then return that event;
    // the push event can be used to inject an event from another thread,
    // for example if another thread wishes to terminate the program it could
    // push the SDL_EVENT_QUIT
    if (sdl_push_ev.event_id != SDL_EVENT_NONE) {
        event = sdl_push_ev;
        sdl_push_ev.event_id = SDL_EVENT_NONE;
        return &event;
    }

    while (true) {
        // get the next event, break out of loop if no event
        if (SDL_PollEvent(&ev) == 0) {
            break;
        }

        // process the SDL event
        switch (ev.type) {
        case SDL_MOUSEBUTTONDOWN: {
            bool left_click, right_click;

            DEBUG("MOUSE DOWN which=%d button=%s state=%s x=%d y=%d\n",
                   ev.button.which,
                   (ev.button.button == SDL_BUTTON_LEFT   ? "LEFT" :
                    ev.button.button == SDL_BUTTON_MIDDLE ? "MIDDLE" :
                    ev.button.button == SDL_BUTTON_RIGHT  ? "RIGHT" :
                                                            "???"),
                   (ev.button.state == SDL_PRESSED  ? "PRESSED" :
                    ev.button.state == SDL_RELEASED ? "RELEASED" :
                                                      "???"),
                   ev.button.x,
                   ev.button.y);

            // determine if the left or right button is clicked;
            // if neither then break
            left_click = (ev.button.button == SDL_BUTTON_LEFT);
            right_click = (ev.button.button == SDL_BUTTON_RIGHT);
            if (!left_click && !right_click) {
                break;
            }

            // clear mouse_drag 
            memset(&mouse_drag,0,sizeof(mouse_drag));

            // search for matching registered mouse_click or mouse_drag event
            for (i = sdl_event_max-1; i >= 0; i--) {
                if (((sdl_event_reg_tbl[i].event_type == SDL_EVENT_TYPE_MOUSE_LEFT_CLICK && left_click) ||
                     (sdl_event_reg_tbl[i].event_type == SDL_EVENT_TYPE_MOUSE_RIGHT_CLICK && right_click) ||
                     (sdl_event_reg_tbl[i].event_type == SDL_EVENT_TYPE_MOUSE_DRAG && left_click)) &&
                    (AT_POS(ev.button.x, ev.button.y, sdl_event_reg_tbl[i].disp_loc))) 
                {
                    break;
                }
            }
            if (i < 0) {
                break;
            }

            // we've found a registered MOUSE_LEFT_CLICK or MOUSE_RIGHT_CLICK or MOUSE_DRAG event;
            // if it is a MOUSE_LEFT_CLICK or MOUSE_RIGHT_CLICK event then
            //   return the event to caller
            // else
            //   initialize mouse_drag state, which will be used in 
            //     the case SDL_MOUSEMOTION below
            // endif
            if (sdl_event_reg_tbl[i].event_type == SDL_EVENT_TYPE_MOUSE_LEFT_CLICK ||
                sdl_event_reg_tbl[i].event_type == SDL_EVENT_TYPE_MOUSE_RIGHT_CLICK) 
            {
                event.event_id = sdl_event_reg_tbl[i].event_id;
                event.mouse_click.x = ev.button.x - sdl_event_reg_tbl[i].disp_loc.x;
                event.mouse_click.y = ev.button.y - sdl_event_reg_tbl[i].disp_loc.y;
            } else {
                mouse_drag.active = true;
                mouse_drag.event_id = sdl_event_reg_tbl[i].event_id;
            }
            break; }

        case SDL_MOUSEBUTTONUP: {
            DEBUG("MOUSE UP which=%d button=%s state=%s x=%d y=%d\n",
                   ev.button.which,
                   (ev.button.button == SDL_BUTTON_LEFT   ? "LEFT" :
                    ev.button.button == SDL_BUTTON_MIDDLE ? "MIDDLE" :
                    ev.button.button == SDL_BUTTON_RIGHT  ? "RIGHT" :
                                                            "???"),
                   (ev.button.state == SDL_PRESSED  ? "PRESSED" :
                    ev.button.state == SDL_RELEASED ? "RELEASED" :
                                                      "???"),
                   ev.button.x,
                   ev.button.y);

            // clear mouse drag
            memset(&mouse_drag,0,sizeof(mouse_drag));
            break; }

        case SDL_MOUSEMOTION: {
            sdl_event_t mouse_pos_event;

            // if mouse_drag_active then return a MOUSE_DRAG event
            if (mouse_drag.active) {
                event.event_id = mouse_drag.event_id;
                event.mouse_drag.delta_x = 0;
                event.mouse_drag.delta_y = 0;
                do {
                    event.mouse_drag.delta_x += ev.motion.xrel;
                    event.mouse_drag.delta_y += ev.motion.yrel;
                } while (SDL_PeepEvents(&ev, 1, SDL_GETEVENT, SDL_MOUSEMOTION, SDL_MOUSEMOTION) == 1);
            }

            // determine if the mouse position corresponds to a registered MOUSE_POSITION event;
            // if not then break
            for (i = sdl_event_max-1; i >= 0; i--) {
                if ((sdl_event_reg_tbl[i].event_type == SDL_EVENT_TYPE_MOUSE_POSITION) &&
                    (AT_POS(ev.motion.x, ev.motion.y, sdl_event_reg_tbl[i].disp_loc))) 
                {
                    break;
                }
            }
            if (i < 0) {
                break;
            }

            // init the mouse_pos_event
            memset(&mouse_pos_event, 0, sizeof(mouse_pos_event));
            mouse_pos_event.event_id = sdl_event_reg_tbl[i].event_id;
            do {
                mouse_pos_event.mouse_position.x = ev.motion.x;
                mouse_pos_event.mouse_position.y = ev.motion.y;
            } while (SDL_PeepEvents(&ev, 1, SDL_GETEVENT, SDL_MOUSEMOTION, SDL_MOUSEMOTION) == 1);

            // if we are not returning the MOUSE_DRAG event then
            //   return the mouse_pos_event
            // else
            //   save the mouse_pos_event for the next call to sdl_poll_event
            // endif
            if (event.event_id == SDL_EVENT_NONE) {
                event = mouse_pos_event;
            } else {
                sdl_push_event(&mouse_pos_event);
            }
            break; }

        case SDL_MOUSEWHEEL: {
            int32_t mouse_x, mouse_y;

            // check if mouse wheel event is registered for the location of the mouse
            SDL_GetMouseState(&mouse_x, &mouse_y);
            for (i = sdl_event_max-1; i >= 0; i--) {
                if (sdl_event_reg_tbl[i].event_type == SDL_EVENT_TYPE_MOUSE_WHEEL &&
                    AT_POS(mouse_x, mouse_y, sdl_event_reg_tbl[i].disp_loc)) 
                {
                    break;
                }
            }

            // if did not find a registered mouse wheel event then get out
            if (i < 0) {
                break;
            }

            // set return event
            event.event_id = sdl_event_reg_tbl[i].event_id;
            event.mouse_wheel.delta_x = ev.wheel.x;
            event.mouse_wheel.delta_y = ev.wheel.y;
            break; }

        case SDL_KEYDOWN: {
            int32_t  key = ev.key.keysym.sym;
            bool     shift = (ev.key.keysym.mod & KMOD_SHIFT) != 0;
            bool     ctrl = (ev.key.keysym.mod & KMOD_CTRL) != 0;
            bool     alt = (ev.key.keysym.mod & KMOD_ALT) != 0;
            int32_t  event_id = SDL_EVENT_NONE;

            // this case returns event_id for special keyboard keys, such as
            // Home, Insert, RightArrow, ctrl-a, etc.

            // map key to event_id used by this code
            if (key == SDL_EVENT_KEY_ESC || key == SDL_EVENT_KEY_DELETE) {
                event_id = key;
            } else if (key >= 'a' && key < 'z' && (ctrl)) {
                event_id = key;
            } else if (key == SDLK_INSERT) {
                event_id = SDL_EVENT_KEY_INSERT;
            } else if (key == SDLK_HOME) {
                event_id = SDL_EVENT_KEY_HOME;
            } else if (key == SDLK_END) {
                event_id = SDL_EVENT_KEY_END;
            } else if (key == SDLK_PAGEUP) {
                event_id= SDL_EVENT_KEY_PGUP;
            } else if (key == SDLK_PAGEDOWN) {
                event_id = SDL_EVENT_KEY_PGDN;
            } else if (key == SDLK_UP) {
                event_id = SDL_EVENT_KEY_UP_ARROW;
            } else if (key == SDLK_DOWN) {
                event_id = SDL_EVENT_KEY_DOWN_ARROW;
            } else if (key == SDLK_LEFT) {
                event_id = SDL_EVENT_KEY_LEFT_ARROW;
            } else if (key == SDLK_RIGHT) {
                event_id = SDL_EVENT_KEY_RIGHT_ARROW;
            } else if (key >= SDLK_F1 && key <= SDLK_F12) {
                event_id = (key - SDLK_F1) + SDL_EVENT_KEY_F(1);
            } else if (key == SDLK_PRINTSCREEN) {
                event_id = SDL_EVENT_KEY_PRINTSCREEN;
            }

            // add shift/ctrl/alt to event-id
            if (event_id != SDL_EVENT_NONE) {
                if (shift) event_id += SDL_EVENT_KEYMOD_SHIFT;
                if (ctrl)  event_id += SDL_EVENT_KEYMOD_CTRL;
                if (alt)   event_id += SDL_EVENT_KEYMOD_ALT;
            }

            // if there is a keyboard event_id then return it
            if (event_id != SDL_EVENT_NONE) {
                event.event_id = event_id;
            }
            break; }

        case SDL_KEYUP:
            break;

        case SDL_TEXTINPUT: {
            char *s = ev.text.text;

            // this case returns ASCII keyboard codes, such as:
            // 'A', 'a', '<', '>', etc.
            if (s[0] >= 0x20 && s[0] < 0x7f) {
                event.event_id = s[0];
            }
            break; }

        case SDL_TEXTEDITING:
            break;

       case SDL_WINDOWEVENT: {
            DEBUG("got event SDL_WINOWEVENT - %s\n", SDL_WINDOWEVENT_STR(ev.window.event));
            switch (ev.window.event)  {
            case SDL_WINDOWEVENT_SIZE_CHANGED:
                sdl_win_info.w = ev.window.data1;
                sdl_win_info.h = ev.window.data2;
                break;
            case SDL_WINDOWEVENT_MINIMIZED:
                sdl_win_info.minimized = true;
                break;
            case SDL_WINDOWEVENT_RESTORED:
                sdl_win_info.minimized = false;
                break;
            case SDL_WINDOWEVENT_LEAVE:
                sdl_win_info.mouse_in_window = false;
                break;
            case SDL_WINDOWEVENT_ENTER:
                sdl_win_info.mouse_in_window = true;
                break;
            }
            event.event_id = SDL_EVENT_WINDOW;
            event.win_info = sdl_win_info;
            break; }

        case SDL_QUIT: {
            DEBUG("got event SDL_QUIT\n");
            event.event_id = SDL_EVENT_QUIT;
            break; }

        default: {
            DEBUG("got event %d - not supported\n", ev.type);
            break; }
        }

        // break if event is set
        if (event.event_id != SDL_EVENT_NONE) {
            break; 
        }
    }

    return &event;
}

// this function is thread-safe
// xxx could use improvement, such as a mutex and fifo of events
void sdl_push_event(sdl_event_t *ev) 
{
    int event_id = ev->event_id;
    sdl_event_t tmp = *ev;

    if (sdl_push_ev.event_id != SDL_EVENT_NONE) {
        ERROR("push event is pending\n");
        return;
    }

    tmp.event_id = SDL_EVENT_NONE;
    sdl_push_ev = tmp;
    __sync_synchronize();
    sdl_push_ev.event_id = event_id;
}

// -----------------  RENDER TEXT  -------------------------------------- 

void sdl_render_text(int32_t x, int32_t y, int32_t font_ptsize, char * str, 
                     int32_t fg_color, int32_t bg_color)
{
    sdl_render_text_ex(x, y, font_ptsize, str, fg_color, bg_color, NULL);
}

void sdl_render_text_ex(int32_t x, int32_t y, int32_t font_ptsize, char * str, 
                        int32_t fg_color, int32_t bg_color, rect_t *loc_arg)
{
    texture_t texture;
    int32_t   width, height;
    rect_t    loc = {0,0,0,0};

    // if zero length string just return
    if (str[0] == '\0') {
        if (loc_arg) *loc_arg = loc;
        return;
    }
    
    // create the text texture
    texture =  sdl_create_text_texture(fg_color, bg_color, font_ptsize, str);
    if (texture == NULL) {
        ERROR("sdl_create_text_texture failed\n");
        if (loc_arg) *loc_arg = loc;
        return;
    }
    sdl_query_texture(texture, &width, &height);

    // render the texture
    loc.x = x;
    loc.y = y; 
    loc.w = width;
    loc.h = height;
    sdl_render_scaled_texture(&loc, texture);

    // clean up
    sdl_destroy_texture(texture);

    // return the location of the text
    if (loc_arg) *loc_arg = loc;
}

void sdl_render_printf(int32_t x, int32_t y, int32_t font_ptsize,
                       int32_t fg_color, int32_t bg_color, char * fmt, ...) 
{
    char str[1000];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(str, sizeof(str), fmt, ap);
    va_end(ap);

    sdl_render_text(x, y, font_ptsize, str, fg_color, bg_color);
}

// -----------------  RENDER RECTANGLES  -------------------------------- 

void sdl_render_rect(rect_t * loc, int32_t line_width, int32_t color)
{
    SDL_Rect rect;
    int32_t i;

    set_color(color);

    rect.x = loc->x;
    rect.y = loc->y;
    rect.w = loc->w;
    rect.h = loc->h;

    for (i = 0; i < line_width; i++) {
        SDL_RenderDrawRect(sdl_renderer, &rect);
        if (rect.w < 2 || rect.h < 2) {
            break;
        }
        rect.x += 1;
        rect.y += 1;
        rect.w -= 2;
        rect.h -= 2;
    }
}

void sdl_render_fill_rect(rect_t * loc, int32_t color)
{
    SDL_Rect rect;

    set_color(color);

    rect.x = loc->x;
    rect.y = loc->y;
    rect.w = loc->w;
    rect.h = loc->h;
    SDL_RenderFillRect(sdl_renderer, &rect);
}

// -----------------  RENDER LINES  ------------------------------------- 

void sdl_render_line(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t color)
{
    point_t points[2] = { {x1,y1}, {x2,y2} };
    sdl_render_lines(points, 2, color);
}

void sdl_render_lines(point_t * points, int32_t count, int32_t color)
{
    #define MAX_SDL_POINTS 1000

    #define ADD_POINT_TO_ARRAY(_point) \
        do { \
            sdl_points[max].x = (_point).x; \
            sdl_points[max].y = (_point).y; \
            max++; \
    } while (0)

    SDL_Point sdl_points[MAX_SDL_POINTS];
    int32_t   i, max=0;

    // return if number of points supplied by caller is invalid
    if (count <= 1) {
        return;
    }

    // set color
    set_color(color);

    // loop over points
    for (i = 0; i < count; i++) {
        ADD_POINT_TO_ARRAY(points[i]);

        if (max == MAX_SDL_POINTS) {
            SDL_RenderDrawLines(sdl_renderer, sdl_points, max);
            max = 0;
            ADD_POINT_TO_ARRAY(points[i]);
        }
    }

    // finish rendering the lines
    if (max > 1) {
        SDL_RenderDrawLines(sdl_renderer, sdl_points, max);
    }
}

// -----------------  RENDER CIRCLE  ------------------------------------ 

void sdl_render_circle(int32_t x_center, int32_t y_center, int32_t radius, int32_t line_width, int32_t color)
{
    int32_t count = 0, i, angle;
    SDL_Point points[370];

    static int32_t sin_table[370];
    static int32_t cos_table[370];
    static bool first_call = true;

    // on first call make table of sin and cos indexed by degrees
    if (first_call) {
        for (angle = 0; angle < 362; angle++) {
            sin_table[angle] = sin(angle*(2*M_PI/360)) * (1<<18);
            cos_table[angle] = cos(angle*(2*M_PI/360)) * (1<<18);
        }
        first_call = false;
    }

    // set the color
    set_color(color);

    // loop over line_width
    for (i = 0; i < line_width; i++) {
        // draw circle
        count = 0;
        for (angle = 0; angle < 362; angle++) {
            points[count].x = x_center + (((int64_t)radius * sin_table[angle]) >> 18);
            points[count].y = y_center + (((int64_t)radius * cos_table[angle]) >> 18);
            count++;
        }
        SDL_RenderDrawLines(sdl_renderer, points, count);

        // reduce radius by 1
        radius--;
        if (radius < 0) {
            break;
        }
    }
}

// -----------------  RENDER POINTS  ------------------------------------ 

void sdl_render_point(int32_t x, int32_t y, int32_t color, int32_t point_size)
{
    point_t point = {x,y};
    sdl_render_points(&point, 1, color, point_size);
}

void sdl_render_points(point_t * points, int32_t count, int32_t color, int32_t point_size)
{
    #define MAX_SDL_POINTS 1000

    static struct point_extend_s {
        int32_t max;
        struct point_extend_offset_s {
            int32_t x;
            int32_t y;
        } offset[300];
    } point_extend[10] = {
    { 1, {
        {0,0}, 
            } },
    { 5, {
        {-1,0}, 
        {0,-1}, {0,0}, {0,1}, 
        {1,0}, 
            } },
    { 21, {
        {-2,-1}, {-2,0}, {-2,1}, 
        {-1,-2}, {-1,-1}, {-1,0}, {-1,1}, {-1,2}, 
        {0,-2}, {0,-1}, {0,0}, {0,1}, {0,2}, 
        {1,-2}, {1,-1}, {1,0}, {1,1}, {1,2}, 
        {2,-1}, {2,0}, {2,1}, 
            } },
    { 37, {
        {-3,-1}, {-3,0}, {-3,1}, 
        {-2,-2}, {-2,-1}, {-2,0}, {-2,1}, {-2,2}, 
        {-1,-3}, {-1,-2}, {-1,-1}, {-1,0}, {-1,1}, {-1,2}, {-1,3}, 
        {0,-3}, {0,-2}, {0,-1}, {0,0}, {0,1}, {0,2}, {0,3}, 
        {1,-3}, {1,-2}, {1,-1}, {1,0}, {1,1}, {1,2}, {1,3}, 
        {2,-2}, {2,-1}, {2,0}, {2,1}, {2,2}, 
        {3,-1}, {3,0}, {3,1}, 
            } },
    { 61, {
        {-4,-1}, {-4,0}, {-4,1}, 
        {-3,-3}, {-3,-2}, {-3,-1}, {-3,0}, {-3,1}, {-3,2}, {-3,3}, 
        {-2,-3}, {-2,-2}, {-2,-1}, {-2,0}, {-2,1}, {-2,2}, {-2,3}, 
        {-1,-4}, {-1,-3}, {-1,-2}, {-1,-1}, {-1,0}, {-1,1}, {-1,2}, {-1,3}, {-1,4}, 
        {0,-4}, {0,-3}, {0,-2}, {0,-1}, {0,0}, {0,1}, {0,2}, {0,3}, {0,4}, 
        {1,-4}, {1,-3}, {1,-2}, {1,-1}, {1,0}, {1,1}, {1,2}, {1,3}, {1,4}, 
        {2,-3}, {2,-2}, {2,-1}, {2,0}, {2,1}, {2,2}, {2,3}, 
        {3,-3}, {3,-2}, {3,-1}, {3,0}, {3,1}, {3,2}, {3,3}, 
        {4,-1}, {4,0}, {4,1}, 
            } },
    { 89, {
        {-5,-1}, {-5,0}, {-5,1}, 
        {-4,-3}, {-4,-2}, {-4,-1}, {-4,0}, {-4,1}, {-4,2}, {-4,3}, 
        {-3,-4}, {-3,-3}, {-3,-2}, {-3,-1}, {-3,0}, {-3,1}, {-3,2}, {-3,3}, {-3,4}, 
        {-2,-4}, {-2,-3}, {-2,-2}, {-2,-1}, {-2,0}, {-2,1}, {-2,2}, {-2,3}, {-2,4}, 
        {-1,-5}, {-1,-4}, {-1,-3}, {-1,-2}, {-1,-1}, {-1,0}, {-1,1}, {-1,2}, {-1,3}, {-1,4}, {-1,5}, 
        {0,-5}, {0,-4}, {0,-3}, {0,-2}, {0,-1}, {0,0}, {0,1}, {0,2}, {0,3}, {0,4}, {0,5}, 
        {1,-5}, {1,-4}, {1,-3}, {1,-2}, {1,-1}, {1,0}, {1,1}, {1,2}, {1,3}, {1,4}, {1,5}, 
        {2,-4}, {2,-3}, {2,-2}, {2,-1}, {2,0}, {2,1}, {2,2}, {2,3}, {2,4}, 
        {3,-4}, {3,-3}, {3,-2}, {3,-1}, {3,0}, {3,1}, {3,2}, {3,3}, {3,4}, 
        {4,-3}, {4,-2}, {4,-1}, {4,0}, {4,1}, {4,2}, {4,3}, 
        {5,-1}, {5,0}, {5,1}, 
            } },
    { 121, {
        {-6,-1}, {-6,0}, {-6,1}, 
        {-5,-3}, {-5,-2}, {-5,-1}, {-5,0}, {-5,1}, {-5,2}, {-5,3}, 
        {-4,-4}, {-4,-3}, {-4,-2}, {-4,-1}, {-4,0}, {-4,1}, {-4,2}, {-4,3}, {-4,4}, 
        {-3,-5}, {-3,-4}, {-3,-3}, {-3,-2}, {-3,-1}, {-3,0}, {-3,1}, {-3,2}, {-3,3}, {-3,4}, {-3,5}, 
        {-2,-5}, {-2,-4}, {-2,-3}, {-2,-2}, {-2,-1}, {-2,0}, {-2,1}, {-2,2}, {-2,3}, {-2,4}, {-2,5}, 
        {-1,-6}, {-1,-5}, {-1,-4}, {-1,-3}, {-1,-2}, {-1,-1}, {-1,0}, {-1,1}, {-1,2}, {-1,3}, {-1,4}, {-1,5}, {-1,6}, 
        {0,-6}, {0,-5}, {0,-4}, {0,-3}, {0,-2}, {0,-1}, {0,0}, {0,1}, {0,2}, {0,3}, {0,4}, {0,5}, {0,6}, 
        {1,-6}, {1,-5}, {1,-4}, {1,-3}, {1,-2}, {1,-1}, {1,0}, {1,1}, {1,2}, {1,3}, {1,4}, {1,5}, {1,6}, 
        {2,-5}, {2,-4}, {2,-3}, {2,-2}, {2,-1}, {2,0}, {2,1}, {2,2}, {2,3}, {2,4}, {2,5}, 
        {3,-5}, {3,-4}, {3,-3}, {3,-2}, {3,-1}, {3,0}, {3,1}, {3,2}, {3,3}, {3,4}, {3,5}, 
        {4,-4}, {4,-3}, {4,-2}, {4,-1}, {4,0}, {4,1}, {4,2}, {4,3}, {4,4}, 
        {5,-3}, {5,-2}, {5,-1}, {5,0}, {5,1}, {5,2}, {5,3}, 
        {6,-1}, {6,0}, {6,1}, 
            } },
    { 177, {
        {-7,-2}, {-7,-1}, {-7,0}, {-7,1}, {-7,2}, 
        {-6,-4}, {-6,-3}, {-6,-2}, {-6,-1}, {-6,0}, {-6,1}, {-6,2}, {-6,3}, {-6,4}, 
        {-5,-5}, {-5,-4}, {-5,-3}, {-5,-2}, {-5,-1}, {-5,0}, {-5,1}, {-5,2}, {-5,3}, {-5,4}, {-5,5}, 
        {-4,-6}, {-4,-5}, {-4,-4}, {-4,-3}, {-4,-2}, {-4,-1}, {-4,0}, {-4,1}, {-4,2}, {-4,3}, {-4,4}, {-4,5}, {-4,6}, 
        {-3,-6}, {-3,-5}, {-3,-4}, {-3,-3}, {-3,-2}, {-3,-1}, {-3,0}, {-3,1}, {-3,2}, {-3,3}, {-3,4}, {-3,5}, {-3,6}, 
        {-2,-7}, {-2,-6}, {-2,-5}, {-2,-4}, {-2,-3}, {-2,-2}, {-2,-1}, {-2,0}, {-2,1}, {-2,2}, {-2,3}, {-2,4}, {-2,5}, {-2,6}, {-2,7}, 
        {-1,-7}, {-1,-6}, {-1,-5}, {-1,-4}, {-1,-3}, {-1,-2}, {-1,-1}, {-1,0}, {-1,1}, {-1,2}, {-1,3}, {-1,4}, {-1,5}, {-1,6}, {-1,7}, 
        {0,-7}, {0,-6}, {0,-5}, {0,-4}, {0,-3}, {0,-2}, {0,-1}, {0,0}, {0,1}, {0,2}, {0,3}, {0,4}, {0,5}, {0,6}, {0,7}, 
        {1,-7}, {1,-6}, {1,-5}, {1,-4}, {1,-3}, {1,-2}, {1,-1}, {1,0}, {1,1}, {1,2}, {1,3}, {1,4}, {1,5}, {1,6}, {1,7}, 
        {2,-7}, {2,-6}, {2,-5}, {2,-4}, {2,-3}, {2,-2}, {2,-1}, {2,0}, {2,1}, {2,2}, {2,3}, {2,4}, {2,5}, {2,6}, {2,7}, 
        {3,-6}, {3,-5}, {3,-4}, {3,-3}, {3,-2}, {3,-1}, {3,0}, {3,1}, {3,2}, {3,3}, {3,4}, {3,5}, {3,6}, 
        {4,-6}, {4,-5}, {4,-4}, {4,-3}, {4,-2}, {4,-1}, {4,0}, {4,1}, {4,2}, {4,3}, {4,4}, {4,5}, {4,6}, 
        {5,-5}, {5,-4}, {5,-3}, {5,-2}, {5,-1}, {5,0}, {5,1}, {5,2}, {5,3}, {5,4}, {5,5}, 
        {6,-4}, {6,-3}, {6,-2}, {6,-1}, {6,0}, {6,1}, {6,2}, {6,3}, {6,4}, 
        {7,-2}, {7,-1}, {7,0}, {7,1}, {7,2}, 
            } },
    { 221, {
        {-8,-2}, {-8,-1}, {-8,0}, {-8,1}, {-8,2}, 
        {-7,-4}, {-7,-3}, {-7,-2}, {-7,-1}, {-7,0}, {-7,1}, {-7,2}, {-7,3}, {-7,4}, 
        {-6,-5}, {-6,-4}, {-6,-3}, {-6,-2}, {-6,-1}, {-6,0}, {-6,1}, {-6,2}, {-6,3}, {-6,4}, {-6,5}, 
        {-5,-6}, {-5,-5}, {-5,-4}, {-5,-3}, {-5,-2}, {-5,-1}, {-5,0}, {-5,1}, {-5,2}, {-5,3}, {-5,4}, {-5,5}, {-5,6}, 
        {-4,-7}, {-4,-6}, {-4,-5}, {-4,-4}, {-4,-3}, {-4,-2}, {-4,-1}, {-4,0}, {-4,1}, {-4,2}, {-4,3}, {-4,4}, {-4,5}, {-4,6}, {-4,7}, 
        {-3,-7}, {-3,-6}, {-3,-5}, {-3,-4}, {-3,-3}, {-3,-2}, {-3,-1}, {-3,0}, {-3,1}, {-3,2}, {-3,3}, {-3,4}, {-3,5}, {-3,6}, {-3,7}, 
        {-2,-8}, {-2,-7}, {-2,-6}, {-2,-5}, {-2,-4}, {-2,-3}, {-2,-2}, {-2,-1}, {-2,0}, {-2,1}, {-2,2}, {-2,3}, {-2,4}, {-2,5}, {-2,6}, {-2,7}, {-2,8}, 
        {-1,-8}, {-1,-7}, {-1,-6}, {-1,-5}, {-1,-4}, {-1,-3}, {-1,-2}, {-1,-1}, {-1,0}, {-1,1}, {-1,2}, {-1,3}, {-1,4}, {-1,5}, {-1,6}, {-1,7}, {-1,8}, 
        {0,-8}, {0,-7}, {0,-6}, {0,-5}, {0,-4}, {0,-3}, {0,-2}, {0,-1}, {0,0}, {0,1}, {0,2}, {0,3}, {0,4}, {0,5}, {0,6}, {0,7}, {0,8}, 
        {1,-8}, {1,-7}, {1,-6}, {1,-5}, {1,-4}, {1,-3}, {1,-2}, {1,-1}, {1,0}, {1,1}, {1,2}, {1,3}, {1,4}, {1,5}, {1,6}, {1,7}, {1,8}, 
        {2,-8}, {2,-7}, {2,-6}, {2,-5}, {2,-4}, {2,-3}, {2,-2}, {2,-1}, {2,0}, {2,1}, {2,2}, {2,3}, {2,4}, {2,5}, {2,6}, {2,7}, {2,8}, 
        {3,-7}, {3,-6}, {3,-5}, {3,-4}, {3,-3}, {3,-2}, {3,-1}, {3,0}, {3,1}, {3,2}, {3,3}, {3,4}, {3,5}, {3,6}, {3,7}, 
        {4,-7}, {4,-6}, {4,-5}, {4,-4}, {4,-3}, {4,-2}, {4,-1}, {4,0}, {4,1}, {4,2}, {4,3}, {4,4}, {4,5}, {4,6}, {4,7}, 
        {5,-6}, {5,-5}, {5,-4}, {5,-3}, {5,-2}, {5,-1}, {5,0}, {5,1}, {5,2}, {5,3}, {5,4}, {5,5}, {5,6}, 
        {6,-5}, {6,-4}, {6,-3}, {6,-2}, {6,-1}, {6,0}, {6,1}, {6,2}, {6,3}, {6,4}, {6,5}, 
        {7,-4}, {7,-3}, {7,-2}, {7,-1}, {7,0}, {7,1}, {7,2}, {7,3}, {7,4}, 
        {8,-2}, {8,-1}, {8,0}, {8,1}, {8,2}, 
            } },
    { 277, {
        {-9,-2}, {-9,-1}, {-9,0}, {-9,1}, {-9,2}, 
        {-8,-4}, {-8,-3}, {-8,-2}, {-8,-1}, {-8,0}, {-8,1}, {-8,2}, {-8,3}, {-8,4}, 
        {-7,-6}, {-7,-5}, {-7,-4}, {-7,-3}, {-7,-2}, {-7,-1}, {-7,0}, {-7,1}, {-7,2}, {-7,3}, {-7,4}, {-7,5}, {-7,6}, 
        {-6,-7}, {-6,-6}, {-6,-5}, {-6,-4}, {-6,-3}, {-6,-2}, {-6,-1}, {-6,0}, {-6,1}, {-6,2}, {-6,3}, {-6,4}, {-6,5}, {-6,6}, {-6,7}, 
        {-5,-7}, {-5,-6}, {-5,-5}, {-5,-4}, {-5,-3}, {-5,-2}, {-5,-1}, {-5,0}, {-5,1}, {-5,2}, {-5,3}, {-5,4}, {-5,5}, {-5,6}, {-5,7}, 
        {-4,-8}, {-4,-7}, {-4,-6}, {-4,-5}, {-4,-4}, {-4,-3}, {-4,-2}, {-4,-1}, {-4,0}, {-4,1}, {-4,2}, {-4,3}, {-4,4}, {-4,5}, {-4,6}, {-4,7}, {-4,8}, 
        {-3,-8}, {-3,-7}, {-3,-6}, {-3,-5}, {-3,-4}, {-3,-3}, {-3,-2}, {-3,-1}, {-3,0}, {-3,1}, {-3,2}, {-3,3}, {-3,4}, {-3,5}, {-3,6}, {-3,7}, {-3,8}, 
        {-2,-9}, {-2,-8}, {-2,-7}, {-2,-6}, {-2,-5}, {-2,-4}, {-2,-3}, {-2,-2}, {-2,-1}, {-2,0}, {-2,1}, {-2,2}, {-2,3}, {-2,4}, {-2,5}, {-2,6}, {-2,7}, {-2,8}, {-2,9}, 
        {-1,-9}, {-1,-8}, {-1,-7}, {-1,-6}, {-1,-5}, {-1,-4}, {-1,-3}, {-1,-2}, {-1,-1}, {-1,0}, {-1,1}, {-1,2}, {-1,3}, {-1,4}, {-1,5}, {-1,6}, {-1,7}, {-1,8}, {-1,9}, 
        {0,-9}, {0,-8}, {0,-7}, {0,-6}, {0,-5}, {0,-4}, {0,-3}, {0,-2}, {0,-1}, {0,0}, {0,1}, {0,2}, {0,3}, {0,4}, {0,5}, {0,6}, {0,7}, {0,8}, {0,9}, 
        {1,-9}, {1,-8}, {1,-7}, {1,-6}, {1,-5}, {1,-4}, {1,-3}, {1,-2}, {1,-1}, {1,0}, {1,1}, {1,2}, {1,3}, {1,4}, {1,5}, {1,6}, {1,7}, {1,8}, {1,9}, 
        {2,-9}, {2,-8}, {2,-7}, {2,-6}, {2,-5}, {2,-4}, {2,-3}, {2,-2}, {2,-1}, {2,0}, {2,1}, {2,2}, {2,3}, {2,4}, {2,5}, {2,6}, {2,7}, {2,8}, {2,9}, 
        {3,-8}, {3,-7}, {3,-6}, {3,-5}, {3,-4}, {3,-3}, {3,-2}, {3,-1}, {3,0}, {3,1}, {3,2}, {3,3}, {3,4}, {3,5}, {3,6}, {3,7}, {3,8}, 
        {4,-8}, {4,-7}, {4,-6}, {4,-5}, {4,-4}, {4,-3}, {4,-2}, {4,-1}, {4,0}, {4,1}, {4,2}, {4,3}, {4,4}, {4,5}, {4,6}, {4,7}, {4,8}, 
        {5,-7}, {5,-6}, {5,-5}, {5,-4}, {5,-3}, {5,-2}, {5,-1}, {5,0}, {5,1}, {5,2}, {5,3}, {5,4}, {5,5}, {5,6}, {5,7}, 
        {6,-7}, {6,-6}, {6,-5}, {6,-4}, {6,-3}, {6,-2}, {6,-1}, {6,0}, {6,1}, {6,2}, {6,3}, {6,4}, {6,5}, {6,6}, {6,7}, 
        {7,-6}, {7,-5}, {7,-4}, {7,-3}, {7,-2}, {7,-1}, {7,0}, {7,1}, {7,2}, {7,3}, {7,4}, {7,5}, {7,6}, 
        {8,-4}, {8,-3}, {8,-2}, {8,-1}, {8,0}, {8,1}, {8,2}, {8,3}, {8,4}, 
        {9,-2}, {9,-1}, {9,0}, {9,1}, {9,2}, 
            } },
                };

    int32_t i, j;
    SDL_Point sdl_points[MAX_SDL_POINTS];
    int32_t sdl_points_count = 0;
    struct point_extend_s * pe = &point_extend[point_size];
    struct point_extend_offset_s * peo = pe->offset;

    if (count < 0) {
        return;
    }
    if (point_size < 0 || point_size > 9) {
        return;
    }

    set_color(color);

    for (i = 0; i < count; i++) {
        for (j = 0; j < pe->max; j++) {
            sdl_points[sdl_points_count].x = points[i].x + peo[j].x;
            sdl_points[sdl_points_count].y = points[i].y + peo[j].y;
            sdl_points_count++;

            if (sdl_points_count == MAX_SDL_POINTS) {
                SDL_RenderDrawPoints(sdl_renderer, sdl_points, sdl_points_count);
                sdl_points_count = 0;
            }
        }
    }

    if (sdl_points_count > 0) {
        SDL_RenderDrawPoints(sdl_renderer, sdl_points, sdl_points_count);
        sdl_points_count = 0;
    }
}

// -----------------  RENDER USING TEXTURES  ---------------------------- 

texture_t sdl_create_texture(int32_t w, int32_t h)
{
    SDL_Texture * texture;

    texture = SDL_CreateTexture(sdl_renderer,
                                SDL_PIXELFORMAT_ABGR8888,
                                SDL_TEXTUREACCESS_STREAMING,
                                w, h);
    if (texture == NULL) {
        ERROR("failed to allocate texture, %s\n", SDL_GetError());
        return NULL;
    }

    return (texture_t)texture;
}

texture_t sdl_create_texture_from_win_pixels(rect_t *loc)
{
    texture_t texture;
    int32_t ret;
    uint8_t * pixels;
    SDL_Rect rect = {loc->x, loc->y, loc->w, loc->h};

    // allocate memory for the pixels
    pixels = calloc(1, rect.h * rect.w * BYTES_PER_PIXEL);
    if (pixels == NULL) {
        ERROR("allocate pixels failed\n");
        return NULL;
    }

    // read the pixels
    ret = SDL_RenderReadPixels(sdl_renderer, 
                               &rect,  
                               SDL_PIXELFORMAT_ABGR8888, 
                               pixels, 
                               rect.w * BYTES_PER_PIXEL);
    if (ret < 0) {
        ERROR("SDL_RenderReadPixels, %s\n", SDL_GetError());
        free(pixels);
        return NULL;
    }

    // create the texture
    texture = sdl_create_texture(rect.w, rect.h);
    if (texture == NULL) {
        ERROR("failed to allocate texture\n");
        free(pixels);
        return NULL;
    }

    // update the texture with the pixels
    SDL_UpdateTexture(texture, NULL, pixels, rect.w * BYTES_PER_PIXEL);

    // free pixels
    free(pixels);

    // return the texture
    return texture;
}

texture_t sdl_create_filled_circle_texture(int32_t radius, int32_t color)
{
    int32_t width = 2 * radius + 1;
    int32_t x = radius;
    int32_t y = 0;
    int32_t radiusError = 1-x;
    int32_t pixels[width][width];
    SDL_Texture * texture;
    uint32_t rgba = sdl_color_to_rgba[color];

    #define DRAWLINE(Y, XS, XE, V) \
        do { \
            int32_t i; \
            for (i = XS; i <= XE; i++) { \
                pixels[Y][i] = (V); \
            } \
        } while (0)

    // initialize pixels
    memset(pixels,0,sizeof(pixels));
    while(x >= y) {
        DRAWLINE(y+radius, -x+radius, x+radius, rgba);
        DRAWLINE(x+radius, -y+radius, y+radius, rgba);
        DRAWLINE(-y+radius, -x+radius, x+radius, rgba);
        DRAWLINE(-x+radius, -y+radius, y+radius, rgba);
        y++;
        if (radiusError<0) {
            radiusError += 2 * y + 1;
        } else {
            x--;
            radiusError += 2 * (y - x) + 1;
        }
    }

    // create the texture and copy the pixels to the texture
    texture = sdl_create_texture(width, width);
    if (texture == NULL) {
        ERROR("failed to allocate texture\n");
        return NULL;
    }
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(texture, NULL, pixels, width*BYTES_PER_PIXEL);

    // return texture
    return (texture_t)texture;
}

texture_t sdl_create_text_texture(int32_t fg_color, int32_t bg_color, int32_t font_ptsize, char * str)
{
    SDL_Surface * surface;
    SDL_Texture * texture;
    uint32_t      fg_rgba;
    uint32_t      bg_rgba;
    SDL_Color     fg_sdl_color;
    SDL_Color     bg_sdl_color UNUSED;

    if (str[0] == '\0') {
        return NULL;
    }

    fg_rgba = sdl_color_to_rgba[fg_color];
    fg_sdl_color.r = (fg_rgba >>  0) & 0xff;
    fg_sdl_color.g = (fg_rgba >>  8) & 0xff;
    fg_sdl_color.b = (fg_rgba >> 16) & 0xff;
    fg_sdl_color.a = (fg_rgba >> 24) & 0xff;

    bg_rgba = sdl_color_to_rgba[bg_color];
    bg_sdl_color.r = (bg_rgba >>  0) & 0xff;
    bg_sdl_color.g = (bg_rgba >>  8) & 0xff;
    bg_sdl_color.b = (bg_rgba >> 16) & 0xff;
    bg_sdl_color.a = (bg_rgba >> 24) & 0xff;

    // if the font has not been initialized then do so
    font_init(font_ptsize);

    // render the text to a surface,
    // create a texture from the surface
    // free the surface
#if 0
    surface = TTF_RenderUTF8_Shaded(sdl_font[font_ptsize].font, str, fg_sdl_color, bg_sdl_color);
#else
    surface = TTF_RenderUTF8_Blended_Wrapped(sdl_font[font_ptsize].font, str, fg_sdl_color, 0);
#endif
    if (surface == NULL) {
        ERROR("failed to allocate surface\n");
        return NULL;
    }
    texture = SDL_CreateTextureFromSurface(sdl_renderer, surface);
    if (texture == NULL) {
        ERROR("failed to allocate texture\n");
        SDL_FreeSurface(surface);
        return NULL;
    }
    SDL_FreeSurface(surface);

    // return the texture which contains the text
    return (texture_t)texture;
}

void sdl_update_texture(texture_t texture, uint8_t * pixels, int32_t pitch) 
{
    SDL_UpdateTexture((SDL_Texture*)texture,
                      NULL,                   // update entire texture
                      pixels,                 // pixels
                      pitch);                 // pitch  
}

void sdl_query_texture(texture_t texture, int32_t * width, int32_t * height)
{
    if (texture == NULL) {
        *width = 0;
        *height = 0;
        return;
    }

    SDL_QueryTexture((SDL_Texture *)texture, NULL, NULL, width, height);
}

void sdl_render_texture(int32_t x, int32_t y, texture_t texture)
{
    int32_t width, height;
    rect_t loc;

    // verify texture arg
    if (texture == NULL) {
        return;
    }

    // construct loc from caller supplied x,y and from the texture width,height
    sdl_query_texture(texture, &width, &height);
    loc.x = x;
    loc.y = y;
    loc.w = width;
    loc.h = height;
    
    // render the texture 
    sdl_render_scaled_texture(&loc, texture);
}

void sdl_render_scaled_texture(rect_t * loc, texture_t texture)
{
    SDL_Rect dstrect;

    dstrect.x = loc->x;
    dstrect.y = loc->y;
    dstrect.w = loc->w;
    dstrect.h = loc->h;

    // NULL means entire texture is copied
    SDL_RenderCopy(sdl_renderer, texture, NULL, &dstrect);
}

void sdl_render_scaled_texture_ex(rect_t *src, rect_t *dst, texture_t texture)
{
    SDL_Rect dstrect, srcrect;

    dstrect.x = dst->x;
    dstrect.y = dst->y;
    dstrect.w = dst->w;
    dstrect.h = dst->h;

    srcrect.x = src->x;
    srcrect.y = src->y;
    srcrect.w = src->w;
    srcrect.h = src->h;

    SDL_RenderCopy(sdl_renderer, texture, &srcrect, &dstrect);
}

void sdl_destroy_texture(texture_t texture)
{
    if (texture) {
        SDL_DestroyTexture((SDL_Texture *)texture);
    }
}

// -----------------  RENDER USING TEXTURES - WEBCAM SUPPORT ------------ 

// the webcams I use provide jpeg, which when decoded are in yuy2 pixel format

// - - - -  YUY2   - - - - - - - - 

texture_t sdl_create_yuy2_texture(int32_t w, int32_t h)
{
    SDL_Texture * texture;

    texture = SDL_CreateTexture(sdl_renderer,
                                SDL_PIXELFORMAT_YUY2,
                                SDL_TEXTUREACCESS_STREAMING,
                                w, h);
    if (texture == NULL) {
        ERROR("failed to allocate texture\n");
        return NULL;
    }

    return (texture_t)texture;
}

void sdl_update_yuy2_texture(texture_t texture, uint8_t * pixels, int32_t pitch)
{
    SDL_UpdateTexture((SDL_Texture*)texture,
                      NULL,            // update entire texture
                      pixels,          // pixels
                      pitch);          // pitch
}

// - - - -  IYUV   - - - - - - - - 

texture_t sdl_create_iyuv_texture(int32_t w, int32_t h)
{
    SDL_Texture * texture;

    texture = SDL_CreateTexture(sdl_renderer,
                                SDL_PIXELFORMAT_IYUV,
                                SDL_TEXTUREACCESS_STREAMING,
                                w, h);
    if (texture == NULL) {
        ERROR("failed to allocate texture\n");
        return NULL;
    }

    return (texture_t)texture;
}

void sdl_update_iyuv_texture(texture_t texture, 
                             uint8_t *y_plane, int y_pitch,
                             uint8_t *u_plane, int u_pitch,
                             uint8_t *v_plane, int v_pitch)
{
    SDL_UpdateYUVTexture((SDL_Texture*)texture,
                         NULL,            // update entire texture
                         y_plane, y_pitch,
                         u_plane, u_pitch,
                         v_plane, v_pitch);
}

