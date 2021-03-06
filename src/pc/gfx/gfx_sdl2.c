#include "../compat.h"

#if !defined(__BSD__) && !defined(TARGET_DOS) && (defined(ENABLE_OPENGL) || defined(ENABLE_OPENGL_LEGACY) || defined(ENABLE_SOFTRAST))

#ifdef __MINGW32__
#define FOR_WINDOWS 1
#else
#define FOR_WINDOWS 0
#endif

#if FOR_WINDOWS
#include <GL/glew.h>
#include "SDL.h"
#define GL_GLEXT_PROTOTYPES 1
#include "SDL_opengl.h"
#else
#include <SDL/SDL.h>
/*#define GL_GLEXT_PROTOTYPES 1
#ifdef ENABLE_OPENGL_LEGACY
#include <SDL2/SDL_opengl.h>
#else
#include <SDL2/SDL_opengles2.h>
#endif*/
#endif

#include "../common.h"
#include "../configfile.h"
#include "gfx_window_manager_api.h"
#include "gfx_screen_config.h"

#ifdef VERSION_EU
# define FRAMERATE 25
#else
# define FRAMERATE 30
#endif

#ifdef ENABLE_SOFTRAST
#define GFX_API_NAME "SDL1.2 - Software"
#include "gfx_soft.h"
//static SDL_Renderer *renderer;
SDL_Surface *sdl_screen = NULL;
SDL_PixelFormat sdl_screen_bgr;
static SDL_Surface *buffer = NULL;
static SDL_Surface *texture = NULL;
//static SDL_Texture *texture = NULL;
#else
#define GFX_API_NAME "SDL2 - OpenGL"
#endif

//static SDL_Window *wnd;
static int inverted_scancode_table[512];
static int vsync_enabled = 0;
static unsigned int window_width = DESIRED_SCREEN_WIDTH;
static unsigned int window_height = DESIRED_SCREEN_HEIGHT;
static bool fullscreen_state;
static void (*on_fullscreen_changed_callback)(bool is_now_fullscreen);
static bool (*on_key_down_callback)(int scancode);
static bool (*on_key_up_callback)(int scancode);
static void (*on_all_keys_up_callback)(void);

// time between consequtive game frames
const int frame_time = 1000 / FRAMERATE;

// fps stats tracking
static int f_frames = 0;
static double f_time = 0.0;


const SDLKey windows_scancode_table[] =
{
    /*	0						1							2							3							4						5							6							7 */
    /*	8						9							A							B							C						D							E							F */
    SDLK_UNKNOWN,		SDLK_ESCAPE,		SDLK_1,				SDLK_2,				SDLK_3,			SDLK_4,				SDLK_5,				SDLK_6,			/* 0 */
    SDLK_7,				SDLK_8,				SDLK_9,				SDLK_0,				SDLK_MINUS,		SDLK_EQUALS,		SDLK_BACKSPACE,		SDLK_TAB,		/* 0 */

    SDLK_q,				SDLK_w,				SDLK_e,				SDLK_r,				SDLK_t,			SDLK_y,				SDLK_u,				SDLK_i,			/* 1 */
    SDLK_o,				SDLK_p,				SDLK_LEFTBRACKET,	SDLK_RIGHTBRACKET,	SDLK_RETURN,	SDLK_LCTRL,			SDLK_a,				SDLK_s,			/* 1 */

    SDLK_d,				SDLK_f,				SDLK_g,				SDLK_h,				SDLK_j,			SDLK_k,				SDLK_l,				SDLK_SEMICOLON,	/* 2 */
    SDLK_UNKNOWN,	SDLK_UNKNOWN,			SDLK_LSHIFT,		SDLK_BACKSLASH,		SDLK_z,			SDLK_x,				SDLK_c,				SDLK_v,			/* 2 */

    SDLK_b,				SDLK_n,				SDLK_m,				SDLK_COMMA,			SDLK_PERIOD,	SDLK_SLASH,			SDLK_RSHIFT,		SDLK_PRINT,/* 3 */
    SDLK_LALT,			SDLK_SPACE,			SDLK_CAPSLOCK,		SDLK_F1,			SDLK_F2,		SDLK_F3,			SDLK_F4,			SDLK_F5,		/* 3 */

    SDLK_F6,			SDLK_F7,			SDLK_F8,			SDLK_F9,			SDLK_F10,		SDLK_NUMLOCK,	SDLK_SCROLLOCK,	SDLK_HOME,		/* 4 */
    SDLK_UP,			SDLK_PAGEUP,		SDLK_KP_MINUS,		SDLK_LEFT,			SDLK_KP5,		SDLK_RIGHT,			SDLK_KP_PLUS,		SDLK_END,		/* 4 */

    SDLK_DOWN,			SDLK_PAGEDOWN,		SDLK_INSERT,		SDLK_DELETE,		SDLK_UNKNOWN,	SDLK_UNKNOWN,		SDLK_BACKSLASH,SDLK_F11,		/* 5 */
    SDLK_F12,			SDLK_PAUSE,			SDLK_UNKNOWN,		SDLK_UNKNOWN,			SDLK_UNKNOWN,		SDLK_UNKNOWN,	SDLK_UNKNOWN,		SDLK_UNKNOWN,	/* 5 */

    SDLK_UNKNOWN,		SDLK_UNKNOWN,		SDLK_UNKNOWN,		SDLK_UNKNOWN,		SDLK_F13,		SDLK_F14,			SDLK_F15,			SDLK_UNKNOWN,		/* 6 */
    SDLK_UNKNOWN,			SDLK_UNKNOWN,			SDLK_UNKNOWN,			SDLK_UNKNOWN,		SDLK_UNKNOWN,	SDLK_UNKNOWN,		SDLK_UNKNOWN,		SDLK_UNKNOWN,	/* 6 */

    SDLK_UNKNOWN,		SDLK_UNKNOWN,		SDLK_UNKNOWN,		SDLK_UNKNOWN,		SDLK_UNKNOWN,	SDLK_UNKNOWN,		SDLK_UNKNOWN,		SDLK_UNKNOWN,	/* 7 */
    SDLK_UNKNOWN,		SDLK_UNKNOWN,		SDLK_UNKNOWN,		SDLK_UNKNOWN,		SDLK_UNKNOWN,	SDLK_UNKNOWN,		SDLK_UNKNOWN,		SDLK_UNKNOWN	/* 7 */
};

const SDLKey scancode_rmapping_extended[][2] = {
    {SDLK_KP_ENTER, SDLK_RETURN},
    {SDLK_RALT, SDLK_LALT},
    {SDLK_RCTRL, SDLK_LCTRL},
    {SDLK_KP_DIVIDE, SDLK_SLASH},
    //{SDLK_KP_PLUS, SDLK_CAPSLOCK}
};

const SDLKey scancode_rmapping_nonextended[][2] = {
    {SDLK_KP7, SDLK_HOME},
    {SDLK_KP8, SDLK_UP},
    {SDLK_KP9, SDLK_PAGEUP},
    {SDLK_KP4, SDLK_LEFT},
    {SDLK_KP6, SDLK_RIGHT},
    {SDLK_KP1, SDLK_END},
    {SDLK_KP2, SDLK_DOWN},
    {SDLK_KP3, SDLK_PAGEDOWN},
    {SDLK_KP0, SDLK_INSERT},
    {SDLK_KP_PERIOD, SDLK_DELETE},
    {SDLK_KP_MULTIPLY, SDLK_PRINT},
    {SDLK_UP, SDLK_UP},
    {SDLK_LEFT, SDLK_LEFT},
    {SDLK_RIGHT, SDLK_RIGHT},
    {SDLK_DOWN, SDLK_DOWN},
};

static void set_fullscreen(bool on, bool call_callback) {
    /*if (fullscreen_state == on) {
        return;
    }
    fullscreen_state = on;

    if (on) {
        SDL_DisplayMode mode;
        SDL_GetDesktopDisplayMode(0, &mode);
        window_width = mode.w;
        window_height = mode.h;
    } else {
        window_width = DESIRED_SCREEN_WIDTH;
        window_height = DESIRED_SCREEN_HEIGHT;
    }
    SDL_SetWindowSize(wnd, window_width, window_height);
    SDL_SetWindowFullscreen(wnd, on ? SDL_WINDOW_FULLSCREEN : 0);

    if (on_fullscreen_changed_callback != NULL && call_callback) {
        on_fullscreen_changed_callback(on);
    }*/
}

int test_vsync(void) {
    // Even if SDL_GL_SetSwapInterval succeeds, it doesn't mean that VSync actually works.
    // A 60 Hz monitor should have a swap interval of 16.67 milliseconds.
    // Try to detect the length of a vsync by swapping buffers some times.
    // Since the graphics card may enqueue a fixed number of frames,
    // first send in four dummy frames to hopefully fill the queue.
    // This method will fail if the refresh rate is changed, which, in
    // combination with that we can't control the queue size (i.e. lag)
    // is a reason this generic SDL2 backend should only be used as last resort.
    /*Uint32 start;
    Uint32 end;

    SDL_GL_SwapWindow(wnd);
    SDL_GL_SwapWindow(wnd);
    SDL_GL_SwapWindow(wnd);
    SDL_GL_SwapWindow(wnd);
    SDL_GL_SwapWindow(wnd);
    SDL_GL_SwapWindow(wnd);
    SDL_GL_SwapWindow(wnd);
    SDL_GL_SwapWindow(wnd);
    start = SDL_GetTicks();
    SDL_GL_SwapWindow(wnd);
    SDL_GL_SwapWindow(wnd);
    SDL_GL_SwapWindow(wnd);
    SDL_GL_SwapWindow(wnd);
    end = SDL_GetTicks();

    float average = 4.0 * 1000.0 / (end - start);

    vsync_enabled = 1;
    if (average > 27 && average < 33) {
        SDL_GL_SetSwapInterval(1);
    } else if (average > 57 && average < 63) {
        SDL_GL_SetSwapInterval(2);
    } else if (average > 86 && average < 94) {
        SDL_GL_SetSwapInterval(3);
    } else if (average > 115 && average < 125) {
        SDL_GL_SetSwapInterval(4);
    } else {
        vsync_enabled = 0;
    }*/
    vsync_enabled = 1;
}

static void gfx_sdl_init(const char *game_name, bool start_in_fullscreen) {
    SDL_Init(SDL_INIT_VIDEO);

    char title[512];
    sprintf(title, "%s (%s)", game_name, GFX_API_NAME);

	configScreenWidth = 320;
	configScreenHeight = 240;
    window_width = configScreenWidth;
    window_height = configScreenHeight;
#ifdef ENABLE_SOFTRAST

#ifdef CONVERT
	sdl_screen = SDL_SetVideoMode(window_width, window_height, 16, SDL_HWSURFACE | SDL_DOUBLEBUF);
#else
	sdl_screen = SDL_SetVideoMode(window_width, window_height, 32, SDL_HWSURFACE | SDL_DOUBLEBUF);
#endif
	//texture = SDL_CreateRGBSurface(SDL_HWSURFACE, window_width, window_height, 32, 0,0,0,0);

    /*wnd = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            window_width * 2, window_height * 2, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    renderer = SDL_CreateRenderer(wnd, -1, SDL_RENDERER_ACCELERATED);

    SDL_RenderSetIntegerScale(renderer, SDL_TRUE);
    SDL_RenderSetLogicalSize(renderer, window_width, window_height);

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, window_width, window_height);
    if (start_in_fullscreen) set_fullscreen(true, false);*/
#else
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    //SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    //SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    wnd = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            window_width, window_height, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    if (start_in_fullscreen) {
        set_fullscreen(true, false);
    }

    SDL_GL_CreateContext(wnd);

    SDL_GL_SetSwapInterval(1);
    test_vsync();
    if (!vsync_enabled)
        puts("Warning: VSync is not enabled or not working. Falling back to timer for synchronization");
#endif

    f_time = SDL_GetTicks();

    for (size_t i = 0; i < sizeof(windows_scancode_table) / sizeof(SDLKey); i++) {
        inverted_scancode_table[windows_scancode_table[i]] = i;
    }

    for (size_t i = 0; i < sizeof(scancode_rmapping_extended) / sizeof(scancode_rmapping_extended[0]); i++) {
        inverted_scancode_table[scancode_rmapping_extended[i][0]] = inverted_scancode_table[scancode_rmapping_extended[i][1]] + 0x100;
    }

    for (size_t i = 0; i < sizeof(scancode_rmapping_nonextended) / sizeof(scancode_rmapping_nonextended[0]); i++) {
        inverted_scancode_table[scancode_rmapping_nonextended[i][0]] = inverted_scancode_table[scancode_rmapping_nonextended[i][1]];
        inverted_scancode_table[scancode_rmapping_nonextended[i][1]] += 0x100;
    }
}

static void gfx_sdl_set_fullscreen_changed_callback(void (*on_fullscreen_changed)(bool is_now_fullscreen)) {
    on_fullscreen_changed_callback = on_fullscreen_changed;
}

static void gfx_sdl_set_fullscreen(bool enable) {
    set_fullscreen(enable, true);
}

static void gfx_sdl_set_keyboard_callbacks(bool (*on_key_down)(int scancode), bool (*on_key_up)(int scancode), void (*on_all_keys_up)(void)) {
    on_key_down_callback = on_key_down;
    on_key_up_callback = on_key_up;
    on_all_keys_up_callback = on_all_keys_up;
}

static void gfx_sdl_main_loop(void (*run_one_game_iter)(void)) {
    while (1) {
        run_one_game_iter();
    }
}

static void gfx_sdl_get_dimensions(uint32_t *width, uint32_t *height) {
#ifdef ENABLE_SOFTRAST
    *width = configScreenWidth;
    *height = configScreenHeight;
#else
    *width = window_width;
    *height = window_height;
#endif
}

static int translate_scancode(int scancode) {
    if (scancode < 512) {
        return inverted_scancode_table[scancode];
    } else {
        return 0;
    }
}

static void gfx_sdl_onkeydown(int scancode) {
    int key = translate_scancode(scancode);
    if (on_key_down_callback != NULL) {
        on_key_down_callback(key);
    }
}

static void gfx_sdl_onkeyup(int scancode) {
    int key = translate_scancode(scancode);
    if (on_key_up_callback != NULL) {
        on_key_up_callback(key);
    }
}

static void gfx_sdl_handle_events(void) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
#ifndef TARGET_WEB
            // Scancodes are broken in Emscripten SDL2: https://bugzilla.libsdl.org/show_bug.cgi?id=3259
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_F10) {
                    set_fullscreen(!fullscreen_state, true);
                    break;
                }
                gfx_sdl_onkeydown(event.key.keysym.sym);
                break;
            case SDL_KEYUP:
                gfx_sdl_onkeyup(event.key.keysym.sym);
                break;
#endif
            case SDL_QUIT:
                game_exit();
                break;
        }
    }
}

static bool gfx_sdl_start_frame(void) {
    return true;
}

static void sync_framerate_with_timer(void) {
    static Uint32 last_time = 0;
    // get base timestamp on the first frame (might be different from 0)
    if (last_time == 0) last_time = SDL_GetTicks();
    const int elapsed = SDL_GetTicks() - last_time;
    if (elapsed < frame_time)
        SDL_Delay(frame_time - elapsed);
    last_time += frame_time;
}

static void gfx_sdl_swap_buffers_begin(void) {
    if (!vsync_enabled) {
        sync_framerate_with_timer();
    }

#ifdef ENABLE_SOFTRAST
	memcpy(sdl_screen->pixels, (const void *)gfx_output, (window_width*window_height)*sdl_screen->format->BytesPerPixel);
	//SDL_BlitSurface(texture, NULL, sdl_screen, NULL);
	SDL_Flip(sdl_screen);
   /* SDL_UpdateTexture(texture, NULL, (const void *)gfx_output, 4 * configScreenWidth);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);*/
#else
    SDL_GL_SwapWindow(wnd);
#endif
}

static void gfx_sdl_swap_buffers_end(void) {
    f_frames++;
}

static double gfx_sdl_get_time(void) {
    return 0.0;
}

static void gfx_sdl_shutdown(void) {
    const double elapsed = (SDL_GetTicks() - f_time) / 1000.0;
    printf("\nstats\n");
    printf("frames    %010d\n", f_frames);
    printf("time      %010.4lf sec\n", elapsed);
    printf("frametime %010.8lf sec\n", elapsed / (double)f_frames);
    printf("framerate %010.5lf fps\n\n", (double)f_frames / elapsed);
    fflush(stdout);
}

struct GfxWindowManagerAPI gfx_sdl = {
    gfx_sdl_init,
    gfx_sdl_set_keyboard_callbacks,
    gfx_sdl_set_fullscreen_changed_callback,
    gfx_sdl_set_fullscreen,
    gfx_sdl_main_loop,
    gfx_sdl_get_dimensions,
    gfx_sdl_handle_events,
    gfx_sdl_start_frame,
    gfx_sdl_swap_buffers_begin,
    gfx_sdl_swap_buffers_end,
    gfx_sdl_get_time,
    gfx_sdl_shutdown,
};

#endif
