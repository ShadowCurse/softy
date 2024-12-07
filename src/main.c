#include <SDL2/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define bool uint8_t
#define true 1
#define false 0

#define u8 uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t

#define f32 float
#define f64 double

#define NS_PER_SEC 1000 * 1000 * 1000

#define FPS 60
#define FRAME_TIME_S 1.0 / FPS
#define FRAME_TIME_NS FRAME_TIME_S *NS_PER_SEC

static bool stop = false;
static SDL_Window *window;
static SDL_Surface *surface;

static clock_t time_old;
static f64 dt;
static f64 r;

void loop() {
  SDL_Event sdl_event;
  while (SDL_PollEvent(&sdl_event) != 0) {
    if (sdl_event.type == SDL_QUIT) {
      stop = true;
      break;
    }
  }

#ifndef __EMSCRIPTEN__
  clock_t time_new = clock();
  f64 dt = (f64)(time_new - time_old) / CLOCKS_PER_SEC;
  time_old = time_new;

  if (dt < FRAME_TIME_S) {
    f64 sleep_s = FRAME_TIME_S - dt;
    u64 sec = (u64)sleep_s;
    u64 nsec = (sleep_s - (f64)sec) * NS_PER_SEC;
    struct timespec req = {
        .tv_sec = sec,
        .tv_nsec = nsec,
    };

    while (nanosleep(&req, &req) == -1) {
    }

    dt = FRAME_TIME_S;
  }
#else
  f64 dt = FRAME_TIME_S;
#endif

  r += dt;
  if (1.0 < r) {
    r = 0;
  }

  SDL_FillRect(surface, 0,
               SDL_MapRGB(surface->format, (u8)((f64)255.0 * r), 0, 0));
  SDL_UpdateWindowSurface(window);
}

int main() {
  SDL_Init(SDL_INIT_VIDEO);

  window = SDL_CreateWindow("softy", SDL_WINDOWPOS_UNDEFINED,
                            SDL_WINDOWPOS_UNDEFINED, 640, 480, 0);
  if (!window) {
    printf("SDL error: %s", SDL_GetError());
    exit(1);
  }

  surface = SDL_GetWindowSurface(window);
  if (!surface) {
    printf("SDL error: %s", SDL_GetError());
    exit(1);
  }

#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop(loop, FPS, 1);
#else
  time_old = clock();
  r = 0;

  while (!stop) {
    loop();
  }

#endif

  SDL_DestroyWindow(window);
  SDL_Quit();
}
