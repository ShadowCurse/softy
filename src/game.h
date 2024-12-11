#include "defines.h"
#include "log.h"
#include "memory.h"
#include <SDL2/SDL.h>

#include "stb_image.h"
#include "stb_truetype.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

typedef struct {
  stbtt_bakedchar *char_info;
  u8 *bitmap;
  u32 bitmap_width;
  u32 bitmap_hight;
} Font;

Font load_font(Memory *memory, const char *font_path, f32 font_size,
               u32 bitmap_width, u32 bitmap_hight) {
  i32 fd = open(font_path, O_RDONLY);
  ASSERT((0 < fd), "Failed to open font file 2: %s", font_path);

  struct stat sb;
  ASSERT((fstat(fd, &sb) != -1), "Failed to get a font file %s size",
         font_path);

  u8 *file_mem = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  ASSERT(file_mem, "Failed to mmap font file: %s", font_path);

  stbtt_fontinfo stb_font;
  stbtt_InitFont(&stb_font, file_mem, stbtt_GetFontOffsetForIndex(file_mem, 0));

  Font font = {
      .char_info =
          perm_alloc_array(memory, stbtt_bakedchar, stb_font.numGlyphs),
      .bitmap = perm_alloc_array(memory, u8, bitmap_width * bitmap_hight),
      .bitmap_width = bitmap_width,
      .bitmap_hight = bitmap_hight,
  };

  stbtt_BakeFontBitmap(file_mem, 0, font_size, font.bitmap, bitmap_width,
                       bitmap_hight, 0, stb_font.numGlyphs, font.char_info);

  munmap(file_mem, sb.st_size);

  INFO("Loaded font %s with %d glyphs", font_path, stb_font.numGlyphs);

  return font;
}

void draw_character(SDL_Surface *surface, Font *font, char c, u32 color, f32 x,
                    f32 y) {
  u32 char_width = font->char_info[c].x1 - font->char_info[c].x0;
  u32 char_hight = font->char_info[c].y1 - font->char_info[c].y0;
  u32 bitmap_min_x = font->char_info[c].x0;
  u32 bitmap_max_x = font->char_info[c].x1;
  u32 bitmap_min_y = font->char_info[c].y0;
  u32 bitmap_max_y = font->char_info[c].y1;

  u32 surface_min_x = 0;
  u32 surface_max_x = surface->w;
  u32 surface_min_y = 0;
  u32 surface_max_y = surface->h;

  if (x < (f32)char_width / 2.0) {
    bitmap_min_x += (f32)char_width / 2.0 - x;
  } else {
    surface_min_x = x - (f32)char_width / 2.0;
  }

  if (surface->w < x + (f32)char_width / 2.0) {
    bitmap_max_x -= x + (f32)char_width / 2.0 - (f32)surface->w;
  } else {
    surface_max_x = x + (f32)char_width / 2.0;
  }

  if (y < (f32)char_hight / 2.0) {
    bitmap_min_y += (f32)char_hight / 2.0 - y;
  } else {
    surface_min_y = y - (f32)char_hight / 2.0;
  }

  if (surface->h < y + (f32)char_hight / 2.0) {
    bitmap_max_y -= y + (f32)char_hight / 2.0 - (f32)surface->h;
  } else {
    surface_max_y = y + (f32)char_hight / 2.0;
  }

  u8 *surface_start =
      surface->pixels + surface_min_x * 4 + surface_min_y * surface->pitch;

  u8 *bitmap_start =
      font->bitmap + bitmap_min_x + bitmap_min_y * font->bitmap_width;

  for (int y = 0; y < bitmap_max_y - bitmap_min_y; y++) {
    u8 *surface_row = surface_start + y * surface->pitch;
    u8 *bitmap_row = bitmap_start + y * font->bitmap_width;
    for (u32 x = 0; x < bitmap_max_x - bitmap_min_x; x++) {
      f32 s = ((f32)(*(bitmap_row + x)) / 255.0);
      u32 a = (u32)(s * (f32)((color & 0xFF000000) >> 24));
      u32 r = (u32)(s * (f32)((color & 0x00FF0000) >> 16));
      u32 g = (u32)(s * (f32)((color & 0x0000FF00) >> 8));
      u32 b = (u32)(s * (f32)((color & 0x000000FF) >> 0));

      *((u32 *)surface_row + x) = a << 24 | r << 16 | g << 8 | b << 0;
    }
  }
}

void draw_text(SDL_Surface *surface, Font *font, const char *text, u32 color,
               f32 x, f32 y) {
  while (*text) {
    draw_character(surface, font, *text, color, x, y);
    x += (f32)(font->char_info[*text].xadvance);
    text += 1;
  }
}

typedef struct {
  u32 width;
  u32 hight;
  u32 channels;
  u8 *data;
} BitMap;

BitMap load_bitmap(Memory *memory, const char *filename) {
  i32 x;
  i32 y;
  i32 n;
  u8 *data = stbi_load(filename, &x, &y, &n, 0);
  ASSERT(data, "Failed to load bitmap from %s", filename);

  BitMap bm = {
      .width = (u32)x,
      .hight = (u32)y,
      .channels = (u32)n,
      .data = perm_alloc_array(memory, u8, x * y * n),
  };
  memcpy(bm.data, data, x * y * n);

  stbi_image_free(data);

  return bm;
}

void draw_bitmap(SDL_Surface *surface, BitMap *bitmap, f32 x, f32 y) {
  u32 bitmap_min_x = 0;
  u32 bitmap_max_x = bitmap->width;
  u32 bitmap_min_y = 0;
  u32 bitmap_max_y = bitmap->hight;

  u32 surface_min_x = 0;
  u32 surface_max_x = surface->w;
  u32 surface_min_y = 0;
  u32 surface_max_y = surface->h;

  if (x < (f32)bitmap->width / 2.0) {
    bitmap_min_x += (f32)bitmap->width / 2.0 - x;
  } else {
    surface_min_x = x - (f32)bitmap->width / 2.0;
  }

  if (surface->w < x + (f32)bitmap->width / 2.0) {
    bitmap_max_x -= x + (f32)bitmap->width / 2.0 - (f32)surface->w;
  } else {
    surface_max_x = x + (f32)bitmap->width / 2.0;
  }

  if (y < (f32)bitmap->hight / 2.0) {
    bitmap_min_y += (f32)bitmap->hight / 2.0 - y;
  } else {
    surface_min_y = y - (f32)bitmap->hight / 2.0;
  }

  if (surface->h < y + (f32)bitmap->hight / 2.0) {
    bitmap_max_y -= y + (f32)bitmap->hight / 2.0 - (f32)surface->h;
  } else {
    surface_max_y = y + (f32)bitmap->hight / 2.0;
  }

  u8 *surface_start =
      surface->pixels + surface_min_x * 4 + surface_min_y * surface->pitch;

  u8 *bitmap_start = bitmap->data + bitmap_min_x * bitmap->channels +
                     bitmap_min_y * bitmap->width * bitmap->channels;

  for (int y = 0; y < bitmap_max_y - bitmap_min_y; y++) {
    u8 *surface_row = surface_start + y * surface->pitch;
    u8 *bitmap_row = bitmap_start + y * bitmap->width * bitmap->channels;
    memcpy(surface_row, bitmap_row, (bitmap_max_x - bitmap_min_x) * 4);
  }
}

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

  BitMap bm;
  Font font;
} Game;

void init(Game *game) {
  if (!init_memory(&game->memory)) {
    exit(1);
  }

  perm_alloc((&game->memory), u64[2]);
  perm_alloc((&game->memory), u32[4]);
  frame_alloc((&game->memory), u64[1]);
  frame_alloc((&game->memory), struct {
    bool b;
    f32 f;
  });
  frame_alloc((&game->memory), u8[3]);
  frame_alloc((&game->memory), u64);
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
  ASSERT(game->window, "SDL error: %s", SDL_GetError());

  game->surface = SDL_GetWindowSurface(game->window);
  ASSERT(game->surface, "SDL error: %s", SDL_GetError());

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

  game->bm = load_bitmap(&game->memory, "assets/a.png");
  game->font = load_font(&game->memory, "assets/font.ttf", 32.0, 512, 512);
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
    switch (sdl_event.type) {
    case SDL_QUIT:
      game->stop = true;
      break;
    case SDL_WINDOWEVENT:
      switch (sdl_event.window.event) {
      case SDL_WINDOWEVENT_RESIZED:
      case SDL_WINDOWEVENT_SIZE_CHANGED:
        game->surface = SDL_GetWindowSurface(game->window);
        ASSERT(game->surface, "SDL error: %s", SDL_GetError());
        break;
      default:
        break;
      }
      break;
    default:
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
  draw_bitmap(game->surface, &game->bm, game->rect.x, game->rect.y);
  draw_character(game->surface, &game->font, 'X',
                 SDL_MapRGB(game->surface->format, 0,
                            (u8)((f64)255.0 * game->r),
                            (u8)((f64)255.0 * game->r)),
                 20.0, 20.0);

  draw_text(game->surface, &game->font, "Test text",
            SDL_MapRGB(game->surface->format, 0, (u8)((f64)255.0 * game->r),
                       (u8)((f64)255.0 * game->r)),
            20.0, 50.0);

  SDL_UpdateWindowSurface(game->window);
}
