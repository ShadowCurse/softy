#include "defines.h"
#include <SDL2/SDL.h>

#include <stdio.h>
#include <time.h>

typedef struct {
  SDL_Window *window;
  SDL_Surface *surface;

  bool stop;
  clock_t time_old;
  clock_t time_new;
  f64 dt;

  f64 r;
} Game;

void init(Game *game) {
  SDL_Init(SDL_INIT_VIDEO);

  game->window = SDL_CreateWindow("softy", SDL_WINDOWPOS_UNDEFINED,
                                  SDL_WINDOWPOS_UNDEFINED, 640, 480, 0);
  if (!game->window) {
    printf("SDL error: %s", SDL_GetError());
    exit(1);
  }

  game->surface = SDL_GetWindowSurface(game->window);
  if (!game->surface) {
    printf("SDL error: %s", SDL_GetError());
    exit(1);
  }

  game->dt = FRAME_TIME_S;
}

void destroy(Game *game) {
  SDL_DestroyWindow(game->window);
  SDL_Quit();
}

#ifndef __EMSCRIPTEN__
void cap_fps(Game *game) {
  game->time_new = clock();
  game->dt = (f64)(game->time_new - game->time_old) / CLOCKS_PER_SEC;
  game->time_old = game->time_new;

  if (game->dt < FRAME_TIME_S) {
    f64 sleep_s = FRAME_TIME_S - game->dt;
    u64 sec = (u64)sleep_s;
    u64 nsec = (sleep_s - (f64)sec) * NS_PER_SEC;
    struct timespec req = {
        .tv_sec = sec,
        .tv_nsec = nsec,
    };

    while (nanosleep(&req, &req) == -1) {
    }

    game->dt = FRAME_TIME_S;
  }
}
#endif

void run(Game *game) {
  SDL_Event sdl_event;
  while (SDL_PollEvent(&sdl_event) != 0) {
    if (sdl_event.type == SDL_QUIT) {
      game->stop = true;
      break;
    }
  }

#ifndef __EMSCRIPTEN__
  cap_fps(game);
#endif

  game->r += game->dt;
  if (1.0 < game->r) {
    game->r = 0;
  }

  SDL_FillRect(
      game->surface, 0,
      SDL_MapRGB(game->surface->format, (u8)((f64)255.0 * game->r), 0, 0));
  SDL_UpdateWindowSurface(game->window);
}
