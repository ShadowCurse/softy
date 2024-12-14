#include "SDL2/SDL_surface.h"
#include "defines.h"
#include "log.h"
#include "math.h"
#include "memory.h"
#include <SDL2/SDL.h>

#include "stb_image.h"
#include "stb_truetype.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

#define WINDOW_WIDTH 1280
#define WINDOW_HIGHT 720

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
      .pos = {bm->width / 2.0, bm->hight / 2.0},
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

  AABB aabb_src = rect_aabb(rect);
  AABB aabb_dst = rect_aabb(rect_dst);
  if (!aabb_intersect(&aabb_src, &aabb_dst))
    return;

  AABB intersection = aabb_intersection(&aabb_src, &aabb_dst);

  u32 copy_area_width = aabb_width(&intersection);
  u32 copy_area_hight = aabb_hight(&intersection);
  if (copy_area_width == 0 && copy_area_hight == 0)
    return;

  V2 src_start_offset = v2_sub(intersection.min, aabb_src.min);
  V2 dst_start_offset = v2_sub(intersection.min, aabb_dst.min);

  u8 *dst_start = dst->data + (u32)dst_start_offset.x * dst->channels +
                  (u32)dst_start_offset.y * (dst->width * dst->channels);
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
                 V2 pos, u32 tint) {
  AABB aabb_src;
  u8 *src_start;
  if (rect_src) {
    ASSERT((rect_src->width <= src->hight), "Invalid blit rect_src");
    ASSERT((rect_src->hight <= src->hight), "Invalid blit rect_src");
    aabb_src = aabb_from_parts(pos, (V2){rect_src->width, rect_src->hight});
    src_start = src->data + (u32)(rect_src->pos.x - rect_src->width / 2.0) +
                (u32)(rect_src->pos.y - rect_src->hight / 2.0) * src->width *
                    src->channels;
  } else {
    aabb_src = aabb_from_parts(pos, (V2){src->width, src->hight});
    src_start = src->data;
  }

  AABB aabb_dst;
  u8 *dst_start;
  if (rect_dst) {
    ASSERT((rect_dst->width <= dst->width), "Invalid blit rect_dst");
    ASSERT((rect_dst->hight <= dst->hight), "Invalid blit rect_dst");
    aabb_dst = rect_aabb(rect_dst);
    dst_start = dst->data + (u32)(rect_dst->pos.x - rect_dst->width / 2.0) +
                (u32)(rect_dst->pos.y - rect_dst->hight / 2.0) * dst->width *
                    dst->channels;
  } else {
    aabb_dst = (AABB){{0.0, 0.0}, {dst->width, dst->hight}};
    dst_start = dst->data;
  }

  if (!aabb_intersect(&aabb_src, &aabb_dst))
    return;

  AABB intersection = aabb_intersection(&aabb_src, &aabb_dst);

  u32 copy_area_width = aabb_width(&intersection);
  u32 copy_area_hight = aabb_hight(&intersection);
  if (copy_area_width == 0 && copy_area_hight == 0)
    return;

  V2 src_start_offset = v2_sub(intersection.min, aabb_src.min);
  V2 dst_start_offset = v2_sub(intersection.min, aabb_dst.min);

  src_start += (u32)src_start_offset.x * src->channels +
               (u32)src_start_offset.y * (src->width * src->channels);

  dst_start += (u32)dst_start_offset.x * dst->channels +
               (u32)dst_start_offset.y * (dst->width * dst->channels);

  f32 tint_mul_a = (f32)((tint >> 24) & 0xFF) / 255.0;
  f32 tint_mul_r = (f32)((tint >> 16) & 0xFF) / 255.0;
  f32 tint_mul_g = (f32)((tint >> 8) & 0xFF) / 255.0;
  f32 tint_mul_b = (f32)((tint >> 0) & 0xFF) / 255.0;

  if (src->channels == dst->channels)
    for (u32 y = 0; y < copy_area_hight; y++) {
      u8 *src_row = src_start + y * (src->width * src->channels);
      u8 *dst_row = dst_start + y * (dst->width * dst->channels);
      for (u32 x = 0; x < copy_area_width; x++) {
        u32 *src_color = (u32 *)(src_row + x * src->channels);
        f32 src_a = (f32)((*src_color >> 24) & 0xFF) / 255.0;
        f32 src_r = (f32)((*src_color >> 16) & 0xFF);
        f32 src_g = (f32)((*src_color >> 8) & 0xFF);
        f32 src_b = (f32)((*src_color >> 0) & 0xFF);

        src_a *= tint_mul_a;
        src_r *= tint_mul_r;
        src_g *= tint_mul_g;
        src_b *= tint_mul_b;

        u32 *dst_color = (u32 *)(dst_row + x * dst->channels);
        f32 dst_r = (f32)((*dst_color >> 16) & 0xFF);
        f32 dst_g = (f32)((*dst_color >> 8) & 0xFF);
        f32 dst_b = (f32)((*dst_color >> 0) & 0xFF);

        u32 out_r = (u32)(lerp(dst_r, src_r, src_a));
        u32 out_g = (u32)(lerp(dst_g, src_g, src_a));
        u32 out_b = (u32)(lerp(dst_b, src_b, src_a));

        *dst_color = out_r << 16 | out_g << 8 | out_b << 0;
      }
    }
  else if (src->channels == 1 && dst->channels == 4)
    for (u32 y = 0; y < copy_area_hight; y++) {
      u8 *src_row = src_start + y * (src->width * src->channels);
      u8 *dst_row = dst_start + y * (dst->width * dst->channels);
      for (u32 x = 0; x < copy_area_width; x++) {
        u8 src_color = *(src_row + x);
        f32 src_a = (f32)(src_color) / 255.0;
        f32 src_r = src_color;
        f32 src_g = src_color;
        f32 src_b = src_color;

        src_a *= tint_mul_a;
        src_r *= tint_mul_r;
        src_g *= tint_mul_g;
        src_b *= tint_mul_b;

        u32 *dst_color = (u32 *)(dst_row + x * dst->channels);
        f32 dst_r = (f32)((*dst_color >> 16) & 0xFF);
        f32 dst_g = (f32)((*dst_color >> 8) & 0xFF);
        f32 dst_b = (f32)((*dst_color >> 0) & 0xFF);

        u32 out_r = (u32)(lerp(dst_r, src_r, src_a));
        u32 out_g = (u32)(lerp(dst_g, src_g, src_a));
        u32 out_b = (u32)(lerp(dst_b, src_b, src_a));

        // *(dst_row + x) = a << 24 | r << 16 | g << 8 | b << 0;
        *dst_color = out_r << 16 | out_g << 8 | out_b << 0;
      }
    }
  else
    ASSERT(false,
           "No implementation for blit_bitmap from src %d channels to dst %d "
           "channels",
           src->channels, dst->channels);
}

// Draw a triangle assuming vertices are in the CCW order.
void draw_triangle(BitMap *dst, Rect *rect_dst, u32 color, Triangle triangle) {
  AABB aabb_tri = triangle_aabb(&triangle);

  AABB aabb_dst;
  u8 *dst_start;
  if (rect_dst) {
    ASSERT((rect_dst->width <= dst->width), "Invalid blit rect_dst");
    ASSERT((rect_dst->hight <= dst->hight), "Invalid blit rect_dst");
    aabb_dst = rect_aabb(rect_dst);
    dst_start = dst->data + (u32)(rect_dst->pos.x - rect_dst->width / 2.0) +
                (u32)(rect_dst->pos.y - rect_dst->hight / 2.0) * dst->width *
                    dst->channels;
  } else {
    aabb_dst = (AABB){{0.0, 0.0}, {dst->width, dst->hight}};
    dst_start = dst->data;
  }

  if (!aabb_intersect(&aabb_tri, &aabb_dst))
    return;

  AABB intersection = aabb_intersection(&aabb_tri, &aabb_dst);

  u32 copy_area_width = aabb_width(&intersection);
  u32 copy_area_hight = aabb_hight(&intersection);
  if (copy_area_width == 0 && copy_area_hight == 0)
    return;

  V2 dst_start_offset = v2_sub(intersection.min, aabb_dst.min);
  dst_start += (u32)dst_start_offset.x * dst->channels +
               (u32)dst_start_offset.y * (dst->width * dst->channels);

  for (u32 y = 0; y < copy_area_hight; y++) {
    u8 *dst_row = dst_start + y * (dst->width * dst->channels);
    for (u32 x = 0; x < copy_area_width; x++) {
      V2 p = {intersection.min.x + (f32)x, intersection.min.y + (f32)y};

      V2 v0v1 = v2_sub(triangle.v1, triangle.v0);
      V2 v0p = v2_sub(p, triangle.v0);

      V2 v1v2 = v2_sub(triangle.v2, triangle.v1);
      V2 v1p = v2_sub(p, triangle.v1);

      V2 v2v0 = v2_sub(triangle.v0, triangle.v2);
      V2 v2p = v2_sub(p, triangle.v2);

      f32 c1 = v2_perp_dot(v0v1, v0p);
      f32 c2 = v2_perp_dot(v1v2, v1p);
      f32 c3 = v2_perp_dot(v2v0, v2p);

      if (c1 <= 0.0 && c2 <= 0.0 && c3 <= 0.0) {
        u32 *dst_color = (u32 *)(dst_row + x * dst->channels);
        *dst_color = color;
      }
    }
  }
}

void draw_char(BitMap *dst, Rect *rect_dst, Font *font, char c, u32 color,
               V2 pos) {
  BitMap font_bm = {
      .data = font->bitmap,
      .width = font->bitmap_width,
      .hight = font->bitmap_hight,
      .channels = 1,
  };
  Rect char_rect = {
      .pos = {(font->char_info[c].x1 + font->char_info[c].x0) / 2.0,
              (font->char_info[c].y1 + font->char_info[c].y0) / 2.0},
      .width = (f32)(font->char_info[c].x1 - font->char_info[c].x0),
      .hight = (f32)(font->char_info[c].y1 - font->char_info[c].y0),
  };
  blit_bitmap(dst, rect_dst, &font_bm, &char_rect, pos, color);
}

void draw_text(BitMap *dst, Rect *rect_dst, Font *font, const char *text,
               u32 color, V2 pos) {
  while (*text) {
    draw_char(dst, rect_dst, font, *text, color, pos);
    pos.x += (f32)(font->char_info[*text].xadvance);
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
  V2 rect_vel;
  V3 camera_pos;
  f32 camera_speed;
  V3 camera_vel;
  bool camera_active;
  f32 camera_sense;
  f32 camera_pitch;
  f32 camera_yaw;

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
      .pos = {(f32)game->surface->w / 2.0, (f32)game->surface->h / 2.0},
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

  game->window =
      SDL_CreateWindow("softy", SDL_WINDOWPOS_UNDEFINED,
                       SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH, WINDOW_HIGHT, 0);
  ASSERT(game->window, "SDL error: %s", SDL_GetError());

  update_window_surface(game);

  game->dt = FRAME_TIME_S;

  game->r = 0.0;

  Rect rect = {
      .pos = {0.0, 0.0},
      .width = 100.0,
      .hight = 150.0,
  };
  game->rect = rect;
  game->rect_vel = (V2){1.2, 2.1};
  game->camera_pos = (V3){0.0, 0.0, -50.0};
  game->camera_speed = 10.0;
  game->camera_vel = (V3){0.0, 0.0, 0.0};
  game->camera_active = false;
  game->camera_sense = 0.1;
  game->camera_pitch = 0.0;
  game->camera_yaw = 0.0;

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
  frame_reset(&game->memory);

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
    case SDL_KEYDOWN:
      switch (sdl_event.key.keysym.sym) {
      case SDLK_w:
        game->camera_vel.z = 1.0;
        break;
      case SDLK_s:
        game->camera_vel.z = -1.0;
        break;
      case SDLK_a:
        game->camera_vel.x = -1.0;
        break;
      case SDLK_d:
        game->camera_vel.x = 1.0;
        break;
      default:
        break;
      }
      break;
    case SDL_KEYUP:
      switch (sdl_event.key.keysym.sym) {
      case SDLK_w:
        game->camera_vel.z = 0.0;
        break;
      case SDLK_s:
        game->camera_vel.z = 0.0;
        break;
      case SDLK_a:
        game->camera_vel.x = 0.0;
        break;
      case SDLK_d:
        game->camera_vel.x = 0.0;
        break;
      default:
        break;
      }
      break;
    case SDL_MOUSEBUTTONDOWN:
      game->camera_active = true;
      break;
    case SDL_MOUSEBUTTONUP:
      game->camera_active = false;
      break;
    case SDL_MOUSEMOTION:
      if (game->camera_active) {
        game->camera_yaw -=
            sdl_event.motion.xrel * game->camera_sense * game->dt;
        game->camera_pitch -=
            sdl_event.motion.yrel * game->camera_sense * game->dt;
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

  game->rect.pos = v2_add(game->rect.pos, game->rect_vel);
  if (game->rect.pos.x < 0 || game->surface->w < game->rect.pos.x) {
    game->rect_vel.x *= -1;
  }
  if (game->rect.pos.y < 0 || game->surface->h < game->rect.pos.y) {
    game->rect_vel.y *= -1;
  }

  SDL_FillRect(game->surface, 0, 0);

  V4 vertices[3] = {
      {0.0, 1.0, 0.0, 1.0},
      {1.0, -1.0, 0.0, 1.0},
      {-1.0, -1.0, 0.0, 1.0},
  };
  Mat4 triangle_transform = mat4_idendity();

  Mat4 camera_rotation_pitch =
      mat4_rotation((V3){-1.0, 0.0, 0.0}, game->camera_pitch);
  Mat4 camera_rotation_yaw =
      mat4_rotation((V3){0.0, 1.0, 0.0}, game->camera_yaw);
  Mat4 camera_rotation = mat4_mul(&camera_rotation_pitch, &camera_rotation_yaw);

  V3 camera_vel = v3_mul(game->camera_vel, game->camera_speed * game->dt);
  V4 camera_vel_v4 = v3_to_v4(camera_vel, 1.0);

  V4 camera_vel_v4_rotated = v4_mul_mat4(camera_vel_v4, &camera_rotation);
  game->camera_pos = v3_add(game->camera_pos, v4_to_v3(camera_vel_v4_rotated));

  Mat4 camera_translation = mat4_idendity();
  mat4_translate(&camera_translation, game->camera_pos);
  Mat4 camera_transform = mat4_mul(&camera_rotation, &camera_translation);

  Mat4 perspective = mat4_perspective(
      70.0 / 180.0 * 3.14, (f32)WINDOW_WIDTH / (f32)WINDOW_HIGHT, 0.1, 1000.0);

  Mat4 model_view = mat4_mul(&camera_transform, &triangle_transform);
  Mat4 mvp = mat4_mul(&perspective, &model_view);

  for (u32 i = 0; i < 3; i++) {
    vertices[i] = mat4_mul_v4(&mvp, vertices[i]);
    vertices[i].x = vertices[i].x / vertices[i].w;
    vertices[i].y = vertices[i].y / vertices[i].w;
    vertices[i].z = vertices[i].z / vertices[i].w;
  }

  Triangle t = {
      .v0 = {(vertices[0].x + 1.0) / 2.0 * WINDOW_WIDTH,
             (vertices[0].y + 1.0) / 2.0 * WINDOW_HIGHT},
      .v1 = {(vertices[1].x + 1.0) / 2.0 * WINDOW_WIDTH,
             (vertices[1].y + 1.0) / 2.0 * WINDOW_HIGHT},
      .v2 = {(vertices[2].x + 1.0) / 2.0 * WINDOW_WIDTH,
             (vertices[2].y + 1.0) / 2.0 * WINDOW_HIGHT},
  };
  draw_triangle(&game->surface_bm, NULL, 0xFF0000FF, t);

  blit_color_rect(&game->surface_bm, &game->surface_rect, 0xFF666666,
                  &game->rect);

  blit_bitmap(&game->surface_bm, NULL, &game->bm, NULL, game->rect.pos,
              0xFF0033EE);

  char *buf = frame_alloc((&game->memory), char[70]);
  snprintf(buf, 70, "FPS: %.02f dt: %.5f", 1.0 / game->dt, game->dt);
  draw_text(&game->surface_bm, &game->surface_rect, &game->font, buf,
            0xFF00FF00, (V2){20.0, 20.0});

  SDL_UpdateWindowSurface(game->window);
}
