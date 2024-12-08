#include "defines.h"
#include "log.h"
#include "memory.h"
#include <SDL2/SDL.h>

#include <stdio.h>
#include <time.h>

typedef struct {
  f32 x;
  f32 y;
  f32 width;
  f32 hight;
} Rect;

void draw_rect(SDL_Surface *surface, Rect *rect, u32 color) {
  u32 rect_min_x = rect->x < rect->width / 2 ? 0 : rect->x - rect->width / 2;
  u32 rect_max_x = surface->w < rect->x + rect->width / 2
                       ? surface->w
                       : rect->x + rect->width / 2;

  u32 rect_min_y = rect->y < rect->hight / 2 ? 0 : rect->y - rect->hight / 2;
  u32 rect_max_y = surface->h < rect->y + rect->hight / 2
                       ? surface->h
                       : rect->y + rect->hight / 2;

  u8 *pixels_start =
      surface->pixels + rect_min_x * 4 + rect_min_y * surface->pitch;

  for (int y = 0; y < rect_max_y - rect_min_y; y++) {
    u8 *row = pixels_start + y * surface->pitch;
    for (int x = 0; x < rect_max_x - rect_min_x; x++) {
      *((u32 *)row + x) = color;
    }
  }
}

typedef struct {
  Memory memory;

  SDL_Window *window;
  SDL_Surface *surface;

  bool stop;
  clock_t time_old;
  clock_t time_new;
  f64 dt;

  f64 r;

  Rect rect;
  f32 rect_vel_x;
  f32 rect_vel_y;
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

  Rect rect = {
      .x = 0.0,
      .y = 0.0,
      .width = 100.0,
      .hight = 150.0,
  };
  game->rect = rect;
  game->rect_vel_x = 1.2;
  game->rect_vel_y = 2.1;
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

  game->rect.x += game->rect_vel_x;
  game->rect.y += game->rect_vel_y;

  if (game->rect.x < 0 || game->surface->w < game->rect.x) {
    game->rect_vel_x *= -1;
  }
  if (game->rect.y < 0 || game->surface->h < game->rect.y) {
    game->rect_vel_y *= -1;
  }

  SDL_FillRect(
      game->surface, 0,
      SDL_MapRGB(game->surface->format, (u8)((f64)255.0 * game->r), 0, 0));

  draw_rect(game->surface, &game->rect, 0xFFFFFFFF);

  SDL_UpdateWindowSurface(game->window);
}
