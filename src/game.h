#include "defines.h"
#include "log.h"
#include "memory.h"
#include <SDL2/SDL.h>

#include <stdio.h>
#include <time.h>

typedef struct {
  Memory memory;

  SDL_Window *window;
  SDL_Surface *surface;

  bool stop;
  clock_t time_old;
  clock_t time_new;
  f64 dt;

  f64 r;
  i32 rect_x;
  i32 rect_y;

  i32 rect_vel_x;
  i32 rect_vel_y;
} Game;

void init(Game *game) {
  if (!init_memory(&game->memory)) {
    exit(1);
  }

  perm_alloc(&game->memory, u64[2]);
  perm_alloc(&game->memory, u32[4]);
  frame_alloc(&game->memory, u64[1]);
  frame_alloc(
      &game->memory, struct {
        bool b;
        f32 f;
      });
  frame_alloc(&game->memory, u8[3]);
  frame_alloc(&game->memory, u64);
  printf("perm_mem end %lu\n", game->memory.perm_memory.end);
  printf("frame_mem end %lu\n", game->memory.frame_memory.end);
  frame_reset(&game->memory);
  printf("frame_mem end %lu\n", game->memory.frame_memory.end);

  INFO("test %d", 69);
  WARN("test %d", 69);
  ERROR("test %d", 69);
  DEBUG("test %d", 69);

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

  game->r = 0.0;
  game->rect_x = 0;
  game->rect_y = 0;
  game->rect_vel_x = 1;
  game->rect_vel_y = 2;
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

void draw_rect(Game *game, u32 x, u32 y, u32 width, u32 hight, u32 color) {
  u32 rect_min_x = x < width / 2 ? 0 : x - width / 2;
  u32 rect_max_x =
      game->surface->w < x + width / 2 ? game->surface->w : x + width / 2;

  u32 rect_min_y = y < hight / 2 ? 0 : y - hight / 2;
  u32 rect_max_y =
      game->surface->h < y + hight / 2 ? game->surface->h : y + hight / 2;

  u8 *pixels_start = game->surface->pixels + rect_min_x * 4 +
                     rect_min_y * game->surface->pitch;

  for (int y = 0; y < rect_max_y - rect_min_y; y++) {
    u8 *row = pixels_start + y * game->surface->pitch;
    for (int x = 0; x < rect_max_x - rect_min_x; x++) {
      *((u32 *)row + x) = color;
    }
  }
}

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

  game->rect_x += game->rect_vel_x;
  game->rect_y += game->rect_vel_y;

  if (game->rect_x < 0 || game->surface->w < game->rect_x) {
    game->rect_vel_x *= -1;
  }
  if (game->rect_y < 0 || game->surface->h < game->rect_y) {
    game->rect_vel_y *= -1;
  }

  if (game->rect_x < 0) {
    game->rect_x = 0;
  }
  if (game->rect_y < 0) {
    game->rect_y = 0;
  }

  SDL_FillRect(
      game->surface, 0,
      SDL_MapRGB(game->surface->format, (u8)((f64)255.0 * game->r), 0, 0));

  draw_rect(game, game->rect_x, game->rect_y, 400, 200, 0xFFFFFFFF);

  SDL_UpdateWindowSurface(game->window);
}
