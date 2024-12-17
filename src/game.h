#include "SDL2/SDL_surface.h"
#include "defines.h"
#include "log.h"
#include "math.h"
#include "memory.h"
#include "primitives.h"
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

void draw_aabb(BitMap *dst, Rect *rect_dst, AABB *aabb, u32 color) {
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

  if (!aabb_intersect(aabb, &aabb_dst))
    return;

  AABB intersection = aabb_intersection(aabb, &aabb_dst);

  u32 copy_area_width = aabb_width(&intersection);
  u32 copy_area_hight = aabb_hight(&intersection);
  if (copy_area_width == 0 && copy_area_hight == 0)
    return;

  V2 dst_start_offset = v2_sub(intersection.min, aabb_dst.min);
  dst_start += (u32)dst_start_offset.x * dst->channels +
               (u32)dst_start_offset.y * (dst->width * dst->channels);

  for (u32 y = 0; y < copy_area_hight; y++) {
    u8 *dst_row = dst_start + y * (dst->width * dst->channels);
    if (y == 0 || y == copy_area_hight - 1) {
      for (u32 x = 0; x < copy_area_width; x++) {
        u32 *dst_color = (u32 *)(dst_row + x * dst->channels);
        *dst_color = color;
      }
    } else {
      u32 *dst_color = (u32 *)(dst_row);
      *dst_color = color;

      dst_color += copy_area_width;
      *dst_color = color;
    }
  }
}

// Draw a triangle assuming vertices are in the CCW order.
void draw_triangle(f32 *depthbuffer, BitMap *dst, Rect *rect_dst, u32 color,
                   Triangle triangle, CullMode cullmode) {
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

      V2 v0v1 = v2_sub(triangle.v1.xy, triangle.v0.xy);
      V2 v0p = v2_sub(p, triangle.v0.xy);

      V2 v1v2 = v2_sub(triangle.v2.xy, triangle.v1.xy);
      V2 v1p = v2_sub(p, triangle.v1.xy);

      V2 v2v0 = v2_sub(triangle.v0.xy, triangle.v2.xy);
      V2 v2p = v2_sub(p, triangle.v2.xy);

      f32 c1 = v2_perp_dot(v0v1, v0p);
      f32 c2 = v2_perp_dot(v1v2, v1p);
      f32 c3 = v2_perp_dot(v2v0, v2p);

      bool render = false;
      switch (cullmode) {
      case CW:
        render = c1 >= 0.0 && c2 >= 0.0 && c3 >= 0.0;
        break;
      case CCW:
        render = c1 <= 0.0 && c2 <= 0.0 && c3 <= 0.0;
        break;
      case None:
        render = (c1 >= 0.0 && c2 >= 0.0 && c3 >= 0.0) ||
                 (c1 <= 0.0 && c2 <= 0.0 && c3 <= 0.0);
        break;
      }
      if (render) {
        f32 w0 =
            ((triangle.v1.y - triangle.v2.y) * (p.x - triangle.v2.x) +
             (triangle.v2.x - triangle.v1.x) * (p.y - triangle.v2.y)) /
            ((triangle.v1.y - triangle.v2.y) * (triangle.v0.x - triangle.v2.x) +
             (triangle.v2.x - triangle.v1.x) * (triangle.v0.y - triangle.v2.y));
        f32 w1 =
            ((triangle.v2.y - triangle.v0.y) * (p.x - triangle.v2.x) +
             (triangle.v0.x - triangle.v2.x) * (p.y - triangle.v2.y)) /
            ((triangle.v1.y - triangle.v2.y) * (triangle.v0.x - triangle.v2.x) +
             (triangle.v2.x - triangle.v1.x) * (triangle.v0.y - triangle.v2.y));
        f32 w2 = 1.0 - w0 - w1;

        f32 depth = (w0 * 1.0 / triangle.v0.z + w1 * 1.0 / triangle.v1.z +
                     w2 * 1.0 / triangle.v2.z);

        f32 *current_depth = depthbuffer + (u32)(intersection.min.x) +
                             (u32)(intersection.min.y) * dst->width + x +
                             y * dst->width;
        if (*current_depth < depth) {
          *current_depth = depth;
          u32 *dst_color = (u32 *)(dst_row + x * dst->channels);
          *dst_color = color;
        }
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
  V3 position;
  f32 speed;
  V3 velocity;
  f32 mouse_sense;
  f32 pitch;
  f32 yaw;
  bool is_active;
} Camera;

void camera_init(Camera *camera) {
  camera->position = (V3){0.0, 0.0, -50.0};
  camera->speed = 10.0;
  camera->velocity = (V3){0.0, 0.0, 0.0};
  camera->is_active = false;
  camera->mouse_sense = 0.1;
  camera->pitch = 0.0;
  camera->yaw = 0.0;
}

void camera_handle_event(Camera *camera, SDL_Event *sdl_event, f32 dt) {
  switch (sdl_event->type) {
  case SDL_KEYDOWN:
    switch (sdl_event->key.keysym.sym) {
    case SDLK_w:
      camera->velocity.z = 1.0;
      break;
    case SDLK_s:
      camera->velocity.z = -1.0;
      break;
    case SDLK_a:
      camera->velocity.x = -1.0;
      break;
    case SDLK_d:
      camera->velocity.x = 1.0;
      break;
    case SDLK_SPACE:
      camera->velocity.y = -1.0;
      break;
    case SDLK_LCTRL:
      camera->velocity.y = 1.0;
      break;
    default:
      break;
    }
    break;
  case SDL_KEYUP:
    switch (sdl_event->key.keysym.sym) {
    case SDLK_w:
      camera->velocity.z = 0.0;
      break;
    case SDLK_s:
      camera->velocity.z = 0.0;
      break;
    case SDLK_a:
      camera->velocity.x = 0.0;
      break;
    case SDLK_d:
      camera->velocity.x = 0.0;
      break;
    case SDLK_SPACE:
      camera->velocity.y = 0.0;
      break;
    case SDLK_LCTRL:
      camera->velocity.y = 0.0;
      break;
    default:
      break;
    }
    break;
  case SDL_MOUSEBUTTONDOWN:
    camera->is_active = true;
    break;
  case SDL_MOUSEBUTTONUP:
    camera->is_active = false;
    break;
  case SDL_MOUSEMOTION:
    if (camera->is_active) {
      camera->yaw -= sdl_event->motion.xrel * camera->mouse_sense * dt;
      camera->pitch -= sdl_event->motion.yrel * camera->mouse_sense * dt;
    }
    break;
  default:
    break;
  }
}

Mat4 camera_translation(Camera *camera) {
  Mat4 translation = mat4_idendity();
  mat4_translate(&translation, camera->position);
  return translation;
}

Mat4 camera_rotation(Camera *camera) {
  Mat4 camera_rotation_pitch =
      mat4_rotation((V3){-1.0, 0.0, 0.0}, camera->pitch);
  Mat4 camera_rotation_yaw = mat4_rotation((V3){0.0, 1.0, 0.0}, camera->yaw);
  return mat4_mul(&camera_rotation_pitch, &camera_rotation_yaw);
}

void camera_update(Camera *camera, f32 dt) {
  Mat4 rotation = camera_rotation(camera);

  V3 camera_vel = v3_mul(camera->velocity, camera->speed * dt);
  V4 camera_vel_v4 = v3_to_v4(camera_vel, 1.0);

  V4 camera_vel_v4_rotated = v4_mul_mat4(camera_vel_v4, &rotation);
  camera->position = v3_add(camera->position, v4_to_v3(camera_vel_v4_rotated));
}

Mat4 calculate_mvp(Camera *camera, Mat4 *model_transform) {
  Mat4 c_rotation = camera_rotation(camera);
  Mat4 c_translation = camera_translation(camera);
  Mat4 camera_transform = mat4_mul(&c_rotation, &c_translation);

  Mat4 perspective = mat4_perspective(
      70.0 / 180.0 * 3.14, (f32)WINDOW_WIDTH / (f32)WINDOW_HIGHT, 0.1, 1000.0);

  Mat4 model_view = mat4_mul(&camera_transform, model_transform);
  return mat4_mul(&perspective, &model_view);
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

  Camera camera;

  BitMap bm;
  Font font;

  Model model;
  f32 model_rotation;
  Mat4 model_transform;
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

  camera_init(&game->camera);

  game->bm = load_bitmap(&game->memory, "assets/a.png");
  game->font = load_font(&game->memory, "assets/font.ttf", 32.0, 512, 512);
  game->model = load_model(&game->memory, "assets/monkey.obj");
  game->model_rotation = 0.0;
  game->model_transform = mat4_idendity();
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
      case SDLK_q:
        game->model_rotation += game->dt;
        game->model_transform =
            mat4_rotation((V3){0.0, 1.0, 0.0}, game->model_rotation);
        break;
      case SDLK_e:
        game->model_rotation -= game->dt;
        game->model_transform =
            mat4_rotation((V3){0.0, 1.0, 0.0}, game->model_rotation);
        break;
      }
      break;
    default:
      break;
    }
    camera_handle_event(&game->camera, &sdl_event, game->dt);
  }
  camera_update(&game->camera, game->dt);

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

  f32 *depthbuffer = frame_alloc_array((&game->memory), f32,
                                       game->surface->w * game->surface->h);
  memset(depthbuffer, 0, game->surface->w * game->surface->h * 4);

  SDL_FillRect(game->surface, 0, 0);

  Mat4 mvp = calculate_mvp(&game->camera, &game->model_transform);
  for (u32 i = 0; i < game->model.vertices_num; i += 3) {
    Triangle t = vertices_to_triangle(
        &game->model.vertices[i], &game->model.vertices[i + 1],
        &game->model.vertices[i + 2], &mvp, WINDOW_WIDTH, WINDOW_HIGHT);
    u32 color =
        (f32)(0xFFAA33FF) * (f32)(i + 1) / (f32)(game->model.vertices_num + 1);
    draw_triangle(depthbuffer, &game->surface_bm, NULL, color, t, CCW);
  }

  // Draw depth buffer
  // for (u32 x = 0; x < game->surface_rect.width; x++) {
  //   for (u32 y = 0; y < game->surface_rect.width; y++) {
  //     u32* pixel = (u32*)(game->surface->pixels) + x + y * game->surface->w;
  //     f32* depth = depthbuffer + x + y * game->surface->w;
  //     u32 d = (u32)(*depth * 255.0);
  //     *pixel = d << 16 | d << 8 | d << 0;
  //   }
  // }

  blit_color_rect(&game->surface_bm, &game->surface_rect, 0xFF666666,
                  &game->rect);

  blit_bitmap(&game->surface_bm, NULL, &game->bm, NULL, game->rect.pos,
              0xFF0033EE);

  {
    char *buf = frame_alloc((&game->memory), char[70]);
    snprintf(buf, 70, "FPS: %.02f dt: %.5f", 1.0 / game->dt, game->dt);
    draw_text(&game->surface_bm, &game->surface_rect, &game->font, buf,
              0xFF00FF00, (V2){20.0, 20.0});
  }

  char *buf = frame_alloc((&game->memory), char[70]);
  snprintf(buf, 70, "Camera: x: %.02f y: %.02f z: %.02f",
           game->camera.position.x, game->camera.position.y,
           game->camera.position.z);
  draw_text(&game->surface_bm, &game->surface_rect, &game->font, buf,
            0xFF00FF00, (V2){20.0, game->surface_rect.hight - 50.0});

  SDL_UpdateWindowSurface(game->window);
}
