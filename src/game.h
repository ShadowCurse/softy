#include "SDL2/SDL_surface.h"
#include "defines.h"
#include "log.h"
#include "memory.h"
#include <SDL2/SDL.h>

#include "stb_image.h"
#include "stb_truetype.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

#define MIN(a, b) a < b ? a : b
#define MAX(a, b) a < b ? b : a

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

typedef struct {
  f32 x;
  f32 y;
  f32 width;
  f32 hight;
} Rect;

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

Rect bitmap_full_rect(BitMap *bm) {
  Rect r = {
      .x = bm->width / 2.0,
      .y = bm->hight / 2.0,
      .width = bm->width,
      .hight = bm->hight,
  };
  return r;
}

// Set `dst` bitmap region `rect_dst` at (rect.x, rect.y) position to `color`
// assuming `dst` top left corner is at (0,0)
void blit_color_rect(BitMap *dst, Rect *rect_dst, u32 color, Rect *rect) {
  ASSERT((rect_dst->width <= dst->width), "Invalid blit dest rect");
  ASSERT((rect_dst->hight <= dst->hight), "Invalid blit dest rect");

  f32 rect_src_min_x = rect->x - rect->width / 2.0;
  f32 rect_src_max_x = rect->x + rect->width / 2.0;
  f32 rect_src_min_y = rect->y - rect->hight / 2.0;
  f32 rect_src_max_y = rect->y + rect->hight / 2.0;

  f32 rect_dst_min_x = rect_dst->x - rect_dst->width / 2.0;
  f32 rect_dst_max_x = rect_dst->x + rect_dst->width / 2.0;
  f32 rect_dst_min_y = rect_dst->y - rect_dst->hight / 2.0;
  f32 rect_dst_max_y = rect_dst->y + rect_dst->hight / 2.0;

  if (rect_dst_max_x < rect_src_min_x || rect_src_max_x < rect_dst_min_x ||
      rect_src_max_y < rect_dst_min_y || rect_dst_max_y < rect_src_min_y)
    return;

  f32 min_y = MAX(rect_dst_min_y, rect_src_min_y);
  f32 max_y = MIN(rect_dst_max_y, rect_src_max_y);
  f32 min_x = MAX(rect_dst_min_x, rect_src_min_x);
  f32 max_x = MIN(rect_dst_max_x, rect_src_max_x);

  u32 src_start_x_offset = min_x - rect_src_min_x;
  u32 src_start_y_offset = min_y - rect_src_min_y;

  u32 dst_start_x_offset = min_x - rect_dst_min_x;
  u32 dst_start_y_offset = min_y - rect_dst_min_y;

  u32 copy_area_width = max_x - min_x;
  u32 copy_area_hight = max_y - min_y;

  if (copy_area_width == 0 && copy_area_hight == 0)
    return;

  u8 *dst_start = dst->data + dst_start_x_offset * dst->channels +
                  dst_start_y_offset * (dst->width * dst->channels);
  for (u32 y = 0; y < copy_area_hight; y++) {
    u32 *dst_row = (u32 *)(dst_start + y * (dst->width * dst->channels));
    u32 count = copy_area_width;
    while (count--)
      *dst_row++ = color;
  }
}

// Copy `src` bitmap region `rect_src` into a `dst` bitmap region `rect_dst`
// at (pos_x, pos_y) position assuming `dst` top left corner is at (0,0)
// Apply tint when used with 1 channel src
void blit_bitmap(BitMap *dst, Rect *rect_dst, BitMap *src, Rect *rect_src,
                 f32 pos_x, f32 pos_y, u32 tint) {

  f32 rect_src_min_x;
  f32 rect_src_max_x;
  f32 rect_src_min_y;
  f32 rect_src_max_y;

  f32 rect_dst_min_x;
  f32 rect_dst_max_x;
  f32 rect_dst_min_y;
  f32 rect_dst_max_y;

  u8 *src_start;
  u8 *dst_start;

  if (rect_dst) {
    ASSERT((rect_dst->width <= dst->width), "Invalid blit rect_dst");
    ASSERT((rect_dst->hight <= dst->hight), "Invalid blit rect_dst");

    rect_dst_min_x = rect_dst->x - rect_dst->width / 2.0;
    rect_dst_max_x = rect_dst->x + rect_dst->width / 2.0;
    rect_dst_min_y = rect_dst->y - rect_dst->hight / 2.0;
    rect_dst_max_y = rect_dst->y + rect_dst->hight / 2.0;

    dst_start =
        dst->data + (u32)(rect_dst->x - rect_dst->width / 2.0) +
        (u32)(rect_dst->y - rect_dst->hight / 2.0) * dst->width * dst->channels;
  } else {
    rect_dst_min_x = 0.0;
    rect_dst_max_x = dst->width;
    rect_dst_min_y = 0.0;
    rect_dst_max_y = dst->hight;

    dst_start = dst->data;
  }

  if (rect_src) {
    ASSERT((rect_src->width <= src->hight), "Invalid blit rect_src");
    ASSERT((rect_src->hight <= src->hight), "Invalid blit rect_src");

    rect_src_min_x = pos_x - rect_src->width / 2.0;
    rect_src_max_x = pos_x + rect_src->width / 2.0;
    rect_src_min_y = pos_y - rect_src->hight / 2.0;
    rect_src_max_y = pos_y + rect_src->hight / 2.0;

    src_start =
        src->data + (u32)(rect_src->x - rect_src->width / 2.0) +
        (u32)(rect_src->y - rect_src->hight / 2.0) * src->width * src->channels;
  } else {
    rect_src_min_x = pos_x - src->width / 2.0;
    rect_src_max_x = pos_x + src->width / 2.0;
    rect_src_min_y = pos_y - src->hight / 2.0;
    rect_src_max_y = pos_y + src->hight / 2.0;

    src_start = src->data;
  }

  if (rect_dst_max_x < rect_src_min_x || rect_src_max_x < rect_dst_min_x ||
      rect_src_max_y < rect_dst_min_y || rect_dst_max_y < rect_src_min_y)
    return;

  f32 min_y = MAX(rect_dst_min_y, rect_src_min_y);
  f32 max_y = MIN(rect_dst_max_y, rect_src_max_y);
  f32 min_x = MAX(rect_dst_min_x, rect_src_min_x);
  f32 max_x = MIN(rect_dst_max_x, rect_src_max_x);

  u32 src_start_x_offset = min_x - rect_src_min_x;
  u32 src_start_y_offset = min_y - rect_src_min_y;

  u32 dst_start_x_offset = min_x - rect_dst_min_x;
  u32 dst_start_y_offset = min_y - rect_dst_min_y;

  u32 copy_area_width = max_x - min_x;
  u32 copy_area_hight = max_y - min_y;

  if (copy_area_width == 0 && copy_area_hight == 0)
    return;

  src_start += src_start_x_offset * src->channels +
               src_start_y_offset * (src->width * src->channels);

  dst_start += dst_start_x_offset * dst->channels +
               dst_start_y_offset * (dst->width * dst->channels);

  if (src->channels == dst->channels)
    for (u32 y = 0; y < copy_area_hight; y++) {
      u8 *src_row = src_start + y * (src->width * src->channels);
      u8 *dst_row = dst_start + y * (dst->width * dst->channels);
      memcpy(dst_row, src_row, copy_area_width * dst->channels);
    }
  else if (src->channels == 1 && dst->channels == 4)
    for (u32 y = 0; y < copy_area_hight; y++) {
      u8 *src_row = src_start + y * (src->width * src->channels);
      u32 *dst_row = (u32 *)(dst_start + y * (dst->width * dst->channels));
      for (u32 x = 0; x < copy_area_width; x++) {
        f32 s = ((f32)(*(src_row + x)) / 255.0);
        u32 a = (u32)(s * (f32)((tint & 0xFF000000) >> 24));
        u32 r = (u32)(s * (f32)((tint & 0x00FF0000) >> 16));
        u32 g = (u32)(s * (f32)((tint & 0x0000FF00) >> 8));
        u32 b = (u32)(s * (f32)((tint & 0x000000FF) >> 0));

        *(dst_row + x) = a << 24 | r << 16 | g << 8 | b << 0;
      }
    }
  else
    ASSERT(false,
           "No implementation for blit_bitmap from src %d channels to dst %d "
           "channels",
           src->channels, dst->channels);
}

void draw_char(BitMap *dst, Rect *rect_dst, Font *font, char c, u32 color,
               f32 x, f32 y) {
  BitMap font_bm = {
      .data = font->bitmap,
      .width = font->bitmap_width,
      .hight = font->bitmap_hight,
      .channels = 1,
  };
  Rect char_rect = {
      .x = (font->char_info[c].x1 + font->char_info[c].x0) / 2.0,
      .y = (font->char_info[c].y1 + font->char_info[c].y0) / 2.0,
      .width = (f32)(font->char_info[c].x1 - font->char_info[c].x0),
      .hight = (f32)(font->char_info[c].y1 - font->char_info[c].y0),
  };
  blit_bitmap(dst, rect_dst, &font_bm, &char_rect, x, y, color);
}

void draw_text(BitMap *dst, Rect *rect_dst, Font *font, const char *text,
               u32 color, f32 x, f32 y) {
  while (*text) {
    draw_char(dst, rect_dst, font, *text, color, x, y);
    x += (f32)(font->char_info[*text].xadvance);
    text++;
  }
}

typedef struct {
  Memory memory;

  SDL_Window *window;

  SDL_Surface *surface;
  BitMap surface_bm;
  Rect surface_rect;

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

void update_window_surface(Game *game) {
  game->surface = SDL_GetWindowSurface(game->window);
  ASSERT(game->surface, "SDL error: %s", SDL_GetError());

  BitMap surface_bm = {
      .data = game->surface->pixels,
      .width = game->surface->w,
      .hight = game->surface->h,
      .channels = 4,
  };
  game->surface_bm = surface_bm;

  Rect surface_rect = {
      .x = (f32)game->surface->w / 2.0,
      .y = (f32)game->surface->h / 2.0,
      .width = game->surface->w,
      .hight = game->surface->h,
  };
  game->surface_rect = surface_rect;
}

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

  update_window_surface(game);

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
        update_window_surface(game);
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

  blit_color_rect(&game->surface_bm, &game->surface_rect, 0xFFFFFFFF,
                  &game->rect);

  blit_bitmap(&game->surface_bm, NULL, &game->bm, NULL, game->rect.x,
              game->rect.y, 0);

  draw_char(&game->surface_bm, &game->surface_rect, &game->font, 'X',
            SDL_MapRGB(game->surface->format, 0, (u8)((f64)255.0 * game->r),
                       (u8)((f64)255.0 * game->r)),
            20.0, 20.0);

  draw_text(&game->surface_bm, &game->surface_rect, &game->font, "Test text",
            SDL_MapRGB(game->surface->format, 0, (u8)((f64)255.0 * game->r),
                       (u8)((f64)255.0 * game->r)),
            20.0, 50.0);

  SDL_UpdateWindowSurface(game->window);
}
